"""
Web Server for Car Manual Voice Assistant Demo
Provides WebSocket interface for real-time UI updates.
All AI queries are routed to the unified LangChain agent service.
"""

import base64
import io
import os
import re
import threading
import uuid
import json
import requests as http_requests
from typing import Optional
from flask import Flask, render_template, request, jsonify, Response
from flask_socketio import SocketIO, emit
from flask_cors import CORS
import ollama
import tempfile

from main import VoiceAssistant
import config

app = Flask(__name__)
app.config['SECRET_KEY'] = 'car-manual-demo-secret'
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

# ── Service URLs ──────────────────────────────────────────────────────────────
CONVERSATION_SERVICE_URL  = os.getenv("CONVERSATION_SERVICE_URL",  "http://localhost:8081")
NAVIGATION_SERVICE_URL    = os.getenv("NAVIGATION_SERVICE_URL",    "http://localhost:5001")
AGENT_SERVICE_URL         = os.getenv("AGENT_SERVICE_URL",         "http://localhost:5002")
VSS_TELEMETRY_SERVICE_URL = os.getenv("VSS_TELEMETRY_SERVICE_URL", "http://localhost:8086")
VSS_SIMULATOR_URL         = os.getenv("VSS_SIMULATOR_URL",         "http://localhost:8087")

# ── Piper TTS (binary, fully offline) ────────────────────────────────────────
import subprocess as _subprocess
_PIPER_BIN   = os.getenv("PIPER_BIN",   "/opt/piper/piper")
_PIPER_MODEL = os.getenv("PIPER_MODEL", "/app/en_US-lessac-medium.onnx")
if os.path.exists(_PIPER_BIN) and os.path.exists(_PIPER_MODEL):
    print(f"✅ Piper TTS ready.", flush=True)
else:
    print(f"⚠️  Piper TTS not found (bin={_PIPER_BIN}, model={_PIPER_MODEL}).", flush=True)
    _PIPER_BIN = None

_tts_proc: "_subprocess.Popen | None" = None
_tts_lock = threading.Lock()

_MD_BOLD   = re.compile(r"\*\*(.*?)\*\*")
_MD_ITALIC = re.compile(r"\*(.*?)\*")
_BULLETS   = re.compile(r"[•·]")
_SPACES    = re.compile(r"\s+")

# ── Global state ──────────────────────────────────────────────────────────────
assistant        = None
assistant_thread = None
should_stop      = False
user_location    = {"lat": None, "lon": None}
_sessions        = {}  # sid -> {conversation_id, user_id}


# ── TTS helper ───────────────────────────────────────────────────────────────

def _clean_tts_text(text: str) -> str:
    text = _MD_BOLD.sub(r"\1", text)
    text = _MD_ITALIC.sub(r"\1", text)
    text = _BULLETS.sub(" ", text)
    text = _SPACES.sub(" ", text)
    return text.strip()


def _push_tts(sid: str, text: str) -> None:
    """Synthesise speech with the Piper binary and push WAV to the client.

    Kills any in-progress synthesis before starting a new one so responses
    never overlap.
    """
    global _tts_proc
    if not _PIPER_BIN:
        return
    tmp_path = None
    try:
        clean = _clean_tts_text(text)
        if not clean:
            return

        with _tts_lock:
            # Kill any previous synthesis that is still running.
            if _tts_proc and _tts_proc.poll() is None:
                _tts_proc.kill()
                _tts_proc.wait()

            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                tmp_path = f.name

            # Piper reads text from stdin and writes a WAV to --output_file.
            _tts_proc = _subprocess.Popen(
                [_PIPER_BIN, "--model", _PIPER_MODEL, "--output_file", tmp_path],
                stdin=_subprocess.PIPE,
                stdout=_subprocess.DEVNULL,
                stderr=_subprocess.DEVNULL,
            )
            _tts_proc.communicate(input=clean.encode())

        with open(tmp_path, "rb") as f:
            audio_b64 = base64.b64encode(f.read()).decode("utf-8")
        socketio.emit("audio", {"data": audio_b64, "format": "wav"}, to=sid)
    except Exception as e:
        print(f"[tts] Synthesis failed: {e}", flush=True)
    finally:
        if tmp_path and os.path.exists(tmp_path):
            os.unlink(tmp_path)


# ── Agent helpers ─────────────────────────────────────────────────────────────

