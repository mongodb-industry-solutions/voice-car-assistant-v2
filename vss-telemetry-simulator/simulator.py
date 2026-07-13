"""
Flask + SocketIO VSS telemetry simulator.
Generates vehicle snapshots on a background thread, POSTs them to the
telemetry service, and broadcasts them over WebSocket.
"""

import threading
import time
import os
import requests as http_requests

from flask import Flask, jsonify, request
from flask_socketio import SocketIO
from flask_cors import CORS

from vss_generator import VssGenerator

app = Flask(__name__)
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

VSS_TELEMETRY_SERVICE_URL = os.getenv(
    "VSS_TELEMETRY_SERVICE_URL", "http://localhost:8086"
)
VEHICLE_ID = os.getenv("VEHICLE_ID", "VSS-DEMO-VIN-001")
INTERVAL_S = float(os.getenv("SIMULATOR_INTERVAL", "2.0"))


# Vehicle metadata now rides inside every snapshot (see vss_generator.META) as a
# sibling of the domain blocks — no separate /vss/meta seeding is needed.

# ------------------------------------------------------------------ #
#  Shared state                                                        #
# ------------------------------------------------------------------ #

_lock = threading.Lock()
_state = {
    "running": False,
    "count": 0,
    "interval_s": INTERVAL_S,
    "last_snapshot": None,
}
_generator = VssGenerator()
_thread: threading.Thread | None = None
_stop_event = threading.Event()


# ------------------------------------------------------------------ #
#  Background worker                                                   #
# ------------------------------------------------------------------ #

def _background_loop():
    while not _stop_event.is_set():
        with _lock:
            interval = _state["interval_s"]

        snapshot = _generator.generate_snapshot()

        # POST to telemetry service
        try:
            http_requests.post(
                f"{VSS_TELEMETRY_SERVICE_URL}/vss/snapshot",
                json=snapshot,
                timeout=3.0,
            )
        except Exception:
            pass  # telemetry service may not be available; continue regardless

        # Emit over WebSocket
        socketio.emit("vss_snapshot", snapshot)

        with _lock:
            _state["count"] += 1
            _state["last_snapshot"] = snapshot

        _stop_event.wait(interval)


def _start_thread():
    global _thread
    _stop_event.clear()
    _thread = threading.Thread(target=_background_loop, daemon=True, name="vss-sim")
    _thread.start()


# ------------------------------------------------------------------ #
#  Endpoints                                                           #
# ------------------------------------------------------------------ #

@app.route("/health", methods=["GET"])
def health():
    with _lock:
        running = _state["running"]
        count = _state["count"]
    return jsonify({"status": "healthy", "running": running, "count": count})


@app.route("/simulator/status", methods=["GET"])
def simulator_status():
    with _lock:
        s = dict(_state)

    last = s.get("last_snapshot")
    summary = None
    if last:
        summary = {
            "vehicle_id": last.get("vehicle_id"),
            "ts": last.get("ts"),
            "trip_id": last.get("trip_id"),
            "speedKph": last.get("powertrain", {}).get("speedKph"),
            "socPct": last.get("battery", {}).get("socPct"),
            "fuelLevelPct": last.get("powertrain", {}).get("fuelLevelPct"),
        }

    return jsonify({
        "running": s["running"],
        "count": s["count"],
        "interval_s": s["interval_s"],
        "last_snapshot_summary": summary,
    })


@app.route("/simulator/start", methods=["POST"])
def simulator_start():
    global _thread
    with _lock:
        if _state["running"]:
            return jsonify({"status": "already_running", "count": _state["count"]}), 200
        _state["running"] = True

    _start_thread()
    return jsonify({"status": "started"}), 200


@app.route("/simulator/stop", methods=["POST"])
def simulator_stop():
    global _thread
    with _lock:
        if not _state["running"]:
            return jsonify({"status": "not_running"}), 200
        _state["running"] = False

    _stop_event.set()
    if _thread is not None:
        _thread.join(timeout=5.0)
        _thread = None

    return jsonify({"status": "stopped"}), 200


@app.route("/simulator/config", methods=["POST"])
def simulator_config():
    data = request.get_json(silent=True) or {}
    interval_s = data.get("interval_s")

    if interval_s is None:
        return jsonify({"error": "Missing 'interval_s' field"}), 400

    try:
        interval_s = float(interval_s)
    except (TypeError, ValueError):
        return jsonify({"error": "'interval_s' must be a number"}), 400

    if interval_s <= 0:
        return jsonify({"error": "'interval_s' must be positive"}), 400

    with _lock:
        _state["interval_s"] = interval_s

    return jsonify({"status": "updated", "interval_s": interval_s}), 200


@app.route("/simulator/snapshot", methods=["GET"])
def simulator_snapshot():
    snapshot = _generator.generate_snapshot()
    return jsonify(snapshot), 200


# ------------------------------------------------------------------ #
#  Entry point                                                         #
# ------------------------------------------------------------------ #

if __name__ == "__main__":
    # Auto-start the background simulator on launch
    with _lock:
        _state["running"] = True
    _start_thread()

    socketio.run(
        app,
        host="0.0.0.0",
        port=8087,
        debug=False,
        allow_unsafe_werkzeug=True,
    )
