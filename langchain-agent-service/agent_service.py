"""
LangChain Agent Service — Unified car assistant
Combines: car manual RAG, vehicle telemetry (via VSS Telemetry API), navigation.

Uses a LangGraph ReAct agent (create_react_agent) with:
  - a chat model chosen by LLM_PROVIDER (local Ollama, or Grove-hosted Claude) via llm_provider.make_chat_llm
  - MemorySaver checkpointer for per-conversation history
  - Two agents pre-compiled at startup: _OFFLINE_AGENT and _ONLINE_AGENT
    Selected per-request by network_mode; graph compilation cost is zero per request.
"""

import functools
import json
import os
import re
import time
import unicodedata
import uuid
from typing import Any, Optional

import requests
from requests.adapters import HTTPAdapter
from sentence_transformers import SentenceTransformer
from flask import Flask, jsonify, request, stream_with_context, Response
from flask_cors import CORS
from langchain_core.callbacks import BaseCallbackHandler
from langchain_core.messages import AIMessageChunk, HumanMessage, SystemMessage, ToolMessage, trim_messages
from langchain_core.runnables import RunnableLambda, RunnableConfig
from langchain_core.tools import StructuredTool
from langgraph.prebuilt import create_react_agent
from langgraph.checkpoint.memory import MemorySaver
from pydantic import BaseModel, Field

from llm_provider import make_chat_llm

app = Flask(__name__)
CORS(app)

# ── Service configuration ─────────────────────────────────────────────────────

OLLAMA_HOST                = os.getenv("OLLAMA_HOST",                "http://localhost:11434")
LLM_MODEL                  = os.getenv("LLM_MODEL",                  "qwen2.5:3b")
EMBEDDING_MODEL            = os.getenv("EMBEDDING_MODEL",            "voyageai/voyage-4-nano")
SEARCH_SERVICE_URL         = os.getenv("SEARCH_SERVICE_URL",         "http://localhost:8080")
MONGODB_SEARCH_SERVICE_URL = os.getenv("MONGODB_SEARCH_SERVICE_URL", "http://localhost:8085")
NAVIGATION_SERVICE_URL     = os.getenv("NAVIGATION_SERVICE_URL",     "http://localhost:5001")
TELEMETRY_SERVICE_URL      = os.getenv("VSS_TELEMETRY_API_URL",      "http://localhost:3002")

# ── Module-level singletons (shared across all Gunicorn threads) ──────────────

# voyage-4-nano ships custom model code (Qwen3 bidirectional) and MUST be loaded
# with trust_remote_code=True; without it transformers emits a wrong 2048-d vector.
# It is a Matryoshka model — truncate_dim=1024 selects its native 1024-d head.
# Requires transformers==4.57.1 (see requirements.txt).
EMBED_DIM = int(os.getenv("EMBED_DIM", "1024"))
if EMBED_DIM != 1024:
    raise ValueError(f"EMBED_DIM must be 1024 to match search-service (got {EMBED_DIM})")
print(f"Loading embedding model: {EMBEDDING_MODEL} @ {EMBED_DIM} dims", flush=True)
_embed_model = SentenceTransformer(EMBEDDING_MODEL, trust_remote_code=True, truncate_dim=EMBED_DIM)
print("Embedding model ready", flush=True)

# LLM — stateless; safe to share across gthread workers.
# Provider selected by LLM_PROVIDER (default "ollama" local; "grove" = hosted Claude
# on Kanopy). Ollama params (num_ctx/num_predict) live in llm_provider.make_chat_llm.
_llm = make_chat_llm()

# Conversation memory — keyed by conversation_id as thread_id
_checkpointer = MemorySaver()

# No retries: slow failures in the hot path should surface immediately
_http = requests.Session()
_http.mount("http://",  HTTPAdapter(max_retries=0))
_http.mount("https://", HTTPAdapter(max_retries=0))

# Regex to extract structured navigation data embedded in navigate_to tool output
_NAV_DATA_RE = re.compile(r"\[ROUTE_DATA:(.*?)\]$", re.DOTALL)

# Regex to extract structured source chunks embedded in search_car_manual tool output
_SOURCES_RE = re.compile(r"\[SOURCES:(.*?)\]$", re.DOTALL)

# Max characters returned by search tools — caps LLM context bloat from large manual chunks
# ~2 400 chars ≈ 600 tokens; a typical manual section fits; the LLM uses ~10 % of it anyway
MAX_SEARCH_CHARS = 2400

