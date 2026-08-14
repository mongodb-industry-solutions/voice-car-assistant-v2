"""
Voice Assistant Backend — REST/SSE API (no UI, no WebSocket).

Separated from the cockpit UI (now the Next.js `voice-assistant-frontend`). This
service orchestrates the agent, navigation, telemetry and search services and adds
server-side speech: Whisper STT (browser audio → text) and Piper TTS (text → audio).

Transport: plain REST + Server-Sent Events (SSE) so everything flows through the
Next.js `/api/*` proxy on Kanopy (SSO/Istio-safe). Listens on :8080.
"""

import base64
import json
import os
import re
import subprocess
import tempfile
import threading
import time
import uuid

import requests as http_requests
from flask import Flask, request, jsonify, Response, stream_with_context
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# ── Service URLs ──────────────────────────────────────────────────────────────
SEARCH_SERVICE_URL        = os.getenv("SEARCH_SERVICE_URL",        "http://localhost:8080")
CONVERSATION_SERVICE_URL  = os.getenv("CONVERSATION_SERVICE_URL",  "http://localhost:8081")
NAVIGATION_SERVICE_URL    = os.getenv("NAVIGATION_SERVICE_URL",    "http://localhost:5001")
AGENT_SERVICE_URL         = os.getenv("AGENT_SERVICE_URL",         "http://localhost:5002")
VSS_TELEMETRY_SERVICE_URL = os.getenv("VSS_TELEMETRY_SERVICE_URL", "http://localhost:8086")
VSS_SIMULATOR_URL         = os.getenv("VSS_SIMULATOR_URL",         "http://localhost:8087")
VSS_TELEMETRY_API_URL     = os.getenv("VSS_TELEMETRY_API_URL",     "http://localhost:3002")

WHISPER_MODEL    = os.getenv("WHISPER_MODEL", "small")
WHISPER_LANGUAGE = os.getenv("WHISPER_LANGUAGE", "en")

# ── Sync proxy (toxiproxy) ──────────────────────────────────────────────────────
# Offline/online is a NETWORK toggle: a toxiproxy proxy sits in front of the ObjectBox
# Sync Server, and the C++ clients connect THROUGH it. Disabling the proxy cuts the
# client↔server link (edge keeps writing locally, nothing reaches Atlas); enabling it
# lets the clients auto-reconnect and flush their backlog. We drive it from here because
# the ObjectBox client cannot be paused/resumed in-process — v5.1.0 forbids restarting a
# started client (start() → "startedOnce") and close()+recreate loses the sync cursor.
TOXIPROXY_URL       = os.getenv("TOXIPROXY_URL",       "http://localhost:8474")
SYNC_PROXY_NAME     = os.getenv("SYNC_PROXY_NAME",     "sync")
SYNC_PROXY_LISTEN   = os.getenv("SYNC_PROXY_LISTEN",   "0.0.0.0:9998")
SYNC_PROXY_UPSTREAM = os.getenv("SYNC_PROXY_UPSTREAM", "sync-server:9999")

# Sync scope for online/offline. "global" (default → local docker-compose) pauses the shared
# toxiproxy connection for the whole deployment, exactly as before. "session" (set on Kanopy)
# buffers per-vehicle in vss-telemetry-service so each browser session goes offline on its own.
# The default preserves local behaviour with no config.
SYNC_SCOPE = os.getenv("SYNC_SCOPE", "global").strip().lower()


def _proxy_get():
    """Return the toxiproxy 'sync' proxy as a dict, creating it (enabled) if missing.
    Returns None when toxiproxy is unreachable."""
    try:
        r = http_requests.get(f"{TOXIPROXY_URL}/proxies/{SYNC_PROXY_NAME}", timeout=3)
        if r.status_code == 404:
            http_requests.post(
                f"{TOXIPROXY_URL}/proxies",
                json={"name": SYNC_PROXY_NAME, "listen": SYNC_PROXY_LISTEN,
                      "upstream": SYNC_PROXY_UPSTREAM, "enabled": True},
                timeout=3,
            )
            r = http_requests.get(f"{TOXIPROXY_URL}/proxies/{SYNC_PROXY_NAME}", timeout=3)
        return r.json() if r.ok else None
    except Exception:
        return None


def _proxy_set_enabled(enabled: bool) -> None:
    """Enable/disable the sync proxy. Disabling closes open connections and stops
    listening, so the ObjectBox clients disconnect and buffer writes locally."""
    p = _proxy_get() or {}
    body = {
        "name":     SYNC_PROXY_NAME,
        "listen":   p.get("listen",   SYNC_PROXY_LISTEN),
        "upstream": p.get("upstream", SYNC_PROXY_UPSTREAM),
        "enabled":  enabled,
    }
    r = http_requests.post(f"{TOXIPROXY_URL}/proxies/{SYNC_PROXY_NAME}", json=body, timeout=5)
    r.raise_for_status()


