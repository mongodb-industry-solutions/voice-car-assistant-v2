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
from langchain_core.messages import AIMessage, HumanMessage, SystemMessage
from langchain_core.tools import StructuredTool
from langchain_mcp_adapters.client import MultiServerMCPClient
from langchain_ollama import ChatOllama
from langgraph.prebuilt import create_react_agent
from pydantic import BaseModel, Field

app = Flask(__name__)
CORS(app)

# ── Service configuration ─────────────────────────────────────────────────────

OLLAMA_HOST          = os.getenv("OLLAMA_HOST",          "http://localhost:11434")
LLM_MODEL            = os.getenv("LLM_MODEL",            "llama3.2")
EMBEDDING_MODEL      = os.getenv("EMBEDDING_MODEL",      "nub235/voyage-4-nano")
SEARCH_SERVICE_URL   = os.getenv("SEARCH_SERVICE_URL",   "http://localhost:8080")
NAVIGATION_SERVICE_URL = os.getenv("NAVIGATION_SERVICE_URL", "http://localhost:5001")
TELEMETRY_MCP_URL    = os.getenv("TELEMETRY_MCP_URL",    "http://localhost:3001/sse")

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

3. **Navigation** (navigate_to) — Find and route to nearby places: mechanics, gas
   stations, pharmacies, hospitals, etc.

When the user reports a warning or fault:
  • Step 1: check the relevant telemetry (get_anomalies or check_system_status)
  • Step 2: search the car manual for repair / safety guidance
  • Step 3: offer to navigate to the nearest relevant service if appropriate

{location_context}

Be concise and safety-focused. For critical issues (overheating, brake failure, etc.)
lead with the safety action before anything else.\
"""


def _build_system_prompt(lat: Optional[float], lon: Optional[float]) -> str:
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
    return _SYSTEM_PROMPT.format(location_context=loc)


# ── Tool helpers ──────────────────────────────────────────────────────────────

def _search_manual_impl(query: str) -> str:
    """Embed query via Ollama and call the search-service."""
    try:
        oc = ollama_client.Client(host=OLLAMA_HOST)
        embedding = oc.embeddings(model=EMBEDDING_MODEL, prompt=query)["embedding"]
        resp = requests.post(
            f"{SEARCH_SERVICE_URL}/search",
            json={"embedding": embedding, "limit": 3},
            timeout=10,
        )
        if not resp.ok:
            return "Car manual search service is unavailable."
        results = resp.json().get("results", [])
        if not results:
            return "No relevant information found in the car manual."
        return "\n\n---\n\n".join(r["text"] for r in results)
    except Exception as e:
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
) -> dict:
    """Build and invoke the LangGraph react agent."""
    llm = ChatOllama(model=LLM_MODEL, base_url=OLLAMA_HOST, temperature=0)

    agent = create_react_agent(llm, all_tools)

    input_messages = (
        [SystemMessage(content=_build_system_prompt(lat, lon))]
        + history
        + [HumanMessage(content=message)]
    )

    result = await agent.ainvoke(
        {"messages": input_messages},
        config={"recursion_limit": 25},
    )

    answer_msg = result["messages"][-1]
    answer = answer_msg.content if hasattr(answer_msg, "content") else ""

    tools_used = [
        tc["name"]
        for msg in result["messages"]
        if isinstance(msg, AIMessage) and getattr(msg, "tool_calls", None)
        for tc in msg.tool_calls
    ]

    return {"output": answer, "tools_used": tools_used}


async def run_agent(
    message: str,
    conversation_id: str,
    lat: Optional[float],
    lon: Optional[float],
) -> dict:
    """
    Full agent pipeline:
      1. Build navigate_to tool (closure captures lat/lon + nav result store)
      2. Open MCP connection and load telemetry tools
      3. Run agent inside the MCP context so the connection stays alive
      4. Return answer + navigation data + tools used
    """
    navigation_result: list = []  # populated by navigate_to closure

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

    manual_tool = StructuredTool.from_function(
        func=_search_manual_impl,
        name="search_car_manual",
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

    history = _histories.get(conversation_id, [])

    # ── Try MCP connection — fall back gracefully if unavailable ─────────────
    try:
        async with MultiServerMCPClient({
            "telemetry": {"url": TELEMETRY_MCP_URL, "transport": "sse"},
        }) as mcp:
            mcp_tools = mcp.get_tools()
            print(f"[agent] Loaded {len(mcp_tools)} MCP telemetry tools")
            all_tools = [manual_tool, navigate_tool] + mcp_tools
            result = await _run_with_tools(all_tools, message, history, lat, lon)
    except Exception as e:
        print(f"[agent] MCP unavailable ({e}), running without telemetry tools")
        result = await _run_with_tools([manual_tool, navigate_tool], message, history, lat, lon)

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

    if not message:
        return jsonify({"error": "message is required"}), 400

    try:
        result = asyncio.run(run_agent(message, conversation_id, lat, lon))
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
    print(f"   Telemetry MCP: {TELEMETRY_MCP_URL}")
    app.run(host="0.0.0.0", port=port, debug=False)