# Max characters returned by telemetry tools — domain status dumps are 800–2 000 chars;
# 600 chars (~150 tokens) captures the key fields and keeps call #2 prefill fast
MAX_TELEMETRY_CHARS = 600

# ── Input validation ──────────────────────────────────────────────────────────

MAX_MESSAGE_CHARS = 500

# Patterns that indicate a prompt-injection attempt rather than a car question.
# Compiled once at module load; matched case-insensitively against the raw message.
_INJECTION_PATTERNS = re.compile(
    r"ignore\s+(all\s+|previous\s+|prior\s+|your\s+)?instructions"
    r"|^\s*(system|assistant|user)\s*:"
    r"|\[\s*system\s*\]"
    r"|<\s*system\s*>"
    r"|you\s+are\s+now\s+(a|an|the)\b"
    r"|act\s+as\s+(a|an|the)\b"
    r"|pretend\s+(you\s+are|to\s+be)\b"
    r"|new\s+instructions\s*:"
    r"|override\s*:"
    r"|jailbreak",
    re.IGNORECASE | re.MULTILINE,
)

_UUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
    re.IGNORECASE,
)


class _InputError(ValueError):
    """Raised for invalid or suspicious request parameters."""


def _sanitize_message(raw: str) -> str:
    """
    Validate and clean a user message before it enters the agent.

    Steps:
    1. Length cap — prevents large injection payloads and context bloat.
    2. Control-character stripping — removes null bytes and Unicode Cc/Cf
       characters (except \\n/\\t) that can confuse model tokenisation.
    3. Injection pattern detection — rejects messages that match known
       prompt-override phrases; raises _InputError so the caller returns 400.
    """
    if not raw or not raw.strip():
        raise _InputError("message is required")
    if len(raw) > MAX_MESSAGE_CHARS:
        raise _InputError(f"message too long (max {MAX_MESSAGE_CHARS} characters)")

    # Strip Unicode control characters (category Cc/Cf) except tab and newline.
    cleaned = "".join(
        ch for ch in raw
        if ch in ("\t", "\n") or unicodedata.category(ch) not in ("Cc", "Cf")
    )

    if _INJECTION_PATTERNS.search(cleaned):
        print(f"[security] injection attempt blocked: {cleaned[:120]!r}", flush=True)
        raise _InputError("message contains disallowed content")

    return cleaned.strip()


def _validate_conversation_id(raw: str | None) -> str:
    """Accept a valid UUID v4 string or generate a fresh one. Rejects arbitrary strings."""
    if raw is None:
        return str(uuid.uuid4())
    if _UUID_RE.match(raw):
        return raw
    # Non-UUID IDs could be used to probe other users' thread histories.
    print(f"[security] invalid conversation_id rejected: {raw!r}", flush=True)
    return str(uuid.uuid4())


def _validate_network_mode(raw: str | None) -> str:
    if raw in ("online", "offline"):
        return raw
    return "offline"


def _validate_coord(value: Any, name: str) -> Optional[float]:
    """Parse and range-check a latitude or longitude value."""
    if value is None:
        return None
    try:
        f = float(value)
    except (TypeError, ValueError):
        raise _InputError(f"invalid {name}: must be a number")
    limits = {"lat": (-90.0, 90.0), "lon": (-180.0, 180.0)}
    lo, hi = limits[name]
    if not (lo <= f <= hi):
        raise _InputError(f"invalid {name}: {f} out of range [{lo}, {hi}]")
    return f


# ── Helpers ───────────────────────────────────────────────────────────────────

def _content_to_text(content) -> str:
    """
    Normalise a LangChain message's content to plain text.

    Ollama returns a string; Anthropic/Claude (langchain_anthropic) returns a list
    of content blocks (e.g. [{"type": "text", "text": "..."}], plus tool_use blocks).
    Concatenate the text blocks and ignore non-text ones so downstream string ops
    (streaming accumulation, think-tag stripping) never see a list.
    """
    if content is None:
        return ""
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for block in content:
            if isinstance(block, str):
                parts.append(block)
            elif isinstance(block, dict) and isinstance(block.get("text"), str):
                parts.append(block["text"])
        return "".join(parts)
    return str(content)


def _strip_think_tags(text: str) -> str:
    """Remove <think>...</think> blocks that some models embed in their output."""
    return re.sub(r"<think>.*?</think>", "", text, flags=re.DOTALL).strip()


