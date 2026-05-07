"""
LangChain Agent Service — Unified car assistant
Combines: car manual RAG, vehicle telemetry (via MongoDB MCP), navigation
"""

import asyncio
import json
import os
import time
import uuid
from typing import Optional

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
from sentence_transformers import SentenceTransformer
from flask import Flask, jsonify, request
from flask_cors import CORS
from langchain_core.messages import AIMessage, HumanMessage, SystemMessage, ToolMessage
from langchain_core.tools import StructuredTool
from langchain_ollama import ChatOllama
from pydantic import BaseModel, Field

app = Flask(__name__)
CORS(app)

# ── Service configuration ─────────────────────────────────────────────────────

OLLAMA_HOST               = os.getenv("OLLAMA_HOST",               "http://localhost:11434")
LLM_MODEL                 = os.getenv("LLM_MODEL",                 "llama3.1:8b")
EMBEDDING_MODEL           = os.getenv("EMBEDDING_MODEL",           "voyageai/voyage-4-nano")
SEARCH_SERVICE_URL        = os.getenv("SEARCH_SERVICE_URL",        "http://localhost:8080")
MONGODB_SEARCH_SERVICE_URL = os.getenv("MONGODB_SEARCH_SERVICE_URL", "http://localhost:8085")
NAVIGATION_SERVICE_URL    = os.getenv("NAVIGATION_SERVICE_URL",    "http://localhost:5001")
TELEMETRY_SERVICE_URL     = os.getenv("TELEMETRY_MCP_URL",         "http://localhost:3001")

# Load embedding model once at startup
print(f"Loading embedding model: {EMBEDDING_MODEL}", flush=True)
_embed_model = SentenceTransformer(EMBEDDING_MODEL)
print("Embedding model ready", flush=True)


# Shared HTTP session with connection pooling (reused across all tool calls)
_http = requests.Session()
_http.mount("http://", HTTPAdapter(max_retries=Retry(total=1, backoff_factor=0.3)))
_http.mount("https://", HTTPAdapter(max_retries=Retry(total=1, backoff_factor=0.3)))

# ── Conversation memory (per session) ────────────────────────────────────────
# { conversation_id: [HumanMessage, AIMessage, ...] }
_histories: dict = {}

# ── System prompt ─────────────────────────────────────────────────────────────

_SYSTEM_PROMPT = """\
You are a unified intelligent car assistant with access to three capabilities:

1. **Car Manual** (search_car_manual) — Answer questions about vehicle maintenance,
   repairs, warning lights, specifications, and procedures.

2. **Vehicle Telemetry** (MCP tools) — Check real-time sensor data: engine temperature,
   oil pressure, battery voltage, fuel level, tire pressure, transmission, brakes.
   Use get_anomalies first when the user reports a problem, then check_system_status
   for the relevant system.
   AVAILABILITY: {telemetry_availability}.

3. **Navigation** (navigate_to) — Find and route to nearby places: mechanics, gas
   stations, pharmacies, hospitals, etc.

MULTI-TOOL USE — use as many tools as the user's request requires:

• If the user asks for a single thing, use one tool. If the request requires multiple
  steps or actions, use multiple tools in sequence — one per loop iteration.

• When the user's request involves a condition (e.g. "check X and if critical do Y"),
  call the first tool, read the result, then call the next tool only if the condition
  is met. Do NOT skip steps and do NOT ask the user to confirm between steps.

• When the user explicitly asks for multiple actions in one message (e.g. "check engine
  AND navigate to a mechanic"), call all required tools in the correct order.

RULES — follow these exactly:

• When search_car_manual returns text content, ALWAYS present that information to the
  user. Summarise it clearly. Never say you could not retrieve it.

• Only say you were unable to retrieve data if the tool explicitly returns an error
  message or empty content.

• If the user asks about live vehicle sensor data and you are in offline mode, tell
  them that telemetry is only available in online mode.

• Never fabricate or guess sensor readings. Never invent values for telemetry tools.

• Never output raw JSON or tool call objects. Always respond in plain human-readable
  language after using a tool.

When the user asks for a general car status or overview:
  → call get_anomalies to get all current warnings and issues across every system.

When the user asks to check a specific system (engine, battery, fuel, tires, transmission, brakes):
  → call check_system_status only. Do NOT search the car manual unless the user also
     asks how to fix it, what it means, or what to do about it.

When the user asks how to fix, repair, or understand a warning or fault:
  → search the car manual. Only also call check_system_status if you need the current
     sensor reading to give a useful answer.

When the user asks to check a system AND navigate in the same message:
  • Step 1: call check_system_status
  • Step 2: call navigate_to immediately — do NOT ask for confirmation first

When the user responds with a short confirmation ("yes", "sure", "ok", "please", "go ahead",
"yes please", etc.) to a navigation offer you just made, call navigate_to immediately.
Do NOT search the manual again, do NOT repeat safety advice, do NOT ask again — just navigate.

{location_context}

Be concise and safety-focused. For critical issues (overheating, brake failure, etc.)
lead with the safety action before anything else.\
"""


