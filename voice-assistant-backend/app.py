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

WHISPER_MODEL    = os.getenv("WHISPER_MODEL", "small")
WHISPER_LANGUAGE = os.getenv("WHISPER_LANGUAGE", "en")

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

    def generate():
        # Echo the resolved ids so a fresh client can adopt them.
        yield f"data: {json.dumps({'meta': {'conversation_id': conversation_id, 'user_id': user_id}})}\n\n"
        _save_message(conversation_id, user_id, "user", message)
        try:
            resp = http_requests.post(
                f"{AGENT_SERVICE_URL}/agent/chat/stream",
                json={"message": message, "conversation_id": conversation_id,
                      "lat": lat, "lon": lon, "network_mode": network_mode},
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
        resp = http_requests.get(f"{VSS_TELEMETRY_SERVICE_URL}/vss/latest", timeout=3)
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
        resp = http_requests.post(f"{VSS_SIMULATOR_URL}/simulator/stop", timeout=10)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/vss/simulator/status")
def api_sim_status():
    try:
        resp = http_requests.get(f"{VSS_SIMULATOR_URL}/simulator/status", timeout=3)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable", "running": False}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


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