def _ui_tool_name(name: str, network_mode: str) -> str:
    """
    Map the internal tool name to a UI-facing name for the tools_used badge list.
    Both agents use 'search_car_manual' as the tool name; the frontend badge map
    expects 'search_car_manual_objectbox' (offline) or 'search_car_manual_atlas' (online).
    """
    if name == "search_car_manual":
        return "search_car_manual_atlas" if network_mode == "online" else "search_car_manual_objectbox"
    return name


def _strip_tool_markers(messages: list) -> list:
    """
    Strip [ROUTE_DATA:...] and [SOURCES:...] blocks from ToolMessages before they
    reach the LLM. Both carry structured data — route geometry (coordinate arrays)
    and manual source chunks — that the LLM has no use for and that would waste
    context. Extraction happens in run_agent/stream_agent from the raw graph state
    before this stripping, so nothing is lost for the caller.
    """
    out = []
    for msg in messages:
        content = msg.content if isinstance(msg, ToolMessage) else None
        if content and (_NAV_DATA_RE.search(content) or _SOURCES_RE.search(content)):
            cleaned = _SOURCES_RE.sub("", _NAV_DATA_RE.sub("", content)).rstrip()
            out.append(ToolMessage(content=cleaned, tool_call_id=msg.tool_call_id, name=msg.name))
        else:
            out.append(msg)
    return out


class _TimingCallback(BaseCallbackHandler):
    """Logs per-LLM-call and per-tool latency inside the agent loop."""

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
        print(f"[cb] LLM error after {time.time() - self._llm_start:.2f}s: {error}", flush=True)

    def on_tool_start(self, serialized: dict, input_str: str, **kwargs: Any) -> None:
        self._tool_start = time.time()
        print(f"[cb] tool '{serialized.get('name', '?')}' start  input={input_str[:120]}", flush=True)

    def on_tool_end(self, output: Any, **kwargs: Any) -> None:
        print(f"[cb] tool end: {time.time() - self._tool_start:.2f}s  output={str(output)[:120]}", flush=True)

    def on_tool_error(self, error: Exception, **kwargs: Any) -> None:
        print(f"[cb] tool error after {time.time() - self._tool_start:.2f}s: {error}", flush=True)


# ── System prompt ─────────────────────────────────────────────────────────────

# Static body — never recomputed per request.
# Kept short (~230 tokens) to maximise usable num_ctx for history and tool results.
# Tool descriptions carry the per-tool routing detail; this prompt covers behaviour rules only.
# The only dynamic part is the one-line location hint appended in _build_prompt.
_SYSTEM_PROMPT_BODY = """\
You are a car assistant. Plain prose only, no preamble, max 3 sentences.

TOOL USE IS MANDATORY:
• "How do I", "what does X mean", procedures, repairs, warning lights, specs, owner's manual → search_car_manual. Never answer from memory.
• Overall status, "how is my car", or several readings at once → get_vehicle_status (ONE call). Never guess values.
• A specific single reading (just fuel, just tires, etc.) → matching LIVE READING tool. Never guess values.
• Navigation → navigate_to immediately.
• "Any faults", "check engine", "warning lights on", "what codes are set" → get_diagnostics (live). But "what does THIS warning light MEAN" / how to fix → search_car_manual.
• Problem reported → relevant domain tool (powertrain, battery, chassis, diagnostics, etc.).
• Reading + what to do → telemetry tool then search_car_manual.

RULES: Summarise search_car_manual results. Never mention page numbers. After navigate_to, confirm destination and ETA only — no turn-by-turn steps. Lead with safety action for critical issues.\
"""


# Conversation-history budget (tokens) sent to the LLM per call. num_ctx=1536 total,
# minus ~230 (system) + ~250 (tool schemas) + 400 (num_predict output) ≈ 650 for history.
_HISTORY_TOKEN_BUDGET = 600


def _approx_tokens(messages: list) -> int:
    """
    Dependency-free ≈token estimate for trim_messages budgeting (~4 chars/token,
    plus per-message and tool-call overhead). Avoids requiring a newer langchain-core
    just for count_tokens_approximately; exactness isn't needed for trimming.
    """
    chars = 0
    for m in messages:
        text = m.content if isinstance(m.content, str) else str(m.content)
        chars += len(text) + 16  # per-message role/formatting overhead
        tool_calls = getattr(m, "tool_calls", None)
        if tool_calls:
            chars += len(str(tool_calls))  # AIMessage tool calls carry no .content
    return chars // 4