def _build_system_prompt(lat: Optional[float], lon: Optional[float], network_mode: str = "offline") -> str:
    if lat is not None and lon is not None:
        loc = (
            "The user's GPS location is known. "
            "You can call navigate_to with just the destination name — coordinates are handled automatically. "
            "Never mention raw coordinates or numbers to the user."
        )
    else:
        loc = (
            "The user's GPS location is not yet available. "
            "If navigation is requested, ask them to allow location access in the browser."
        )
    telemetry = (
        "Available — use the MCP tools to answer telemetry questions"
        if network_mode == "online"
        else "NOT available in offline mode — tell the user to switch to online mode for live sensor data"
    )
    return _SYSTEM_PROMPT.format(location_context=loc, telemetry_availability=telemetry)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _is_raw_tool_call(text: str) -> bool:
    """Return True if text is a leaked tool-call JSON (model forgot to invoke the tool)."""
    stripped = text.strip()
    if not stripped.startswith('{'):
        return False
    try:
        decoder = json.JSONDecoder()
        data, _ = decoder.raw_decode(stripped)
        return isinstance(data, dict) and "name" in data and (
            "parameters" in data or "arguments" in data
        )
    except Exception:
        return False


# ── Tool helpers ──────────────────────────────────────────────────────────────

def _search_manual_impl(query: str, search_url: str = None) -> str:
    """Embed query via Ollama and call the appropriate search-service."""
    url = search_url or SEARCH_SERVICE_URL
    print(f"[search] query='{query[:60]}' url={url}", flush=True)
    try:
        t0 = time.time()
        embedding = _embed_model.encode(query).tolist()
        print(f"[timing] embedding: {time.time()-t0:.2f}s  dims={len(embedding)}", flush=True)
        t1 = time.time()
        resp = _http.post(
            f"{url}/search",
            json={"embedding": embedding, "limit": 3},
            timeout=30,
        )
        print(f"[timing] search HTTP call: {time.time()-t1:.2f}s  status={resp.status_code}", flush=True)
        if not resp.ok:
            print(f"[search] error body: {resp.text[:300]}", flush=True)
            return f"Car manual search service error (HTTP {resp.status_code}): {resp.text[:200]}"
        results = resp.json().get("results", [])
        print(f"[search] results count={len(results)}", flush=True)
        if not results:
            return "No relevant information found in the car manual."
        return "\n\n---\n\n".join(r["text"] for r in results)
    except Exception as e:
        print(f"[search] exception: {e}", flush=True)
        return f"Manual search error: {e}"


# ── Pydantic schemas for tool arguments ──────────────────────────────────────

class ManualSearchInput(BaseModel):
    query: str = Field(description="What to look up in the car manual")


class NavigateInput(BaseModel):
    destination: str = Field(
        description="What to navigate to, e.g. 'nearest mechanic', 'gas station', \"McDonald's\""
    )


# ── Core async agent runner ───────────────────────────────────────────────────

