"""
LangChain Agent Service — Unified car assistant
Combines: car manual RAG, vehicle telemetry (via VSS MCP), navigation.

Uses a LangGraph ReAct agent (create_react_agent) with:
  - ChatOllama as the LLM
  - MemorySaver checkpointer for per-conversation history
  - StructuredTool definitions for all capabilities
"""

import os
import re
import time
import uuid
from typing import Any, Optional

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
from sentence_transformers import SentenceTransformer
from flask import Flask, jsonify, request
from flask_cors import CORS
from langchain_ollama import ChatOllama
from langchain_core.callbacks import BaseCallbackHandler
from langchain_core.messages import HumanMessage, SystemMessage, ToolMessage
from langchain_core.tools import StructuredTool
from langgraph.prebuilt import create_react_agent
from langgraph.checkpoint.memory import MemorySaver
from pydantic import BaseModel, Field

app = Flask(__name__)
CORS(app)

# ── Service configuration ─────────────────────────────────────────────────────

OLLAMA_HOST                = os.getenv("OLLAMA_HOST",                "http://localhost:11434")
LLM_MODEL                  = os.getenv("LLM_MODEL",                  "qwen3:4b")
EMBEDDING_MODEL            = os.getenv("EMBEDDING_MODEL",            "voyageai/voyage-4-nano")
SEARCH_SERVICE_URL         = os.getenv("SEARCH_SERVICE_URL",         "http://localhost:8080")
MONGODB_SEARCH_SERVICE_URL = os.getenv("MONGODB_SEARCH_SERVICE_URL", "http://localhost:8085")
NAVIGATION_SERVICE_URL     = os.getenv("NAVIGATION_SERVICE_URL",     "http://localhost:5001")
TELEMETRY_SERVICE_URL      = os.getenv("VSS_TELEMETRY_MCP_URL",      "http://localhost:3002")

# ── Module-level singletons (shared across all Gunicorn threads) ──────────────

# voyage-4-nano ships custom model code (Qwen3 bidirectional) and MUST be loaded
# with trust_remote_code=True; without it transformers emits a wrong 2048-d
# vector. It is a Matryoshka model — truncate_dim=1024 selects its native 1024-d
# head (identical to the Voyage API's output_dimension=1024). Requires
# transformers==4.57.1 (see requirements.txt). This MUST match how
# load_documents.py embeds stored chunks (same model, truncate_dim, normalisation)
# or query vectors won't align with the indexed vectors.
EMBED_DIM = int(os.getenv("EMBED_DIM", "1024"))
print(f"Loading embedding model: {EMBEDDING_MODEL} @ {EMBED_DIM} dims", flush=True)
_embed_model = SentenceTransformer(EMBEDDING_MODEL, trust_remote_code=True, truncate_dim=EMBED_DIM)
print("Embedding model ready", flush=True)

# LLM — created once; ChatOllama is stateless so it's safe to share across threads
_llm = ChatOllama(
    model=LLM_MODEL,
    base_url=OLLAMA_HOST,
    temperature=0,
    keep_alive="10m",
    num_ctx=4096,
)

# Conversation memory — keyed by conversation_id (thread_id in LangGraph terms)
_checkpointer = MemorySaver()

# Shared HTTP session with connection pooling
_http = requests.Session()
_http.mount("http://",  HTTPAdapter(max_retries=Retry(total=1, backoff_factor=0.3)))
_http.mount("https://", HTTPAdapter(max_retries=Retry(total=1, backoff_factor=0.3)))

# ── Helpers ───────────────────────────────────────────────────────────────────

def _strip_think_tags(text: str) -> str:
    """Remove <think>...</think> blocks that qwen3 embeds in content."""
    return re.sub(r"<think>.*?</think>", "", text, flags=re.DOTALL).strip()