def _build_prompt(input_: list | dict, config: RunnableConfig) -> list:
    """
    Called by LangGraph before every LLM invocation.
    Prepends the static system prompt + a one-line location hint, then token-aware
    trims history to _HISTORY_TOKEN_BUDGET so num_ctx is never silently overflowed.
    LangGraph may pass either the raw message list or the full state dict depending
    on version — both are handled here.
    """
    messages = input_["messages"] if isinstance(input_, dict) else input_
    cfg = config.get("configurable", {})
    lat = cfg.get("lat")
    # Mode hint: live telemetry is Atlas-only, so it is unavailable offline. Tell the
    # LLM its current mode and to refuse (state the mode) rather than guess when the
    # requested capability isn't available in this mode.
    network_mode = cfg.get("network_mode", "offline")
    if network_mode == "online":
        mode_hint = (
            "MODE: You are ONLINE — car manual, live vehicle telemetry, and navigation "
            "are all available."
        )
    else:
        mode_hint = (
            "MODE: You are OFFLINE. Live vehicle telemetry (overall car status, battery, "
            "fuel, tyres, powertrain, cabin, location, fault-code/diagnostics readings) "
            "needs an online connection "
            "and is NOT available now. If the user asks for any live vehicle reading, tell "
            "them you are offline and cannot access live vehicle data right now — do not "
            "guess values. Car-manual questions and navigation still work."
        )
    loc_hint = (
        "The user's GPS location is known. "
        "Call navigate_to with just the destination name — coordinates are handled automatically. "
        "Never mention raw coordinates to the user."
        if lat is not None
        else
        "The user's GPS location is not available. "
        "Ask them to allow location access in the browser if navigation is requested."
    )
    # Strip bulky [ROUTE_DATA:...] geometry first so it is neither counted nor kept,
    # then keep the most recent turns that fit the budget. start_on="human" keeps
    # AIMessage(tool_call)/ToolMessage pairs intact at the cut point (some models
    # reject an orphan tool response), which a plain messages[-N:] slice could split.
    history = _strip_tool_markers(messages)
    trimmed = trim_messages(
        history,
        max_tokens=_HISTORY_TOKEN_BUDGET,
        token_counter=_approx_tokens,
        strategy="last",
        start_on="human",
        include_system=False,
        allow_partial=False,
    )
    # trim_messages returns [] when the most recent human-anchored window alone
    # exceeds the budget (e.g. a large tool result mid-ReAct). Never send an empty
    # history — fall back to the current turn (last human message → end), which
    # keeps the user's question and any in-flight tool results intact.
    if not trimmed:
        last_human = max(
            (i for i, m in enumerate(history) if isinstance(m, HumanMessage)),
            default=0,
        )
        trimmed = history[last_human:]
    return [SystemMessage(content=_SYSTEM_PROMPT_BODY + f"\n\n{mode_hint}\n\n{loc_hint}")] + trimmed


_PROMPT_RUNNABLE = RunnableLambda(_build_prompt)


# ── Tool implementations (module-level, stateless) ────────────────────────────

@functools.lru_cache(maxsize=256)
def _embed_query(query: str) -> tuple:
    """Cached embedding — avoids recomputing for repeated or identical queries."""
    return tuple(_embed_model.encode(query, prompt_name="query", normalize_embeddings=True).tolist())


def _search_manual_impl(query: str, search_url: str) -> str:
    print(f"[search] query='{query[:60]}' url={search_url}", flush=True)
    try:
        t0 = time.time()
        embedding = list(_embed_query(query))
        print(f"[timing] embedding: {time.time() - t0:.2f}s  dims={len(embedding)}", flush=True)
        t1 = time.time()
        resp = _http.post(f"{search_url}/search", json={"embedding": embedding, "limit": 3}, timeout=5)
        print(f"[timing] search HTTP: {time.time() - t1:.2f}s  status={resp.status_code}", flush=True)
        if not resp.ok:
            return f"Car manual search error (HTTP {resp.status_code}): {resp.text[:200]}"
        results = resp.json().get("results", [])
        if not results:
            return "No relevant information found in the car manual."
        joined = "\n\n---\n\n".join(r["text"] for r in results)
        if len(joined) > MAX_SEARCH_CHARS:
            print(f"[search] truncating result {len(joined)} → {MAX_SEARCH_CHARS} chars", flush=True)
            joined = joined[:MAX_SEARCH_CHARS]
        # Preserve the per-chunk source metadata (which is dropped from the LLM-facing
        # text) after a marker. run_agent/stream_agent extract it for the caller, and
        # _strip_tool_markers removes it from the ToolMessage before any LLM call.
        # Only score + text are kept — the source is always the car manual.
        sources = [
            {
                "score": r.get("score"),
                "text":  (r.get("text") or "")[:200],
            }
            for r in results
        ]
        return joined + f"\n[SOURCES:{json.dumps(sources)}]"
    except Exception as e:
        print(f"[search] exception: {e}", flush=True)
        return f"Manual search error: {e}"


