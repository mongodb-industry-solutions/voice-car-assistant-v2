"""
LangChain Agent Service — Unified car assistant
Combines: car manual RAG, vehicle telemetry (via MongoDB MCP), navigation
"""

import asyncio
import json
import os
import uuid
from typing import Optional


import ollama as ollama_client
import requests
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
EMBEDDING_MODEL           = os.getenv("EMBEDDING_MODEL",           "nub235/voyage-4-nano")
SEARCH_SERVICE_URL        = os.getenv("SEARCH_SERVICE_URL",        "http://localhost:8080")
MONGODB_SEARCH_SERVICE_URL = os.getenv("MONGODB_SEARCH_SERVICE_URL", "http://localhost:8085")
NAVIGATION_SERVICE_URL    = os.getenv("NAVIGATION_SERVICE_URL",    "http://localhost:5001")
TELEMETRY_SERVICE_URL     = os.getenv("TELEMETRY_MCP_URL",         "http://localhost:3001")

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

RULES — follow these exactly:

• When search_car_manual returns text content, ALWAYS present that information to the
  user. Summarise it clearly. Never say you could not retrieve it.

• Only say you were unable to retrieve data if the tool explicitly returns an error
  message or empty content.

• If the user asks about live vehicle sensor data and you are in offline mode, tell
  them that telemetry is only available in online mode.

• Never fabricate or guess sensor readings. Never invent values for telemetry tools.

• Never output raw coordinates to the user. If location is known, refer to it only as
  "your current location". Never say "latitude=..." or "longitude=..." to the user.

• Never output raw JSON or tool call objects. Always respond in plain human-readable
  language after using a tool.

When the user asks for a general car status or overview:
  → call get_anomalies to get all current warnings and issues across every system.

When the user reports a warning or fault for a specific system:
  • Step 1: call check_system_status for that system (engine, battery, fuel, tires, transmission, or brakes)
  • Step 2: search the car manual for repair / safety guidance
  • Step 3: offer to navigate to the nearest relevant service if appropriate

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
            f"The user's GPS location is known. "
            f"When calling navigate_to, use latitude={lat:.5f} and longitude={lon:.5f}. "
            f"Never mention raw coordinates or numbers to the user — "
            f"refer to it as 'your current location' instead."
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
    try:
        data = json.loads(text.strip())
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
        oc = ollama_client.Client(host=OLLAMA_HOST)
        embedding = oc.embeddings(model=EMBEDDING_MODEL, prompt=query)["embedding"]
        print(f"[search] embedding dims={len(embedding)}", flush=True)
        resp = requests.post(
            f"{url}/search",
            json={"embedding": embedding, "limit": 3},
            timeout=10,
        )
        print(f"[search] response status={resp.status_code}", flush=True)
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
    latitude: float = Field(description="User's current latitude (from system context)")
    longitude: float = Field(description="User's current longitude (from system context)")


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
    llm = ChatOllama(model=LLM_MODEL, base_url=OLLAMA_HOST, temperature=0)
    llm_with_tools = llm.bind_tools(all_tools)
    tool_map = {t.name: t for t in all_tools}

    messages = (
        [SystemMessage(content=_build_system_prompt(lat, lon, network_mode))]
        + history
        + [HumanMessage(content=message)]
    )

    tools_used: list[str] = []
    answer = ""

    for _ in range(10):
        response = await llm_with_tools.ainvoke(messages)
        content = response.content if isinstance(response.content, str) else ""

        # Case 1: model used structured tool_calls (native support)
        if getattr(response, "tool_calls", None):
            messages.append(response)
            for tc in response.tool_calls:
                tool = tool_map.get(tc["name"])
                if tool:
                    tools_used.append(tc["name"])
                    try:
                        result = await tool.ainvoke(tc["args"])
                    except Exception as e:
                        result = f"Error: {e}"
                else:
                    print(f"[agent] tool '{tc['name']}' not available in {network_mode} mode", flush=True)
                    result = (
                        f"The tool '{tc['name']}' is not available in {network_mode} mode. "
                        f"Tell the user this feature requires switching to online mode."
                    )
                messages.append(ToolMessage(content=str(result), tool_call_id=tc["id"]))
            continue

        # Case 2: model emitted a raw JSON tool call in content
        if _is_raw_tool_call(content):
            tool_call_data = json.loads(content.strip())
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

    def _navigate(destination: str, latitude: float, longitude: float) -> str:
        try:
            resp = requests.post(
                f"{NAVIGATION_SERVICE_URL}/navigate",
                json={"query": destination, "lat": latitude, "lon": longitude},
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
            resp = requests.post(
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
    print(f"   LLM:         {LLM_MODEL} @ {OLLAMA_HOST}")
    print(f"   Search:      {SEARCH_SERVICE_URL}")
    print(f"   Navigation:  {NAVIGATION_SERVICE_URL}")
    print(f"   Telemetry:     {TELEMETRY_SERVICE_URL}")
    app.run(host="0.0.0.0", port=port, debug=False)
