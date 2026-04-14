# Telemetry Simulator Service

Realistic automotive telemetry data generator for the car voice assistant demo. Simulates sensor readings from various car systems with configurable anomalies.

## Features

- **Realistic sensor simulation** based on OBD-II/CAN bus standards
- **Start/Stop control** via HTTP API
- **Configurable anomaly injection** (0-100% probability)
- **Multiple car systems:**
  - Engine (oil pressure, oil level, coolant temp, RPM)
  - Fuel system (level, pressure)
  - Battery (voltage, SOC, health)
  - Tires (pressure, temperature for all 4 wheels)
  - Transmission (oil temp, gear)
  - Brakes (fluid level, pad wear)
- **Gradual value changes** simulating real-world sensor drift
- **Correlated sensors** (e.g., RPM affects oil pressure and temperatures)
- **Driving modes** (idle, city, highway) affecting sensor readings

## Quick Start

### Build and Run (Docker)

```bash
# Build
docker build -t telemetry-simulator .

# Run standalone
docker run -p 8082:8082 telemetry-simulator

# Or with custom config
docker run -p 8082:8082 \
  -e TELEMETRY_SERVICE_URL=http://telemetry-service:8084/telemetry \
  telemetry-simulator
```

### Run Locally (Python)

```bash
# Install dependencies
pip install -r requirements.txt

# Run
python simulator.py
```

## API Endpoints

### POST /simulator/start
Start the telemetry simulation.

**Response:**
```json
{
  "success": true,
  "message": "Simulation started",
  "update_interval": 2.0,
  "anomaly_probability": 0.2
}
```

### POST /simulator/stop
Stop the telemetry simulation.

**Response:**
```json
{
  "success": true,
  "message": "Simulation stopped",
  "snapshots_generated": 142
}
```

### GET /simulator/status
Get current simulation status and last snapshot.

**Response:**
```json
{
  "running": true,
  "update_interval": 2.0,
  "anomaly_probability": 0.2,
  "snapshots_generated": 42,
  "last_snapshot": { ... }
}
```

### POST /simulator/config
Configure simulation parameters.

**Request:**
```json
{
  "update_interval": 3.0,
  "anomaly_probability": 30,
  "telemetry_service_url": "http://telemetry-service:8084/telemetry"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Configuration updated",
  "current_config": { ... }
}
```

### GET /simulator/snapshot
Get a single telemetry snapshot (without storing).

**Response:**
```json
{
  "timestamp": 1744704123456,
  "vehicle_id": "VIN12345678901234",
  "driving_mode": "city",
  "anomaly_count": 2,
  "telemetry_batch": {
    "engine": {
      "oil_pressure": {
        "value": 35.2,
        "unit": "psi",
        "status": "normal",
        "pid": "0x0A"
      },
      ...
    },
    ...
  }
}
```

### GET /health
Health check endpoint.

## Telemetry Data Format

Each snapshot includes:

- **timestamp**: Unix timestamp (milliseconds)
- **vehicle_id**: VIN number
- **driving_mode**: Current mode (idle/city/highway)
- **anomaly_count**: Number of sensors with warnings/critical status
- **telemetry_batch**: All sensor readings organized by system

Each sensor reading includes:
- **value**: Current value
- **unit**: Measurement unit
- **status**: "normal", "warning", or "critical"
- **pid**: OBD-II Parameter ID (when applicable)

## Status Thresholds

Sensors are automatically classified based on thresholds defined in `thresholds.py`:

- **Normal**: Within acceptable operating range
- **Warning**: Outside normal range but not critical (yellow icon in UI)
- **Critical**: Dangerous value requiring immediate attention (red icon in UI)

Example thresholds:
```python
"oil_pressure": {
    "min": 20,           # Warning below this
    "max": 60,           # Warning above this
    "critical_min": 10,  # Critical below this
    "critical_max": 80   # Critical above this
}
```

## Anomaly Simulation

Anomalies are injected with configurable probability:

- **Low oil pressure** (8-15 psi instead of 20-60 psi)
- **High coolant temperature** (108-120°C instead of 80-105°C)
- **Low oil level** (5-18% instead of >20%)
- **Low fuel** (3-12% instead of >15%)
- **Low battery** (11.2-11.7V and 8-18% SOC)
- **Low tire pressure** (22-27 psi instead of 28-38 psi)

Each anomaly has an independent probability based on the configured anomaly rate.

## Sensor Correlations

Realistic correlations between sensors:

- **RPM ↑ → Oil pressure ↑** (higher engine speed = higher pressure)
- **RPM ↑ → Coolant temp ↑** (more heat generated)
- **RPM ↑ → Transmission temp ↑** (more friction)
- **RPM ↑ → Tire temp ↑** (speed generates heat)
- **Driving time → Fuel level ↓** (consumption)
- **Battery SOC ↓ → Voltage ↓** (discharge)

## Testing

```bash
# Check health
curl http://localhost:8082/health

# Start simulation
curl -X POST http://localhost:8082/simulator/start

# Get status
curl http://localhost:8082/simulator/status

# Configure higher anomaly rate
curl -X POST http://localhost:8082/simulator/config \
  -H "Content-Type: application/json" \
  -d '{"anomaly_probability": 50}'

# Get single snapshot
curl http://localhost:8082/simulator/snapshot

# Stop simulation
curl -X POST http://localhost:8082/simulator/stop
```

## Integration

The simulator automatically sends telemetry to the configured telemetry service URL. If the service is unavailable, it continues generating data locally without failing.

Default target: `http://telemetry-service:8084/telemetry`

## Configuration

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| update_interval | 2.0 | 0.5-10.0 | Seconds between snapshots |
| anomaly_probability | 20 | 0-100 | Percentage chance of anomaly |
| telemetry_service_url | http://telemetry-service:8084/telemetry | URL | Target service |

## Logs

Monitor simulation activity:
```bash
docker logs telemetry-simulator -f
```

Expected output:
```
🚗 Telemetry Simulator Service
Update interval: 2.0s
Anomaly probability: 20.0%
📡 Starting HTTP server on port 8082...

🚗 Simulation loop started
✓ Snapshot #1 sent (anomalies: 0)
✓ Snapshot #2 sent (anomalies: 1)
✓ Snapshot #3 sent (anomalies: 0)
```

## Architecture

```
Telemetry Simulator (:8082)
    ↓
telemetry_generator.py (data generation)
    ↓
thresholds.py (status classification)
    ↓
simulator.py (HTTP API + background thread)
    ↓
POST → Telemetry Service (:8084)
```

## License

Part of ObjectBox Automotive Demo project.