def _search_manual_offline(query: str) -> str:
    return _search_manual_impl(query, SEARCH_SERVICE_URL)


def _search_manual_online(query: str) -> str:
    return _search_manual_impl(query, MONGODB_SEARCH_SERVICE_URL)


def _navigate(destination: str, config: RunnableConfig) -> str:
    """
    LangChain injects `config` automatically because the parameter is typed
    as RunnableConfig — it is NOT part of the LLM-facing tool schema.
    lat/lon are read from config["configurable"] set by the caller.
    Navigation route data is embedded after a marker so the caller can extract
    it from the ToolMessage without re-calling the navigation service.
    """
    cfg = config.get("configurable", {})
    lat = cfg.get("lat")
    lon = cfg.get("lon")
    try:
        resp = _http.post(
            f"{NAVIGATION_SERVICE_URL}/navigate",
            json={"query": destination, "lat": lat, "lon": lon},
            timeout=60,
        )
        if resp.ok:
            data = resp.json()
            dest, route = data["destination"], data["route"]
            text = (
                f"Route found to {dest['name']}: "
                f"{route['distance_text']} away, ~{route['duration_text']} by car. "
                "Route is now displayed on the map."
            )
            # Strip turn-by-turn steps before embedding — the LLM must not narrate
            # them, and the frontend only needs the geometry to draw the route.
            route_for_llm = {k: v for k, v in data.items() if k != "route"}
            route_for_llm["route"] = {k: v for k, v in route.items() if k != "steps"}
            return text + f"\n[ROUTE_DATA:{json.dumps(route_for_llm)}]"
        return f"Navigation failed: {resp.json().get('error', 'unknown error')}"
    except Exception as e:
        return f"Navigation service unavailable: {e}"


def _call_telemetry(name: str, args: dict | None = None) -> str:
    try:
        resp = _http.post(
            f"{TELEMETRY_SERVICE_URL}/tools/{name}",
            json=args or {},
            timeout=10,
        )
        if resp.ok:
            result = resp.json().get("result", resp.text)
            if len(result) > MAX_TELEMETRY_CHARS:
                print(f"[telemetry] truncating {name} result {len(result)} → {MAX_TELEMETRY_CHARS} chars", flush=True)
                result = result[:MAX_TELEMETRY_CHARS]
            return result
        return f"Telemetry error: {resp.status_code}"
    except Exception as e:
        return f"Telemetry service unavailable: {e}"


# ── Pydantic schemas ──────────────────────────────────────────────────────────

class ManualSearchInput(BaseModel):
    query: str = Field(description="Search query")

class NavigateInput(BaseModel):
    destination: str = Field(description="Place name or type, e.g. 'gas station', 'nearest mechanic'")

class _NoInput(BaseModel):
    """Empty schema for zero-argument telemetry tools."""


# ── Tool objects ──────────────────────────────────────────────────────────────

# Both search tools share the same name so the system prompt needs no placeholder.
# Each agent gets exactly one of these — offline uses ObjectBox, online uses Atlas.
_SEARCH_MANUAL_OFFLINE_TOOL = StructuredTool.from_function(
    func=_search_manual_offline,
    name="search_car_manual",
    description="PROCEDURES & INSTRUCTIONS: how-to guides, repair steps, maintenance procedures, warning light meanings, owner's manual content, technical specifications. Use for any 'how do I' or 'what does X mean' question.",
    args_schema=ManualSearchInput,
)

_SEARCH_MANUAL_ONLINE_TOOL = StructuredTool.from_function(
    func=_search_manual_online,
    name="search_car_manual",
    description="PROCEDURES & INSTRUCTIONS: how-to guides, repair steps, maintenance procedures, warning light meanings, owner's manual content, technical specifications. Use for any 'how do I' or 'what does X mean' question.",
    args_schema=ManualSearchInput,
)