def _call_agent(message: str, conversation_id: str, lat=None, lon=None, network_mode: str = "offline") -> dict:
    """Blocking call to the agent service. Used by the voice loop (needs full answer before TTS)."""
    try:
        resp = http_requests.post(
            f"{AGENT_SERVICE_URL}/agent/chat",
            json={
                "message": message,
                "conversation_id": conversation_id,
                "lat": lat,
                "lon": lon,
                "network_mode": network_mode,
            },
            timeout=180,
        )
        if resp.ok:
            return resp.json()
        return {
            "answer": f"Agent error (HTTP {resp.status_code}). Please try again.",
            "tools_used": [],
            "navigation": None,
            "conversation_id": conversation_id,
        }
    except http_requests.exceptions.ConnectionError:
        return {
            "answer": "The agent service is not running. Please check that all containers are up.",
            "tools_used": [],
            "navigation": None,
            "conversation_id": conversation_id,
        }
    except Exception as e:
        return {
            "answer": f"Agent unavailable: {e}",
            "tools_used": [],
            "navigation": None,
            "conversation_id": conversation_id,
        }


def _call_agent_stream(message: str, conversation_id: str, sid: str, lat=None, lon=None, network_mode: str = "offline") -> dict:
    """
    Stream agent response via SSE, forwarding each token to the client over SocketIO.
    Emits `answer_token` events as tokens arrive so the UI renders progressively.
    Returns the final done payload (answer, tools_used, navigation) for TTS and persistence.
    """
    fallback = {"answer": "", "tools_used": [], "navigation": None, "conversation_id": conversation_id}
    try:
        resp = http_requests.post(
            f"{AGENT_SERVICE_URL}/agent/chat/stream",
            json={
                "message": message,
                "conversation_id": conversation_id,
                "lat": lat,
                "lon": lon,
                "network_mode": network_mode,
            },
            stream=True,
            timeout=180,
        )
        resp.raise_for_status()

        for raw_line in resp.iter_lines():
            if not raw_line or not raw_line.startswith(b"data: "):
                continue
            try:
                event = json.loads(raw_line[6:])
            except json.JSONDecodeError:
                continue

            if "token" in event:
                socketio.emit("answer_token", {"text": event["token"]}, to=sid)
            elif "status" in event:
                # Tool-call status update — show while the agent is fetching data
                socketio.emit("agent_status", {"text": event["status"]}, to=sid)
            elif event.get("done"):
                return event
            elif "error" in event:
                fallback["answer"] = f"Agent error: {event['error']}"
                return fallback

        fallback["answer"] = "No response received from agent."
        return fallback

    except http_requests.exceptions.ConnectionError:
        fallback["answer"] = "The agent service is not running. Please check that all containers are up."
        return fallback
    except Exception as e:
        fallback["answer"] = f"Agent unavailable: {e}"
        return fallback


# ── Conversation persistence ──────────────────────────────────────────────────

def _save_message(conversation_id: str, user_id: str, role: str, message: str) -> None:
    try:
        http_requests.post(
            f"{CONVERSATION_SERVICE_URL}/conversations/message",
            json={
                "conversation_id": conversation_id,
                "user_id":         user_id,
                "role":            role,
                "message":         message,
            },
            timeout=2,
        )
    except Exception:
        pass


# ── Routes ────────────────────────────────────────────────────────────────────

@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/vss/latest')
def api_vss_latest():
    """Proxy latest VSS state from the VSS telemetry service."""
    try:
        resp = http_requests.get(f"{VSS_TELEMETRY_SERVICE_URL}/vss/latest", timeout=3)
        if not resp.ok:
            return jsonify({"error": f"HTTP {resp.status_code}"}), resp.status_code
        return jsonify(resp.json())
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS telemetry service unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/api/vss/simulator/start', methods=['POST'])
def api_vss_simulator_start():
    """Proxy simulator start to the VSS simulator."""
    try:
        resp = http_requests.post(f"{VSS_SIMULATOR_URL}/simulator/start", json=request.json or {}, timeout=10)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/api/vss/simulator/stop', methods=['POST'])
def api_vss_simulator_stop():
    """Proxy simulator stop to the VSS simulator."""
    try:
        resp = http_requests.post(f"{VSS_SIMULATOR_URL}/simulator/stop", timeout=10)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/api/vss/simulator/status')
