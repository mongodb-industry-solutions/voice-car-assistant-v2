"""
Flask + SocketIO VSS telemetry simulator.
Generates vehicle snapshots on a background thread, POSTs them to the
telemetry service, and broadcasts them over WebSocket.
"""

import threading
import time
import os
import re
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

# vehicle_id is caller-controlled and becomes a dict key + thread name. Allowlist it
# (length + safe chars) so odd input can't bloat memory, spawn many sims, or garble logs.
_VEHICLE_ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,64}$")


# Vehicle metadata now rides inside every snapshot (see vss_generator.META) as a
# sibling of the domain blocks — no separate /vss/meta seeding is needed.

# ------------------------------------------------------------------ #
#  Per-vehicle simulators                                             #
# ------------------------------------------------------------------ #
# Each browser session drives its own vehicleId, so sessions get independent
# telemetry streams. A _Sim owns one generator + background thread and POSTs
# snapshots stamped with its vehicleId. Callers that send no vehicleId fall back
# to the default VEHICLE_ID, preserving single-vehicle behaviour.

class _Sim:
    def __init__(self, vehicle_id: str, interval_s: float):
        self.vehicle_id = vehicle_id
        self.interval_s = interval_s
        self.generator = VssGenerator()
        self.count = 0
        self.last_snapshot = None
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._lock = threading.Lock()

    def start(self) -> bool:
        """Start the loop. Returns False if it was already running."""
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return False
            self._stop.clear()
            self._thread = threading.Thread(
                target=self._loop, daemon=True, name=f"vss-sim-{self.vehicle_id}"
            )
            self._thread.start()
            return True

    def stop(self) -> bool:
        """Stop the loop. Returns False if it was not running."""
        with self._lock:
            t = self._thread
            if t is None:
                return False
            # Set the stop flag and join while still holding the lock, so a concurrent
            # start() cannot swap in a new thread (and clear the flag) mid-stop. The loop
            # never takes this lock, so joining under it can't deadlock.
            self._stop.set()
            t.join(timeout=5.0)
            self._thread = None
            return True

    def running(self) -> bool:
        t = self._thread
        return t is not None and t.is_alive()

    def _loop(self):
        while not self._stop.is_set():
            snapshot = self.generator.generate_snapshot()
            snapshot["vehicle_id"] = self.vehicle_id  # stamp this session's vehicle
            try:
                http_requests.post(
                    f"{VSS_TELEMETRY_SERVICE_URL}/vss/snapshot",
                    json=snapshot,
                    timeout=3.0,
                )
            except Exception:
                pass  # telemetry service may not be available; continue regardless
            socketio.emit("vss_snapshot", snapshot)
            self.count += 1
            self.last_snapshot = snapshot
            self._stop.wait(self.interval_s)


_sims: dict[str, _Sim] = {}
_sims_lock = threading.Lock()


def _resolve_vehicle_id(payload=None) -> str:
    payload = payload or {}
    raw = payload.get("vehicle_id") or payload.get("vehicleId") or request.args.get("vehicleId") or ""
    # str() coerces non-string JSON values (dict/number) so .strip() and the regex are safe.
    vid = str(raw).strip()
    return vid if _VEHICLE_ID_RE.match(vid) else VEHICLE_ID


def _get_or_create(vehicle_id: str) -> _Sim:
    with _sims_lock:
        sim = _sims.get(vehicle_id)
        if sim is None:
            sim = _Sim(vehicle_id, INTERVAL_S)
            _sims[vehicle_id] = sim
        return sim


# ------------------------------------------------------------------ #
#  Endpoints                                                           #
# ------------------------------------------------------------------ #

@app.route("/health", methods=["GET"])
def health():
    with _sims_lock:
        vehicles = len(_sims)
        running = sum(1 for s in _sims.values() if s.running())
        count = sum(s.count for s in _sims.values())
    return jsonify({"status": "healthy", "vehicles": vehicles, "running": running, "count": count})


@app.route("/simulator/status", methods=["GET"])
def simulator_status():
    vid = _resolve_vehicle_id()
    with _sims_lock:
        sim = _sims.get(vid)
    if sim is None:
        return jsonify({
            "vehicle_id": vid, "running": False, "count": 0,
            "interval_s": INTERVAL_S, "last_snapshot_summary": None,
        })

    last = sim.last_snapshot
    summary = None
    if last:
        # data is the full VSS tree (exact VSS paths); surface a few top-level signals.
        summary = {
            "vehicle_id": last.get("vehicle_id"),
            "ts": last.get("ts"),
            "trip_id": last.get("trip_id"),
            "speed": last.get("Speed"),
            "isMoving": last.get("IsMoving"),
            "traveledDistance": last.get("TraveledDistance"),
        }

    return jsonify({
        "vehicle_id": vid,
        "running": sim.running(),
        "count": sim.count,
        "interval_s": sim.interval_s,
        "last_snapshot_summary": summary,
    })


@app.route("/simulator/start", methods=["POST"])
def simulator_start():
    vid = _resolve_vehicle_id(request.get_json(silent=True))
    sim = _get_or_create(vid)
    if not sim.start():
        return jsonify({"status": "already_running", "vehicle_id": vid, "count": sim.count}), 200
    return jsonify({"status": "started", "vehicle_id": vid}), 200


@app.route("/simulator/stop", methods=["POST"])
def simulator_stop():
    vid = _resolve_vehicle_id(request.get_json(silent=True))
    with _sims_lock:
        sim = _sims.get(vid)
    if sim is None or not sim.stop():
        return jsonify({"status": "not_running", "vehicle_id": vid}), 200
    return jsonify({"status": "stopped", "vehicle_id": vid}), 200


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

    vid = _resolve_vehicle_id(data)
    _get_or_create(vid).interval_s = interval_s
    return jsonify({"status": "updated", "vehicle_id": vid, "interval_s": interval_s}), 200


@app.route("/simulator/snapshot", methods=["GET"])
def simulator_snapshot():
    vid = _resolve_vehicle_id()
    snapshot = _get_or_create(vid).generator.generate_snapshot()
    snapshot["vehicle_id"] = vid
    return jsonify(snapshot), 200


# ------------------------------------------------------------------ #
#  Entry point                                                         #
# ------------------------------------------------------------------ #

if __name__ == "__main__":
    # Sessions start their own vehicle on demand; no global auto-start so abandoned
    # streams do not accumulate. Set AUTOSTART_DEFAULT=1 to stream the default vehicle.
    if os.getenv("AUTOSTART_DEFAULT") == "1":
        _get_or_create(VEHICLE_ID).start()

    socketio.run(
        app,
        host="0.0.0.0",
        port=8087,
        debug=False,
        allow_unsafe_werkzeug=True,
    )