_NAVIGATE_TOOL = StructuredTool.from_function(
    func=_navigate,
    name="navigate_to",
    description="Find and show a route to a nearby place (mechanic, gas station, hospital, pharmacy, etc.).",
    args_schema=NavigateInput,
)

# Zero-argument telemetry tools use _NoInput so the LLM calls them with {}
# instead of an ambiguous empty string, which prevents tool-call parse errors.
# Descriptions are one line and carry the keyword routing hints that were removed from the system prompt.
_TELEMETRY_TOOLS = [
    StructuredTool.from_function(
        func=lambda: _call_telemetry("get_vehicle_status"),
        name="get_vehicle_status",
        description="LIVE READING: full car snapshot in ONE call — speed, fuel, battery, tires, cabin, location, ADAS. Use for general 'how is my car' / overall status questions instead of calling several tools.",
        args_schema=_NoInput,
    ),
    StructuredTool.from_function(
        func=lambda: _call_telemetry("get_powertrain_status"),
        name="get_powertrain_status",
        description="LIVE READING: current speed, RPM, coolant temp, gear, throttle.",
        args_schema=_NoInput,
    ),
    StructuredTool.from_function(
        func=lambda: _call_telemetry("get_fuel_status"),
        name="get_fuel_status",
        description="LIVE READING: current fuel level %, litres remaining, consumption rate.",
        args_schema=_NoInput,
    ),
    StructuredTool.from_function(
        func=lambda: _call_telemetry("get_battery_status"),
        name="get_battery_status",
        description="LIVE READING: current battery SOC%, state of health, range, charging status, voltage, temperature.",
        args_schema=_NoInput,
    ),
    StructuredTool.from_function(
        func=lambda: _call_telemetry("get_chassis_status"),
        name="get_chassis_status",
        description="LIVE READING: current tire pressures (all four), ABS, traction control, brake pedal.",
        args_schema=_NoInput,
    ),
    StructuredTool.from_function(
        func=lambda: _call_telemetry("get_diagnostics_status"),
        name="get_diagnostics",
        description="LIVE READING: active fault codes (DTCs), how many are set, and which warning lights are on right now. Use for 'any faults?', 'check engine', 'warning lights', 'error codes'.",
        args_schema=_NoInput,
    ),
]
# get_cabin_status, get_location, get_adas_status, get_driving_history removed to reduce
# tool schema token count; restoring them adds ~160 tokens to every LLM call prefill.


# ── Pre-compiled agents ───────────────────────────────────────────────────────
# Compiled once at startup; .invoke() is called per-request with different configs.
# Both agents share _checkpointer — MessagesState is compatible across both graphs.

print("Compiling offline agent...", flush=True)
_OFFLINE_AGENT = create_react_agent(
    model=_llm,
    tools=[_SEARCH_MANUAL_OFFLINE_TOOL, _NAVIGATE_TOOL],
    prompt=_PROMPT_RUNNABLE,
    checkpointer=_checkpointer,
)

print("Compiling online agent...", flush=True)
_ONLINE_AGENT = create_react_agent(
    model=_llm,
    tools=[_SEARCH_MANUAL_ONLINE_TOOL, _NAVIGATE_TOOL] + _TELEMETRY_TOOLS,
    prompt=_PROMPT_RUNNABLE,
    checkpointer=_checkpointer,
)
print("Agents ready.", flush=True)


# ── Agent runner ──────────────────────────────────────────────────────────────