class _TimingCallback(BaseCallbackHandler):
    """Logs the start/end time of every LLM call and tool call inside the agent loop."""

    def __init__(self) -> None:
        super().__init__()
        self._llm_start: float = 0.0
        self._tool_start: float = 0.0
        self._llm_call: int = 0

    def on_llm_start(self, serialized: dict, prompts: list, **kwargs: Any) -> None:
        self._llm_call += 1
        self._llm_start = time.time()
        print(f"[cb] LLM call #{self._llm_call} start", flush=True)

    def on_llm_end(self, response: Any, **kwargs: Any) -> None:
        print(f"[cb] LLM call #{self._llm_call} end: {time.time() - self._llm_start:.2f}s", flush=True)

    def on_llm_error(self, error: Exception, **kwargs: Any) -> None:
        print(f"[cb] LLM call #{self._llm_call} error after {time.time() - self._llm_start:.2f}s: {error}", flush=True)

    def on_tool_start(self, serialized: dict, input_str: str, **kwargs: Any) -> None:
        self._tool_start = time.time()
        name = serialized.get("name", "?")
        print(f"[cb] tool '{name}' start  input={input_str[:120]}", flush=True)

    def on_tool_end(self, output: Any, **kwargs: Any) -> None:
        print(f"[cb] tool end: {time.time() - self._tool_start:.2f}s  output={str(output)[:120]}", flush=True)

    def on_tool_error(self, error: Exception, **kwargs: Any) -> None:
        print(f"[cb] tool error after {time.time() - self._tool_start:.2f}s: {error}", flush=True)

# ── System prompt ─────────────────────────────────────────────────────────────