async def _run_with_tools(
    all_tools: list,
    message: str,
    history: list,
    lat: Optional[float],
    lon: Optional[float],
    network_mode: str = "offline",
) -> dict:
    """
    Custom ReAct loop that handles both native structured tool calls and models
    (like llama3.1:8b via Ollama) that emit raw JSON tool calls in message content.
    """
    llm = ChatOllama(model=LLM_MODEL, base_url=OLLAMA_HOST, temperature=0, timeout=120, keep_alive="10m")
    llm_with_tools = llm.bind_tools(all_tools)
    tool_map = {t.name: t for t in all_tools}

    messages = (
        [SystemMessage(content=_build_system_prompt(lat, lon, network_mode))]
        + history
        + [HumanMessage(content=message)]
    )

    tools_used: list[str] = []
    answer = ""

    for iteration in range(5):
        t0 = time.time()
        response = await llm_with_tools.ainvoke(messages)
        print(f"[timing] LLM call #{iteration+1}: {time.time()-t0:.2f}s", flush=True)
        content = response.content if isinstance(response.content, str) else ""

        # Case 1: model used structured tool_calls (native support)
        if getattr(response, "tool_calls", None):
            messages.append(response)
            all_unavailable = True
            for tc in response.tool_calls:
                tool = tool_map.get(tc["name"])
                if tool:
                    all_unavailable = False
                    tools_used.append(tc["name"])
                    try:
                        t1 = time.time()
                        result = await tool.ainvoke(tc["args"])
                        print(f"[timing] tool '{tc['name']}': {time.time()-t1:.2f}s", flush=True)
                    except Exception as e:
                        result = f"Error: {e}"
                else:
                    print(f"[agent] tool '{tc['name']}' not available in {network_mode} mode", flush=True)
                    result = (
                        f"The tool '{tc['name']}' is not available in {network_mode} mode. "
                        f"Tell the user this feature requires switching to online mode."
                    )
                messages.append(ToolMessage(content=str(result), tool_call_id=tc["id"]))
            if all_unavailable:
                answer = (
                    "Live vehicle telemetry is not available in offline mode. "
                    "Please switch to online mode to access real-time sensor data."
                )
                break
            continue

        # Case 2: model emitted a raw JSON tool call in content
        if _is_raw_tool_call(content):
            tool_call_data, _ = json.JSONDecoder().raw_decode(content.strip())
            tool_name = tool_call_data.get("name")
            tool_args = tool_call_data.get("parameters") or tool_call_data.get("arguments") or {}
            tool = tool_map.get(tool_name)
            if tool:
                tools_used.append(tool_name)
                print(f"[agent] raw tool call detected: {tool_name}({tool_args})", flush=True)
                try:
                    result = await tool.ainvoke(tool_args)
                except Exception as e:
                    result = f"Error calling {tool_name}: {e}"
                messages.append(AIMessage(content=content))
                messages.append(HumanMessage(
                    content=f"Tool result for {tool_name}: {result}\n\nNow give a concise, helpful response based on this data."
                ))
                continue
            # Tool not available in current mode — respond directly without re-invoking LLM
            print(f"[agent] tool '{tool_name}' not available in {network_mode} mode", flush=True)
            answer = (
                "Live vehicle telemetry is not available in offline mode. "
                "Please switch to online mode to access real-time sensor data."
            )
            break

        # Case 3: plain response — done
        answer = content
        break

    return {"output": answer, "tools_used": tools_used}