def _ensure_proxy_loop():
    """Create the proxy as soon as toxiproxy is reachable so replication works headless
    (before anyone opens the UI). Idempotent; returns once the proxy exists."""
    for _ in range(150):  # ~5 min of retries at 2s
        if _proxy_get() is not None:
            print("Sync proxy ready", flush=True)
            return
        time.sleep(2)


threading.Thread(target=_ensure_proxy_loop, daemon=True, name="ensure-sync-proxy").start()

# ── Whisper STT (lazy singleton; model weights baked into the image) ──────────
_whisper_model = None
_whisper_lock = threading.Lock()


def _get_whisper():
    global _whisper_model
    if _whisper_model is None:
        with _whisper_lock:
            if _whisper_model is None:
                import whisper  # imported lazily so /health doesn't pay the load cost
                print(f"Loading Whisper model: {WHISPER_MODEL}", flush=True)
                _whisper_model = whisper.load_model(WHISPER_MODEL)
                print("Whisper ready", flush=True)
    return _whisper_model


# ── Piper TTS (self-contained binary) ─────────────────────────────────────────
_PIPER_BIN   = os.getenv("PIPER_BIN",   "/opt/piper/piper")
_PIPER_MODEL = os.getenv("PIPER_MODEL", "/app/en_US-lessac-medium.onnx")
if not (os.path.exists(_PIPER_BIN) and os.path.exists(_PIPER_MODEL)):
    print(f"⚠️  Piper TTS not found (bin={_PIPER_BIN}, model={_PIPER_MODEL}); /tts disabled.", flush=True)
    _PIPER_BIN = None

_MD_BOLD   = re.compile(r"\*\*(.*?)\*\*")
_MD_ITALIC = re.compile(r"\*(.*?)\*")
_BULLETS   = re.compile(r"[•·]")
_SPACES    = re.compile(r"\s+")


def _clean_tts_text(text: str) -> str:
    text = _MD_BOLD.sub(r"\1", text)
    text = _MD_ITALIC.sub(r"\1", text)
    text = _BULLETS.sub(" ", text)
    text = _SPACES.sub(" ", text)
    return text.strip()