_SYSTEM_PROMPT = """\
You are a unified intelligent car assistant with access to three capabilities:

1. **Car Manual** ({search_tool_name}) — Answer questions about vehicle maintenance,
   repairs, warning lights, specifications, and procedures.

2. **Vehicle Telemetry** (MCP tools) — Check real-time VSS sensor data across six domains:
   powertrain (speed, RPM, fuel, coolant, gear), battery (SOC, range, charging, voltage,
   health), chassis (tire pressures, ABS, ESC, brake fluid), cabin (doors, HVAC, windows),
   location (GPS coordinates, heading), and ADAS (cruise control, lane keep, collision warning).
   Use get_vehicle_events first when the user reports a problem, then the relevant domain
   tool (e.g. get_chassis_status for tire/brake issues, get_powertrain_status for engine).
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

• When {search_tool_name} returns text content, ALWAYS present that information to the
  user. Summarise it clearly. Never say you could not retrieve it.

• Only say you were unable to retrieve data if the tool explicitly returns an error
  message or empty content.

• If the user asks about live vehicle sensor data and you are in offline mode, tell
  them that telemetry is only available in online mode.

• Never fabricate or guess sensor readings. Never invent values for telemetry tools.

• Never output raw JSON or tool call objects. Always respond in plain human-readable
  language after using a tool.

• Never mention page numbers from the car manual in your answers.

TOOL ROUTING — decide by the user's INTENT, not by keywords alone. The key distinction:
PROCEDURES & ADVICE come from the car manual; CURRENT READINGS come from telemetry tools.

1) PROCEDURES & ADVICE — "how do I…", "what to do…", "how do I fix / replace / change /
   check…", "what does this warning mean", or any maintenance, repair, or troubleshooting
   question → you MUST call {search_tool_name}. This applies EVEN in online mode and EVEN
   for tires, brakes, battery, or engine — the telemetry tools contain NO procedures.
   Never answer these from your own knowledge; always search the manual first.
   Examples:
     • "what to do in case I have a flat tire"  → {search_tool_name}
     • "how do I check the brake fluid"         → {search_tool_name}
     • "what does the coolant warning mean"     → {search_tool_name}

2) CURRENT LIVE READINGS — ONLY when the user asks for the vehicle's current / real-time
   sensor values (e.g. "what's my tire pressure right now", "is the battery charging",
   "what's my current speed") call the matching telemetry tool:
     → engine / speed / gear / RPM / coolant     → get_powertrain_status
     → fuel / petrol / gas / fuel level          → get_fuel_status
     → battery / charge / SOC / electric range   → get_battery_status
     → tires / brakes / ABS / ESC               → get_chassis_status
     → cabin / doors / HVAC / windows           → get_cabin_status
     → location / GPS / heading                 → get_location
     → ADAS / cruise control / lane keep        → get_adas_status
     → full current snapshot or overview        → get_vehicle_status, then get_vehicle_events

3) BOTH — if the user wants a current reading AND what to do about it, call the telemetry
   tool for the reading and {search_tool_name} for the procedure.

When the user asks to check a system AND navigate in the same message:
  • Step 1: call the relevant domain tool (e.g. get_chassis_status for tires/brakes)
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
    is_online = network_mode == "online"
    telemetry = (
        "Available — use the MCP tools to answer telemetry questions"
        if is_online
        else "NOT available in offline mode — tell the user to switch to online mode for live sensor data"
    )
    search_tool = "search_car_manual_atlas" if is_online else "search_car_manual_objectbox"
    return _SYSTEM_PROMPT.format(
        location_context=loc,
        telemetry_availability=telemetry,
        search_tool_name=search_tool,
    )


# ── Tool helpers ──────────────────────────────────────────────────────────────

def _search_manual_impl(query: str, search_url: str = None) -> str:
    url = search_url or SEARCH_SERVICE_URL
    print(f"[search] query='{query[:60]}' url={url}", flush=True)
    try:
        t0 = time.time()
        embedding = _embed_model.encode(query, prompt_name="query", normalize_embeddings=True).tolist()
        print(f"[timing] embedding: {time.time()-t0:.2f}s  dims={len(embedding)}", flush=True)
        t1 = time.time()
        resp = _http.post(f"{url}/search", json={"embedding": embedding, "limit": 3}, timeout=30)
        print(f"[timing] search HTTP: {time.time()-t1:.2f}s  status={resp.status_code}", flush=True)
        if not resp.ok:
            return f"Car manual search service error (HTTP {resp.status_code}): {resp.text[:200]}"
        results = resp.json().get("results", [])
        print(f"[search] results count={len(results)}", flush=True)
        if not results:
            return "No relevant information found in the car manual."
        return "\n\n---\n\n".join(r["text"] for r in results)
    except Exception as e:
        print(f"[search] exception: {e}", flush=True)
        return f"Manual search error: {e}"


# ── Pydantic schemas ──────────────────────────────────────────────────────────

class ManualSearchInput(BaseModel):
    query: str = Field(description="What to look up in the car manual")

class NavigateInput(BaseModel):
    destination: str = Field(
        description="What to navigate to, e.g. 'nearest mechanic', 'gas station', \"McDonald's\""
    )

class VehicleEventsInput(BaseModel):
    minutes:  int = Field(default=30, description="How many minutes back to search (default 30)")
    severity: str = Field(default="all", description="Filter by severity: 'warning', 'critical', or 'all'")

class DrivingHistoryInput(BaseModel):
    domain:  str = Field(description="Domain to query: powertrain, battery, location, cabin, or adas")
    minutes: int = Field(default=10, description="How many minutes back to search (default 10)")


# ── Agent runner ──────────────────────────────────────────────────────────────

def run_agent(
    message: str,
    conversation_id: str,
    lat: Optional[float],
    lon: Optional[float],
    network_mode: str = "offline",
) -> dict:
    navigation_result: list = []
    is_online = network_mode == "online"
    active_search_url = MONGODB_SEARCH_SERVICE_URL if is_online else SEARCH_SERVICE_URL
    tool_name = "search_car_manual_atlas" if is_online else "search_car_manual_objectbox"

    # ── Tool definitions ──────────────────────────────────────────────────────

    def _search_manual(query: str) -> str:
        print(f"[agent] {tool_name} called, url={active_search_url}", flush=True)
        return _search_manual_impl(query, active_search_url)

    def _navigate(destination: str) -> str:
        try:
            resp = _http.post(
                f"{NAVIGATION_SERVICE_URL}/navigate",
                json={"query": destination, "lat": lat, "lon": lon},
                timeout=60,
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

    def _call_telemetry(name: str, args: dict = {}) -> str:
        try:
            resp = _http.post(f"{TELEMETRY_SERVICE_URL}/tools/{name}", json=args, timeout=10)
            if resp.ok:
                return resp.json().get("result", resp.text)
            return f"Telemetry error: {resp.status_code}"
        except Exception as e:
            return f"Telemetry service unavailable: {e}"

    tools = [
        StructuredTool.from_function(
            func=_search_manual,
            name=tool_name,
            description=(
                "Search the car manual for maintenance procedures, repair guides, "
                "warning light explanations, and vehicle specifications."
            ),
            args_schema=ManualSearchInput,
        ),
        StructuredTool.from_function(
            func=_navigate,
            name="navigate_to",
            description=(
                "Navigate to a nearby destination. Use when the user wants directions "
                "to a mechanic, gas station, hospital, pharmacy, or any other place."
            ),
            args_schema=NavigateInput,
        ),
    ]

    if is_online:
        tools += [
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_vehicle_status"),
                name="get_vehicle_status",
                description="Get a full snapshot of the vehicle: VehicleMeta plus all current state entities (powertrain, battery, chassis, cabin, location, ADAS).",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_powertrain_status"),
                name="get_powertrain_status",
                description="Get current powertrain state: speed, RPM, coolant temperature, transmission gear, throttle, and odometer. For fuel level, use get_fuel_status.",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_fuel_status"),
                name="get_fuel_status",
                description="Get the vehicle's liquid FUEL status: fuel level %, litres remaining, and consumption rate. Use for fuel / petrol / gas / 'how much fuel left' questions — NOT the electric battery.",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_battery_status"),
                name="get_battery_status",
                description="Get current battery state: state of charge (SOC%), estimated range, charging status, voltage, current, temperature, and state of health.",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_chassis_status"),
                name="get_chassis_status",
                description="Get current chassis state: tire pressures for all four tires (with warnings), ABS status, ESC status, and brake fluid level.",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_cabin_status"),
                name="get_cabin_status",
                description="Get current cabin state: door open/lock status for all doors, temperature setpoint, interior temperature, HVAC, fan speed, and windows.",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_location"),
                name="get_location",
                description="Get current vehicle location: latitude, longitude, altitude, heading, GPS speed, and geohash.",
            ),
            StructuredTool.from_function(
                func=lambda: _call_telemetry("get_adas_status"),
                name="get_adas_status",
                description="Get current ADAS state: cruise control, lane keep assist, lane departure warning, collision warning, blind spot warnings, and automatic emergency braking.",
            ),
            StructuredTool.from_function(
                func=lambda minutes, severity: _call_telemetry("get_vehicle_events", {"minutes": minutes, "severity": severity}),
                name="get_vehicle_events",
                description="Query recent vehicle events (warnings, alerts, critical notices). Filter by time window and optionally by severity.",
                args_schema=VehicleEventsInput,
            ),
            StructuredTool.from_function(
                func=lambda domain, minutes: _call_telemetry("get_driving_history", {"domain": domain, "minutes": minutes}),
                name="get_driving_history",
                description="Retrieve time-series samples from a specific telemetry domain. Valid domains: powertrain, battery, location, cabin, adas.",
                args_schema=DrivingHistoryInput,
            ),
        ]

    print(f"[agent] network_mode={network_mode} tools={[t.name for t in tools]}", flush=True)

    # ── LangGraph ReAct agent ─────────────────────────────────────────────────

    system_prompt = _build_system_prompt(lat, lon, network_mode)

    agent = create_react_agent(
        model=_llm,
        tools=tools,
        prompt=SystemMessage(content=system_prompt),
        checkpointer=_checkpointer,
    )

    config = {
        "configurable": {"thread_id": conversation_id},
        "callbacks": [_TimingCallback()],
    }

    t0 = time.time()
    print(f"[agent] invoke start — conversation_id={conversation_id}", flush=True)
    result = agent.invoke(
        {"messages": [HumanMessage(content=message)]},
        config=config,
    )
    print(f"[timing] agent total: {time.time()-t0:.2f}s", flush=True)

    # Last message in the graph state is always the final AIMessage
    final_message = result["messages"][-1]
    answer = _strip_think_tags(final_message.content or "")
    if not answer:
        answer = "I couldn't generate a response."

    # Only count tools used in the current turn — MemorySaver keeps full history,
    # so we slice to messages after the last HumanMessage.
    all_msgs = result["messages"]
    last_human = max(
        (i for i, m in enumerate(all_msgs) if isinstance(m, HumanMessage)),
        default=-1,
    )
    tools_used = [m.name for m in all_msgs[last_human + 1:] if isinstance(m, ToolMessage)]

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


@app.route("/debug/search", methods=["POST"])
def debug_search():
    """
    Directly query the search service with timing breakdown.
    Body: { "query": "...", "mode": "offline" | "online" }
    """
    data = request.json or {}
    query = (data.get("query") or "").strip()
    mode  = data.get("mode", "offline")

    if not query:
        return jsonify({"error": "query is required"}), 400

    search_url = MONGODB_SEARCH_SERVICE_URL if mode == "online" else SEARCH_SERVICE_URL

    t0 = time.time()
    try:
        embedding = _embed_model.encode(query, prompt_name="query", normalize_embeddings=True).tolist()
    except Exception as e:
        return jsonify({"error": f"embedding failed: {e}"}), 500
    t_embed = time.time() - t0

    t1 = time.time()
    try:
        resp = _http.post(f"{search_url}/search", json={"embedding": embedding, "limit": 3}, timeout=30)
        t_http = time.time() - t1
        if not resp.ok:
            return jsonify({
                "query": query, "search_url": search_url,
                "timing_ms": {"embed": round(t_embed * 1000), "http": round(t_http * 1000)},
                "error": f"HTTP {resp.status_code}: {resp.text[:200]}",
            }), 502
        results = resp.json().get("results", [])
    except Exception as e:
        t_http = time.time() - t1
        return jsonify({
            "query": query, "search_url": search_url,
            "timing_ms": {"embed": round(t_embed * 1000), "http": round(t_http * 1000)},
            "error": f"unreachable: {e}",
        }), 502

    return jsonify({
        "query": query,
        "mode": mode,
        "search_url": search_url,
        "timing_ms": {
            "embed": round(t_embed * 1000),
            "http":  round(t_http * 1000),
            "total": round((t_embed + t_http) * 1000),
        },
        "result_count": len(results),
        "results": [{"score": r.get("score"), "snippet": r.get("text", "")[:200]} for r in results],
    })


@app.route("/agent/chat", methods=["POST"])
def agent_chat():
    data            = request.json or {}
    message         = data.get("message", "").strip()
    conversation_id = data.get("conversation_id") or str(uuid.uuid4())
    lat             = data.get("lat")
    lon             = data.get("lon")
    network_mode    = data.get("network_mode", "offline")

    if not message:
        return jsonify({"error": "message is required"}), 400

    try:
        result = run_agent(message, conversation_id, lat, lon, network_mode)
        return jsonify(result)
    except Exception as e:
        print(f"[agent] error: {e}", flush=True)
        return jsonify({
            "error": str(e),
            "answer": "I encountered an error processing your request. Please try again.",
            "tools_used": [],
            "navigation": None,
            "conversation_id": conversation_id,
        }), 500


if __name__ == "__main__":
    port = int(os.getenv("PORT", 5002))
    print(f"LangChain Agent Service starting on port {port}")
    print(f"  LLM:              {LLM_MODEL} @ {OLLAMA_HOST}")
    print(f"  Embeddings:       {EMBEDDING_MODEL} (local)")
    print(f"  Search (offline): {SEARCH_SERVICE_URL}")
    print(f"  Search (online):  {MONGODB_SEARCH_SERVICE_URL}")
    print(f"  Navigation:       {NAVIGATION_SERVICE_URL}")
    print(f"  VSS Telemetry:    {TELEMETRY_SERVICE_URL}")
    app.run(host="0.0.0.0", port=port, debug=False)