def api_vss_simulator_status():
    """Proxy simulator status from the VSS simulator."""
    try:
        resp = http_requests.get(f"{VSS_SIMULATOR_URL}/simulator/status", timeout=3)
        return jsonify(resp.json()), resp.status_code
    except http_requests.exceptions.ConnectionError:
        return jsonify({"error": "VSS simulator unavailable", "running": False}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/api/navigate', methods=['POST'])
def api_navigate():
    """Proxy direct navigation requests (from the map tab text search)."""
    try:
        resp = http_requests.post(
            f"{NAVIGATION_SERVICE_URL}/navigate",
            json=request.json,
            timeout=60,
        )
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


# ── SocketIO events ───────────────────────────────────────────────────────────

@socketio.on('update_location')
def handle_update_location(data):
    global user_location
    user_location['lat'] = data.get('lat')
    user_location['lon'] = data.get('lon')


@socketio.on('set_network_mode')
def handle_set_network_mode(data):
    mode = data.get('mode', 'offline')
    if mode not in ('online', 'offline'):
        mode = 'offline'
    session = _sessions.get(request.sid)
    if session:
        session['network_mode'] = mode
    emit('network_mode_changed', {'mode': mode})


@socketio.on('connect')
def handle_connect():
    _sessions[request.sid] = {
        'conversation_id': str(uuid.uuid4()),
        'user_id': str(uuid.uuid4()),
        'network_mode': 'offline',
    }
    print("🔌 Client connected")
    emit('status', {'state': 'ready', 'message': 'Type a message or tap the mic to start'})
    try:
        health = http_requests.get(f"{config.SEARCH_SERVICE_URL}/health", timeout=2).json()
        emit('stats', {'search_service': 'online', 'chunk_count': health.get('chunk_count', 0)})
    except Exception:
        emit('stats', {'search_service': 'offline', 'chunk_count': 0})


@socketio.on('disconnect')
def handle_disconnect():
    _sessions.pop(request.sid, None)
    print("🔌 Client disconnected")


@socketio.on('send_message')
def handle_send_message(data):
    message = (data.get('text') or '').strip()
    if not message:
        return

    session = _sessions.get(request.sid, {})
    conversation_id = session.get('conversation_id') or str(uuid.uuid4())
    user_id         = session.get('user_id')         or str(uuid.uuid4())
    network_mode    = session.get('network_mode',    'offline')

    sid = request.sid
    emit('question', {'text': message})
    emit('status', {'state': 'processing', 'message': '🤖 Thinking...'})

    _save_message(conversation_id, user_id, 'user', message)

    # Stream tokens to the client as they arrive; _call_agent_stream emits
    # `answer_token` events per chunk and returns the final done payload.
    agent_result = _call_agent_stream(
        message,
        conversation_id,
        sid,
        user_location['lat'],
        user_location['lon'],
        network_mode,
    )

    answer = agent_result.get('answer', '')
    _save_message(conversation_id, user_id, 'assistant', answer)

    if agent_result.get('navigation'):
        emit('navigation_result', agent_result['navigation'])

    # TTS runs on the clean full answer from the done event.
    threading.Thread(target=_push_tts, args=(sid, answer), daemon=True).start()

    # `answer` finalises the UI state (tools badge, scroll, etc.) and signals
    # the end of the streaming turn. The text is already rendered token-by-token.
    emit('answer', {'text': answer, 'tools_used': agent_result.get('tools_used', [])})
    emit('status', {'state': 'ready', 'message': 'Ready'})


@socketio.on('start_listening')
def handle_start_listening():
    global assistant, assistant_thread, should_stop

    should_stop = False
    if assistant is None:
        assistant = VoiceAssistantWithEvents(socketio)
    assistant.should_stop = False

    if assistant_thread is None or not assistant_thread.is_alive():
        assistant_thread = threading.Thread(target=run_assistant_loop, daemon=True)
        assistant_thread.start()

    emit('status', {'state': 'ready', 'message': 'Ready to listen'})


@socketio.on('stop_listening')
def handle_stop_listening():
    global should_stop, assistant
    should_stop = True
    if assistant:
        assistant.should_stop = True
    emit('status', {'state': 'ready', 'message': 'Click microphone to start (say "thank you" to exit)'})
    emit('session_complete')


# ── Voice loop ────────────────────────────────────────────────────────────────

def run_assistant_loop():
    global assistant, should_stop

    exit_phrases = ['thank you', 'thanks', 'thank', 'bye', 'goodbye',
                    "that's all", 'thats all', 'stop', 'exit', 'quit']

    while not should_stop:
        try:
            socketio.emit('status', {'state': 'listening',
                                     'message': '🎤 Listening... (say "thank you" to exit)'})
            question = assistant.listen()

            if should_stop:
                break
            if not question:
                socketio.sleep(0.5)
                continue

            question_lower = question.lower().strip()

            # ── Exit check ────────────────────────────────────────────────────
            if any(p in question_lower for p in exit_phrases):
                socketio.emit('question', {'text': question})
                assistant.save_message("user", question)
                goodbye = "You're welcome! Have a great drive! 🚗"
                socketio.emit('answer', {'text': goodbye})
                socketio.emit('status', {'state': 'speaking', 'message': '🔊 Saying goodbye...'})
                assistant.speak(goodbye)
                assistant.save_message("assistant", goodbye)
                socketio.sleep(1)
                break

            socketio.emit('question', {'text': question})
            socketio.emit('status', {'state': 'processing', 'message': '🤖 Thinking...'})
            assistant.save_message("user", question)

            if should_stop:
                break

            # ── Call unified agent ────────────────────────────────────────────
            agent_result = _call_agent(
                question,
                assistant.conversation_id,
                user_location['lat'],
                user_location['lon'],
                getattr(assistant, 'network_mode', 'offline'),
            )

            answer = agent_result['answer']

            # Emit navigation route to map if the agent used the navigate tool
            if agent_result.get('navigation'):
                socketio.emit('navigation_result', agent_result['navigation'])

            if should_stop:
                break

            socketio.emit('answer', {'text': answer, 'tools_used': agent_result.get('tools_used', [])})
            socketio.emit('status', {'state': 'speaking', 'message': '🔊 Speaking...'})
            assistant.save_message("assistant", answer)
            assistant.speak(answer)

            socketio.sleep(1)
            socketio.emit('status', {'state': 'ready', 'message': 'Ready for next question'})

        except KeyboardInterrupt:
            break
        except Exception as e:
            socketio.emit('error', {'message': str(e)})
            socketio.sleep(2)

    socketio.emit('session_complete')
    socketio.emit('status', {'state': 'ready',
                              'message': 'Click microphone to start (say "thank you" to exit)'})


# ── Extended voice assistant (adds SocketIO events + conversation persistence) ─

class VoiceAssistantWithEvents(VoiceAssistant):

    def __init__(self, socketio_instance):
        super().__init__()
        self.socketio       = socketio_instance
        self.should_stop    = False
        self.user_id        = str(uuid.uuid4())
        self.conversation_id = str(uuid.uuid4())
        print(f"🆔 Session: user={self.user_id[:8]}... conv={self.conversation_id[:8]}...")

    def save_message(self, role: str, message: str, sources: list = None):
        try:
            payload = {
                "conversation_id": self.conversation_id,
                "user_id":         self.user_id,
                "role":            role,
                "message":         message,
                "sources":         json.dumps(sources) if sources else "",
            }
            http_requests.post(
                f"{CONVERSATION_SERVICE_URL}/conversations/message",
                json=payload,
                timeout=2,
            )
        except Exception:
            pass

    def listen(self) -> Optional[str]:
        import speech_recognition as sr
        with sr.Microphone() as source:
            try:
                self.recognizer.adjust_for_ambient_noise(source, duration=0.3)
                max_wait, elapsed = 10, 0
                while elapsed < max_wait and not self.should_stop:
                    try:
                        audio = self.recognizer.listen(source, timeout=2, phrase_time_limit=10)
                        if self.should_stop:
                            return None
                        return self.recognizer.recognize_whisper(
                            audio,
                            model=config.WHISPER_MODEL,
                            language=config.WHISPER_LANGUAGE,
                        )
                    except sr.WaitTimeoutError:
                        elapsed += 2
                return None
            except Exception as e:
                if not self.should_stop:
                    print(f"Listen error: {e}")
                return None


def main():
    print("=" * 70)
    print("🚗 Car Assistant — Unified AI (manual + telemetry + navigation)")
    print("=" * 70)
    print(f"\n🌐 http://localhost:5000")
    print("💡 Click microphone to start — ask anything about your car\n")
    socketio.run(app, host='0.0.0.0', port=5000, debug=False, allow_unsafe_werkzeug=True)


if __name__ == '__main__':
    main()