def _synthesize(text: str) -> bytes | None:
    """Run Piper on `text`, returning WAV bytes (or None if TTS unavailable)."""
    if not _PIPER_BIN:
        return None
    clean = _clean_tts_text(text)
    if not clean:
        return None
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            tmp_path = f.name
        proc = subprocess.Popen(
            [_PIPER_BIN, "--model", _PIPER_MODEL, "--output_file", tmp_path],
            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        proc.communicate(input=clean.encode())
        with open(tmp_path, "rb") as f:
            return f.read()
    except Exception as e:
        print(f"[tts] synthesis failed: {e}", flush=True)
        return None
    finally:
        if tmp_path and os.path.exists(tmp_path):
            os.unlink(tmp_path)


# ── Conversation persistence ──────────────────────────────────────────────────

def _save_message(conversation_id, user_id, role, message, tools_used=None, sources=None):
    try:
        http_requests.post(
            f"{CONVERSATION_SERVICE_URL}/conversations/message",
            json={
                "conversation_id": conversation_id,
                "user_id":         user_id,
                "role":            role,
                "message":         message,
                "tools_used":      json.dumps(tools_used) if tools_used else "",
                "sources":         json.dumps(sources) if sources else "",
            },
            timeout=2,
        )
    except Exception:
        pass


# ── Health ────────────────────────────────────────────────────────────────────

@app.route("/")
@app.route("/health")
def health():
    return jsonify({"status": "ok", "service": "voice-assistant-backend"})


@app.route("/api/stats")
def api_stats():
    """Search-service health + chunk count (used by the UI status bar)."""
    try:
        health = http_requests.get(f"{SEARCH_SERVICE_URL}/health", timeout=2).json()
        return jsonify({"search_service": "online", "chunk_count": health.get("chunk_count", 0)})
    except Exception:
        return jsonify({"search_service": "offline", "chunk_count": 0})


# ── Chat (SSE) ─────────────────────────────────────────────────────────────────

@app.route("/chat/stream", methods=["POST"])
def chat_stream():
    """
    Stream an agent answer as SSE. Body: {message, conversation_id?, user_id?, lat, lon, network_mode}.
    Relays the agent's token/status/done events straight through to the caller and
    persists the user + assistant messages to the conversation service.
    """
    data = request.get_json(silent=True) or {}
    message = (data.get("message") or "").strip()
    if not message:
        return jsonify({"error": "message is required"}), 400

    conversation_id = data.get("conversation_id") or str(uuid.uuid4())
    user_id         = data.get("user_id") or str(uuid.uuid4())
    lat, lon        = data.get("lat"), data.get("lon")
    network_mode    = data.get("network_mode", "offline")
    vehicle_id      = data.get("vehicle_id")  # per-session vehicle; agent defaults if None

    def generate():
        # Echo the resolved ids so a fresh client can adopt them.
        yield f"data: {json.dumps({'meta': {'conversation_id': conversation_id, 'user_id': user_id}})}\n\n"
        _save_message(conversation_id, user_id, "user", message)
        try:
            resp = http_requests.post(
                f"{AGENT_SERVICE_URL}/agent/chat/stream",
                json={"message": message, "conversation_id": conversation_id,
                      "lat": lat, "lon": lon, "network_mode": network_mode,
                      "vehicle_id": vehicle_id},
                stream=True, timeout=180,
            )
            resp.raise_for_status()
        except Exception as e:
            yield f"data: {json.dumps({'error': f'Agent unavailable: {e}'})}\n\n"
            return

        try:
            for raw in resp.iter_lines():
                if not raw or not raw.startswith(b"data: "):
                    continue
                # Forward the event verbatim to the browser…
                yield raw.decode("utf-8", "replace") + "\n\n"
                # …and, on the terminal event, persist the assistant turn.
                try:
                    event = json.loads(raw[6:])
                except json.JSONDecodeError:
                    continue
                if event.get("done"):
                    _save_message(conversation_id, user_id, "assistant",
                                  event.get("answer", ""),
                                  tools_used=event.get("tools_used"),
                                  sources=event.get("sources"))
        finally:
            resp.close()

    return Response(
        stream_with_context(generate()),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


# ── Speech: STT + TTS ───────────────────────────────────────────────────────────

@app.route("/stt", methods=["POST"])
def stt():
    """Transcribe an uploaded audio blob (any ffmpeg-decodable format) → {text}."""
    if "audio" not in request.files:
        return jsonify({"error": "audio file is required (multipart field 'audio')"}), 400
    f = request.files["audio"]
    suffix = os.path.splitext(f.filename or "")[1] or ".webm"
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(suffix=suffix, delete=False) as tmp:
            tmp_path = tmp.name
            f.save(tmp_path)
        result = _get_whisper().transcribe(tmp_path, language=WHISPER_LANGUAGE, fp16=False)
        return jsonify({"text": (result.get("text") or "").strip()})
    except Exception as e:
        print(f"[stt] failed: {e}", flush=True)
        return jsonify({"error": str(e)}), 500
    finally:
        if tmp_path and os.path.exists(tmp_path):
            os.unlink(tmp_path)


@app.route("/tts", methods=["POST"])
def tts():
    """Synthesize speech for {text} → audio/wav."""
    data = request.get_json(silent=True) or {}
    text = (data.get("text") or "").strip()
    if not text:
        return jsonify({"error": "text is required"}), 400
    audio = _synthesize(text)
    if audio is None:
        return jsonify({"error": "TTS unavailable"}), 503
    return Response(audio, mimetype="audio/wav")


# ── Proxies (telemetry / simulator / navigation) ────────────────────────────────

@app.route("/api/vss/latest")
def api_vss_latest():
    try:
        # Forward query params (vehicleId for the per-session vehicle, plus any cache-buster).
        resp = http_requests.get(f"{VSS_TELEMETRY_SERVICE_URL}/vss/latest", params=request.args, timeout=3)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS telemetry service unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/vss/simulator/start", methods=["POST"])
def api_sim_start():
    try:
        resp = http_requests.post(f"{VSS_SIMULATOR_URL}/simulator/start", json=request.get_json(silent=True) or {}, timeout=10)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/vss/simulator/stop", methods=["POST"])
def api_sim_stop():
    try:
        resp = http_requests.post(f"{VSS_SIMULATOR_URL}/simulator/stop", json=request.get_json(silent=True) or {}, timeout=10)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/vss/simulator/status")
def api_sim_status():
    try:
        resp = http_requests.get(f"{VSS_SIMULATOR_URL}/simulator/status", params=request.args, timeout=3)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable", "running": False}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


def _session_vehicle_id():
    """vehicleId for session-scope sync ops — from the query string or the JSON body."""
    return (request.args.get("vehicleId")
            or (request.get_json(silent=True) or {}).get("vehicle_id")
            or "")


@app.route("/api/sync/state")
def api_sync_state():
    """Edge counts + buffered backlog + paused/connected + cloud counts, for the live sync panel.
    Session scope: per-vehicle (from vss-telemetry-service /session/status + per-vehicle cloud count).
    Global scope: global edge status + toxiproxy-owned paused + global cloud count."""
    edge, cloud = {}, {}
    if SYNC_SCOPE == "session":
        vid = _session_vehicle_id()
        try:
            edge = http_requests.get(f"{VSS_TELEMETRY_SERVICE_URL}/session/status",
                                     params={"vehicleId": vid}, timeout=3).json()
        except Exception as e:
            edge = {"error": str(e)}
        try:
            cloud = http_requests.get(f"{VSS_TELEMETRY_API_URL}/cloud/counts",
                                      params={"vehicleId": vid}, timeout=5).json()
        except Exception as e:
            cloud = {"error": str(e)}
        return jsonify({"edge": edge, "cloud": cloud})

    # global scope (toxiproxy) — unchanged
    try:
        edge = http_requests.get(f"{VSS_TELEMETRY_SERVICE_URL}/sync/status", timeout=3).json()
    except Exception as e:
        edge = {"error": str(e)}
    # paused/connected are owned by the proxy layer, not the ObjectBox client (which is
    # always "started"). Derive them from whether the sync proxy is currently enabled.
    proxy = _proxy_get()
    if proxy is not None:
        enabled = bool(proxy.get("enabled", True))
        edge["paused"]    = not enabled
        edge["connected"] = enabled and edge.get("available") is not False
    try:
        cloud = http_requests.get(f"{VSS_TELEMETRY_API_URL}/cloud/counts", timeout=5).json()
    except Exception as e:
        cloud = {"error": str(e)}
    return jsonify({"edge": edge, "cloud": cloud})


@app.route("/api/sync/pause", methods=["POST"])
def api_sync_pause():
    """Go OFFLINE. Session scope: buffer this vehicle's snapshots at the edge (nothing reaches
    Atlas). Global scope: cut the shared client↔Sync-Server link at the toxiproxy layer."""
    if SYNC_SCOPE == "session":
        try:
            r = http_requests.post(f"{VSS_TELEMETRY_SERVICE_URL}/session/pause",
                                   params={"vehicleId": _session_vehicle_id()}, timeout=10)
            return jsonify(r.json()), r.status_code
        except Exception as e:
            return jsonify({"success": False, "error": str(e)}), 503
    try:
        _proxy_set_enabled(False)
        return jsonify({"success": True, "paused": True})
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 503


@app.route("/api/sync/resume", methods=["POST"])
def api_sync_resume():
    """Go ONLINE. Session scope: flush this vehicle's buffered snapshots (they sync to Atlas).
    Global scope: restore the toxiproxy link and nudge the clients to reconnect."""
    if SYNC_SCOPE == "session":
        try:
            r = http_requests.post(f"{VSS_TELEMETRY_SERVICE_URL}/session/resume",
                                   params={"vehicleId": _session_vehicle_id()}, timeout=15)
            return jsonify(r.json()), r.status_code
        except Exception as e:
            return jsonify({"success": False, "error": str(e)}), 503
    try:
        _proxy_set_enabled(True)
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 503
    # Nudge every sync client to reconnect immediately instead of waiting out its backoff.
    # Best-effort: any client also auto-reconnects on its own if the request fails.
    for url in (VSS_TELEMETRY_SERVICE_URL, SEARCH_SERVICE_URL, CONVERSATION_SERVICE_URL):
        try:
            http_requests.post(f"{url}/sync/reconnect", timeout=5)
        except Exception:
            pass
    return jsonify({"success": True, "paused": False})


@app.route("/api/navigate", methods=["POST"])
def api_navigate():
    try:
        resp = http_requests.post(f"{NAVIGATION_SERVICE_URL}/navigate", json=request.get_json(silent=True), timeout=60)
        try:
            data = resp.json()
        except ValueError:
            return jsonify({"error": f"Navigation service error (HTTP {resp.status_code})"}), 500
        return jsonify(data), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "Navigation service is not running"}), 503
    except http_requests.exceptions.Timeout:
        return jsonify({"error": "Navigation request timed out"}), 504
    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    port = int(os.getenv("PORT", 8080))
    # Bind host is configurable so the single-pod deploy can restrict this internal
    # API to loopback (HOST=127.0.0.1) — the frontend proxies over localhost in-pod.
    # Default 0.0.0.0 keeps cross-container docker-compose working.
    host = os.getenv("HOST", "0.0.0.0")
    print(f"Voice Assistant Backend on {host}:{port}", flush=True)
    app.run(host=host, port=port, debug=False, threaded=True)