def run_agent(
    message: str,
    conversation_id: str,
    lat: Optional[float],
    lon: Optional[float],
    network_mode: str = "offline",
) -> dict:
    agent = _ONLINE_AGENT if network_mode == "online" else _OFFLINE_AGENT

    config = {
        "configurable": {
            "thread_id": conversation_id,
            "lat": lat,
            "lon": lon,
            "network_mode": network_mode,
        },
        "callbacks": [_TimingCallback()],
        "recursion_limit": 8,
    }

    print(f"[agent] invoke start — conversation_id={conversation_id} mode={network_mode}", flush=True)
    t0 = time.time()
    result = agent.invoke({"messages": [HumanMessage(content=message)]}, config=config)
    print(f"[timing] agent total: {time.time() - t0:.2f}s", flush=True)

    final_message = result["messages"][-1]
    answer = _strip_think_tags(_content_to_text(final_message.content))
    if not answer:
        answer = "I couldn't generate a response."

    # Slice to messages generated in this turn only
    all_msgs = result["messages"]
    last_human = max(
        (i for i, m in enumerate(all_msgs) if isinstance(m, HumanMessage)),
        default=-1,
    )
    turn_msgs = all_msgs[last_human + 1:]
    tools_used = [_ui_tool_name(m.name, network_mode) for m in turn_msgs if isinstance(m, ToolMessage)]

    # Extract structured navigation data from the navigate_to ToolMessage.
    # The tool embeds JSON after [ROUTE_DATA:] so the LLM sees only the human-readable text.
    navigation = None
    nav_msg = next(
        (m for m in turn_msgs if isinstance(m, ToolMessage) and m.name == "navigate_to"),
        None,
    )
    if nav_msg:
        match = _NAV_DATA_RE.search(nav_msg.content)
        if match:
            try:
                navigation = json.loads(match.group(1))
            except json.JSONDecodeError:
                pass

    # Extract structured manual sources from the search_car_manual ToolMessage.
    sources: list = []
    src_msg = next(
        (m for m in turn_msgs if isinstance(m, ToolMessage) and m.name == "search_car_manual"),
        None,
    )
    if src_msg:
        match = _SOURCES_RE.search(src_msg.content or "")
        if match:
            try:
                sources = json.loads(match.group(1))
            except json.JSONDecodeError:
                pass

    return {
        "answer": answer,
        "tools_used": tools_used,
        "navigation": navigation,
        "sources": sources,
        "conversation_id": conversation_id,
    }


def stream_agent(
    message: str,
    conversation_id: str,
    lat: Optional[float],
    lon: Optional[float],
    network_mode: str = "offline",
):
    """
    Generator that yields SSE-formatted strings.
    Emits one `data: {"token": "..."}` event per AIMessageChunk, then a final
    `data: {"done": true, "answer": "...", "tools_used": [...], "navigation": {...}}`.
    The `answer` field in the done event is the full response with think tags stripped,
    intended for TTS — the client uses streamed tokens for progressive display.
    """
    agent = _ONLINE_AGENT if network_mode == "online" else _OFFLINE_AGENT
    config = {
        "configurable": {
            "thread_id": conversation_id,
            "lat": lat,
            "lon": lon,
            "network_mode": network_mode,
        },
        "callbacks": [_TimingCallback()],
        "recursion_limit": 8,
    }

    tools_used: list[str] = []
    full_content = ""
    navigation = None
    sources: list = []
    _status_emitted: set[str] = set()  # track which tool-call statuses we've already sent
    # Think-tag filter state — suppresses <think>...</think> from streamed tokens
    # so the UI never displays raw model reasoning, only the actual answer.
    _in_think = False

    # Human-readable labels shown in the UI while the tool is running
    _TOOL_STATUS = {
        "search_car_manual":   "Searching car manual…",
        "navigate_to":         "Finding route…",
        "get_vehicle_status":  "Checking vehicle status…",
        "get_powertrain_status": "Checking powertrain…",
        "get_fuel_status":     "Checking fuel…",
        "get_battery_status":  "Checking battery…",
        "get_chassis_status":  "Checking tires & chassis…",
        "get_diagnostics":     "Checking fault codes…",
    }

    print(f"[agent] stream start — conversation_id={conversation_id} mode={network_mode}", flush=True)
    t0 = time.time()
    try:
        for item in agent.stream(
            {"messages": [HumanMessage(content=message)]},
            config=config,
            stream_mode="messages",
        ):
            # LangGraph >=0.2 yields (chunk, metadata) tuples; guard against
            # versions that yield chunks directly.
            chunk = item[0] if isinstance(item, tuple) else item

            if isinstance(chunk, AIMessageChunk):
                # Detect tool-call decision as soon as the first tool_call_chunk arrives —
                # emit a status event immediately so the UI shows feedback during the
                # dead period before the tool result comes back.
                if not chunk.content and hasattr(chunk, "tool_call_chunks") and chunk.tool_call_chunks:
                    for tc in chunk.tool_call_chunks:
                        name = getattr(tc, "name", None) or (tc.get("name") if isinstance(tc, dict) else None)
                        if name and name not in _status_emitted:
                            _status_emitted.add(name)
                            label = _TOOL_STATUS.get(name, f"Calling {name}…")
                            yield f"data: {json.dumps({'status': label})}\n\n"
                elif chunk.content:
                    # Claude streams a list of content blocks; coerce to text.
                    piece = _content_to_text(chunk.content)
                    full_content += piece
                    # Filter <think>...</think> blocks so the UI never shows raw reasoning.
                    # Tags may span chunk boundaries; _in_think carries state across iterations.
                    token = piece
                    if _in_think:
                        close = token.find("</think>")
                        if close != -1:
                            _in_think = False
                            token = token[close + len("</think>"):].lstrip()
                        else:
                            token = ""
                    else:
                        open_ = token.find("<think>")
                        if open_ != -1:
                            before = token[:open_]
                            rest = token[open_ + len("<think>"):]
                            close = rest.find("</think>")
                            if close != -1:
                                token = before + rest[close + len("</think>"):].lstrip()
                            else:
                                _in_think = True
                                token = before
                    if token:
                        yield f"data: {json.dumps({'token': token})}\n\n"

            elif isinstance(chunk, ToolMessage):
                tools_used.append(_ui_tool_name(chunk.name, network_mode))
                # Clear the status indicator now that the tool has returned
                yield f"data: {json.dumps({'status': None})}\n\n"
                if chunk.name == "navigate_to":
                    match = _NAV_DATA_RE.search(chunk.content)
                    if match:
                        try:
                            navigation = json.loads(match.group(1))
                        except json.JSONDecodeError:
                            pass
                elif chunk.name == "search_car_manual":
                    match = _SOURCES_RE.search(chunk.content or "")
                    if match:
                        try:
                            sources = json.loads(match.group(1))
                        except json.JSONDecodeError:
                            pass

    except Exception as e:
        print(f"[agent] stream error: {e}", flush=True)
        yield f"data: {json.dumps({'error': str(e)})}\n\n"
        return

    print(f"[timing] agent stream total: {time.time() - t0:.2f}s", flush=True)
    clean_answer = _strip_think_tags(full_content) or "I couldn't generate a response."
    yield f"data: {json.dumps({'done': True, 'answer': clean_answer, 'tools_used': tools_used, 'navigation': navigation, 'sources': sources, 'conversation_id': conversation_id})}\n\n"


