"""
Telemetry Simulator Service
Flask-based HTTP API for generating realistic automotive telemetry data.
"""

from flask import Flask, jsonify, request
from flask_cors import CORS
from flask_socketio import SocketIO, emit
import threading
import time
import requests
from telemetry_generator import TelemetryGenerator

app = Flask(__name__)
CORS(app, resources={r"/*": {"origins": "*"}})

# Initialize SocketIO with CORS support and eventlet for WebSocket
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

# Global state
simulator_state = {
    "running": False,
    "generator": TelemetryGenerator(),
    "update_interval": 2.0,  # seconds
    "telemetry_service_url": "http://telemetry-service:8084/telemetry",
    "snapshots_generated": 0,
    "last_snapshot": None
}

simulation_thread = None


def simulation_loop():
    """Background thread that generates and sends telemetry data."""
    print("🚗 Simulation loop started")
    
    while simulator_state["running"]:
        try:
            # Generate snapshot
            snapshot = simulator_state["generator"].generate_snapshot()
            simulator_state["last_snapshot"] = snapshot
            simulator_state["snapshots_generated"] += 1
            
            # Send to telemetry service (if available)
            try:
                response = requests.post(
                    simulator_state["telemetry_service_url"],
                    json=snapshot,
                    timeout=2.0
                )
                if response.status_code == 200:
                    print(f"✓ Snapshot #{simulator_state['snapshots_generated']} sent "
                          f"(anomalies: {snapshot['anomaly_count']})")
                else:
                    print(f"⚠ Telemetry service returned {response.status_code}")
            except requests.exceptions.RequestException as e:
                print(f"⚠ Could not reach telemetry service: {e}")
                # Continue simulation even if service is unavailable
            
            # Broadcast to WebSocket clients
            try:
                socketio.emit('telemetry_update', snapshot, namespace='/')
            except Exception as e:
                print(f"⚠ WebSocket broadcast error: {e}")
            
            # Wait for next update
            time.sleep(simulator_state["update_interval"])
            
        except Exception as e:
            print(f"❌ Error in simulation loop: {e}")
            time.sleep(1)
    
    print("🛑 Simulation loop stopped")


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint."""
    return jsonify({
        "status": "healthy",
        "service": "telemetry-simulator",
        "simulation_running": simulator_state["running"],
        "snapshots_generated": simulator_state["snapshots_generated"]
    })


@app.route('/simulator/start', methods=['POST'])
def start_simulation():
    """Start the telemetry simulation."""
    global simulation_thread
    
    if simulator_state["running"]:
        return jsonify({
            "success": False,
            "message": "Simulation is already running"
        }), 400
    
    # Reset generator for fresh start
    simulator_state["generator"] = TelemetryGenerator()
    simulator_state["running"] = True
    simulator_state["snapshots_generated"] = 0
    
    # Start background thread
    simulation_thread = threading.Thread(target=simulation_loop, daemon=True)
    simulation_thread.start()
    
    return jsonify({
        "success": True,
        "message": "Simulation started",
        "update_interval": simulator_state["update_interval"],
        "anomaly_probability": simulator_state["generator"].anomaly_probability
    })


@app.route('/simulator/stop', methods=['POST'])
def stop_simulation():
    """Stop the telemetry simulation."""
    if not simulator_state["running"]:
        return jsonify({
            "success": False,
            "message": "Simulation is not running"
        }), 400
    
    simulator_state["running"] = False
    
    # Wait for thread to finish (max 5 seconds)
    if simulation_thread and simulation_thread.is_alive():
        simulation_thread.join(timeout=5.0)
    
    return jsonify({
        "success": True,
        "message": "Simulation stopped",
        "snapshots_generated": simulator_state["snapshots_generated"]
    })


@app.route('/simulator/status', methods=['GET'])
def get_status():
    """Get current simulation status."""
    return jsonify({
        "running": simulator_state["running"],
        "update_interval": simulator_state["update_interval"],
        "anomaly_probability": simulator_state["generator"].anomaly_probability,
        "snapshots_generated": simulator_state["snapshots_generated"],
        "telemetry_service_url": simulator_state["telemetry_service_url"],
        "last_snapshot": simulator_state["last_snapshot"]
    })


@app.route('/simulator/config', methods=['POST'])
def configure_simulation():
    """Configure simulation parameters."""
    data = request.get_json()
    
    if not data:
        return jsonify({"success": False, "message": "No configuration provided"}), 400
    
    updated = []
    
    # Update interval (1-10 seconds)
    if "update_interval" in data:
        interval = float(data["update_interval"])
        if 0.5 <= interval <= 10.0:
            simulator_state["update_interval"] = interval
            updated.append(f"update_interval={interval}s")
        else:
            return jsonify({
                "success": False,
                "message": "update_interval must be between 0.5 and 10 seconds"
            }), 400
    
    # Anomaly probability (0-100%)
    if "anomaly_probability" in data:
        prob = float(data["anomaly_probability"])
        if 0 <= prob <= 100:
            simulator_state["generator"].set_anomaly_probability(prob / 100)
            updated.append(f"anomaly_probability={prob}%")
        else:
            return jsonify({
                "success": False,
                "message": "anomaly_probability must be between 0 and 100"
            }), 400
    
    # Telemetry service URL
    if "telemetry_service_url" in data:
        simulator_state["telemetry_service_url"] = data["telemetry_service_url"]
        updated.append(f"telemetry_service_url={data['telemetry_service_url']}")
    
    return jsonify({
        "success": True,
        "message": f"Configuration updated: {', '.join(updated)}",
        "current_config": {
            "update_interval": simulator_state["update_interval"],
            "anomaly_probability": simulator_state["generator"].anomaly_probability * 100,
            "telemetry_service_url": simulator_state["telemetry_service_url"]
        }
    })


@app.route('/simulator/snapshot', methods=['GET'])
def get_snapshot():
    """Get a single telemetry snapshot without storing it."""
    snapshot = simulator_state["generator"].generate_snapshot()
    return jsonify(snapshot)


@socketio.on('connect')
def handle_connect():
    """Handle WebSocket client connection."""
    print(f"🔌 WebSocket client connected")
    # Send current status immediately on connect
    if simulator_state["last_snapshot"]:
        emit('telemetry_update', simulator_state["last_snapshot"])


@socketio.on('disconnect')
def handle_disconnect():
    """Handle WebSocket client disconnection."""
    print(f"🔌 WebSocket client disconnected")


if __name__ == '__main__':
    print("=" * 60)
    print("🚗 Telemetry Simulator Service")
    print("=" * 60)
    print(f"Update interval: {simulator_state['update_interval']}s")
    print(f"Anomaly probability: {simulator_state['generator'].anomaly_probability * 100}%")
    print(f"Target service: {simulator_state['telemetry_service_url']}")
    print("=" * 60)
    print("\n📡 Starting HTTP + WebSocket server on port 8082...\n")

    # Auto-start simulation on container launch
    simulator_state["running"] = True
    simulation_thread = threading.Thread(target=simulation_loop, daemon=True)
    simulation_thread.start()
    print("▶️  Simulation auto-started.\n")

    socketio.run(app, host='0.0.0.0', port=8082, debug=False, allow_unsafe_werkzeug=True)