async def run_agent(
    message: str,
    conversation_id: str,
    lat: Optional[float],
    lon: Optional[float],
    network_mode: str = "offline",
) -> dict:
    """
    Full agent pipeline:
      1. Build navigate_to tool (closure captures lat/lon + nav result store)
      2. Open MCP connection and load telemetry tools
      3. Run agent inside the MCP context so the connection stays alive
      4. Return answer + navigation data + tools used
    """
    navigation_result: list = []  # populated by navigate_to closure
    is_online = network_mode == "online"
    active_search_url = MONGODB_SEARCH_SERVICE_URL if is_online else SEARCH_SERVICE_URL

    # ── Custom tools ──────────────────────────────────────────────────────────

    def _navigate(destination: str) -> str:
        try:
            resp = _http.post(
                f"{NAVIGATION_SERVICE_URL}/navigate",
                json={"query": destination, "lat": lat, "lon": lon},
                timeout=35,
            )
            if resp.ok:
                data = resp.json()
                navigation_result.append(data)
                dest, route = data["destination"], data["route"]
                return (
                    f"Route found to {dest['name']}: "
                    f"{route['distance_text']} away, ~{route['duration_text']} by car. "
                    "Route is now displayed on the map."
                )
            return f"Navigation failed: {resp.json().get('error', 'unknown error')}"
        except Exception as e:
            return f"Navigation service unavailable: {e}"

    tool_name = "search_car_manual_atlas" if is_online else "search_car_manual_objectbox"

    def _search_manual_for_mode(query: str) -> str:
        print(f"[agent] {tool_name} called, url={active_search_url}", flush=True)
        return _search_manual_impl(query, active_search_url)

    manual_tool = StructuredTool.from_function(
        func=_search_manual_for_mode,
        name=tool_name,
        description=(
            "Search the car manual for maintenance procedures, repair guides, "
            "warning light explanations, and vehicle specifications."
        ),
        args_schema=ManualSearchInput,
    )
    navigate_tool = StructuredTool.from_function(
        func=_navigate,
        name="navigate_to",
        description=(
            "Navigate to a nearby destination. Use when the user wants directions "
            "to a mechanic, gas station, hospital, pharmacy, or any other place."
        ),
        args_schema=NavigateInput,
    )

    # ── Telemetry tools (direct REST calls to MCP server) ────────────────────

    def _call_telemetry_tool(name: str, args: dict = {}) -> str:
        try:
            resp = _http.post(
                f"{TELEMETRY_SERVICE_URL}/tools/{name}",
                json=args,
                timeout=10,
            )
            return resp.text if resp.ok else f"Telemetry error: {resp.status_code}"
        except Exception as e:
            return f"Telemetry service unavailable: {e}"

    telemetry_tools = [
        StructuredTool.from_function(
            func=lambda: _call_telemetry_tool("get_latest_telemetry"),
            name="get_latest_telemetry",
            description="Get the most recent snapshot of all vehicle sensor data.",
        ),
        StructuredTool.from_function(
            func=lambda system: _call_telemetry_tool("check_system_status", {"system": system}),
            name="check_system_status",
            description="Check the status of a specific vehicle system. Valid systems: engine, battery, fuel, tires, transmission, brakes.",
            args_schema=type("CheckSystemInput", (BaseModel,), {
                "system": Field(description="System to check: engine, battery, fuel, tires, transmission, or brakes"),
                "__annotations__": {"system": str},
            }),
        ),
        StructuredTool.from_function(
            func=lambda: _call_telemetry_tool("get_tire_pressure"),
            name="get_tire_pressure",
            description="Get tire pressure readings for all four tires.",
        ),
        StructuredTool.from_function(
            func=lambda: _call_telemetry_tool("get_anomalies"),
            name="get_anomalies",
            description="Get all current warnings and critical issues across all vehicle systems.",
        ),
    ]

    history = _histories.get(conversation_id, [])
    all_tools = [manual_tool, navigate_tool] + (telemetry_tools if is_online else [])
    print(f"[agent] network_mode={network_mode} tools={[t.name for t in all_tools]}", flush=True)
    result = await _run_with_tools(all_tools, message, history, lat, lon, network_mode)

    answer = result.get("output", "I couldn't generate a response.")
    tools_used = result.get("tools_used", [])

    # Update conversation memory (cap at 20 turns)
    updated = history + [HumanMessage(content=message), AIMessage(content=answer)]
    _histories[conversation_id] = updated[-20:]

    return {
        "answer": answer,
        "tools_used": tools_used,
        "navigation": navigation_result[0] if navigation_result else None,
        "conversation_id": conversation_id,
    }


# ── Flask endpoints ───────────────────────────────────────────────────────────

@app.route("/health")
def health():
    return jsonify({"status": "ok", "service": "langchain-agent-service"})


@app.route("/agent/chat", methods=["POST"])
def agent_chat():
    data = request.json or {}
    message         = data.get("message", "").strip()
    conversation_id = data.get("conversation_id") or str(uuid.uuid4())
    lat             = data.get("lat")
    lon             = data.get("lon")
    network_mode    = data.get("network_mode", "offline")

    if not message:
        return jsonify({"error": "message is required"}), 400

    try:
        result = asyncio.run(run_agent(message, conversation_id, lat, lon, network_mode))
        return jsonify(result)
    except Exception as e:
        print(f"[agent] Error: {e}")
        return jsonify({
            "error": str(e),
            "answer": "I encountered an error processing your request. Please try again.",
            "tools_used": [],
            "navigation": None,
            "conversation_id": conversation_id,
        }), 500


if __name__ == "__main__":
    port = int(os.getenv("PORT", 5002))
    print(f"🤖 LangChain Agent Service starting on port {port}")
    print(f"   LLM:              {LLM_MODEL} @ {OLLAMA_HOST}")
    print(f"   Embeddings:       {EMBEDDING_MODEL} (local)")
    print(f"   Search (offline): {SEARCH_SERVICE_URL}")
    print(f"   Search (online):  {MONGODB_SEARCH_SERVICE_URL}")
    print(f"   Navigation:       {NAVIGATION_SERVICE_URL}")
    print(f"   Telemetry:        {TELEMETRY_SERVICE_URL}")
    app.run(host="0.0.0.0", port=port, debug=False)