# ── Flask endpoints ───────────────────────────────────────────────────────────

@app.route("/health")
def health():
    return jsonify({"status": "ok", "service": "langchain-agent-service"})


@app.route("/debug/search", methods=["POST"])
def debug_search():
    """Directly query the search service with timing breakdown."""
    data  = request.json or {}
    query = (data.get("query") or "").strip()
    mode  = data.get("mode", "offline")

    if not query:
        return jsonify({"error": "query is required"}), 400

    search_url = MONGODB_SEARCH_SERVICE_URL if mode == "online" else SEARCH_SERVICE_URL

    t0 = time.time()
    try:
        embedding = list(_embed_query(query))
    except Exception as e:
        return jsonify({"error": f"embedding failed: {e}"}), 500
    t_embed = time.time() - t0

    t1 = time.time()
    try:
        resp = _http.post(f"{search_url}/search", json={"embedding": embedding, "limit": 3}, timeout=5)
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


def _parse_request(data: dict) -> tuple:
    """
    Validate and sanitize all user-controlled fields from a request payload.
    Returns (message, conversation_id, lat, lon, network_mode).
    Raises _InputError with a human-readable message on any violation.
    """
    message         = _sanitize_message(data.get("message", ""))
    conversation_id = _validate_conversation_id(data.get("conversation_id"))
    lat             = _validate_coord(data.get("lat"), "lat")
    lon             = _validate_coord(data.get("lon"), "lon")
    network_mode    = _validate_network_mode(data.get("network_mode"))
    return message, conversation_id, lat, lon, network_mode


@app.route("/agent/chat", methods=["POST"])
def agent_chat():
    data = request.json or {}
    try:
        message, conversation_id, lat, lon, network_mode = _parse_request(data)
    except _InputError as e:
        return jsonify({"error": str(e)}), 400

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


@app.route("/agent/chat/stream", methods=["POST"])
def agent_chat_stream():
    data = request.json or {}
    try:
        message, conversation_id, lat, lon, network_mode = _parse_request(data)
    except _InputError as e:
        return jsonify({"error": str(e)}), 400

    return Response(
        stream_with_context(stream_agent(message, conversation_id, lat, lon, network_mode)),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no",  # prevent nginx from buffering SSE chunks
        },
    )


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
