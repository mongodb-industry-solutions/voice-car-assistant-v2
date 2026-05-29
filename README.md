# Voice Car Assistant v2

A multi-service, fully Dockerised in-vehicle assistant that combines live VSS telemetry, car-manual vector search, voice I/O, and a LangChain ReAct agent — all running locally with MongoDB Atlas as the cloud replica.

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│  Browser  http://localhost:5000                                       │
│  Voice UI (Flask + SocketIO + Whisper STT + Piper TTS)               │
└──────┬───────────────────────────────────────┬────────────────────────┘
       │ chat / agent                           │ conversation history
       │                              ┌─────────▼──────────────┐
       │                              │  conversation-service  │
       │                              │  :8081  ObjectBox      │
       │                              └────────────────────────┘
       │
┌──────▼────────────────────────────────────────┐
│  LangChain ReAct Agent  :5002                  │
│  Ollama (qwen2.5:3b)                             │
│  Tools: car-manual search · navigation ·       │
│         9 VSS MCP tools (online mode)          │
└──┬──────────────┬─────────────────┬────────────┘
   │              │                 │
   │    ┌─────────▼──────┐  ┌───────▼──────────────────┐
   │    │ search-service │  │  vss-telemetry-mcp-server │
   │    │ :8080          │  │  :3002  (Node.js MCP)     │
   │    │ ObjectBox HNSW │  │  reads MongoDB Atlas      │
   │    │ vector search  │  └──────────────────────────-┘
   │    └────────────────┘             ▲
   │                                   │ ObjectBox Sync
   │                        ┌──────────┴──────────────────┐
   │                        │  ObjectBox Sync Server       │
   │                        │  :9999 sync  :9980 admin     │
   │                        │  replicates → MongoDB Atlas  │
   │                        └──────────┬──────────────────┘
   │                                   │
   │                        ┌──────────▼──────────────────┐
   │                        │  vss-telemetry-service :8086 │
   │                        │  C++ ObjectBox, 14 entities  │
   │                        └──────────┬──────────────────┘
   │                                   │ POST /vss/snapshot every 2 s
   │                        ┌──────────▼──────────────────┐
   │                        │  vss-telemetry-simulator     │
   │                        │  :8087  Python               │
   │                        └─────────────────────────────┘
   │
┌──▼────────────────────────┐
│  navigation-service :5001  │
│  Ollama (qwen2.5:3b) + OSRM  │
└────────────────────────────┘
```

## Services

All services are defined in [`sync-server-setup/docker-compose.yml`](sync-server-setup/docker-compose.yml).

| Service | Port | Description |
|---|---|---|
| `voice-assistant-backend` | 5000 | Flask + SocketIO web UI; Whisper STT; Piper TTS |
| `langchain-agent-service` | 5002 | LangChain ReAct agent (Ollama qwen2.5:3b) |
| `navigation-service` | 5001 | LLM-powered navigation + OSRM routing |
| `ollama` | 11434 | Ollama server — serves qwen2.5:3b; GPU-ready |
| `search-service` | 8080 | ObjectBox HNSW vector search on car-manual chunks |
| `mongodb-search-service` | 8085 | MongoDB Atlas vector search (voyage-4-nano) |
| `conversation-service` | 8081 | ObjectBox conversation history |
| `sync-server` | 9980 / 9999 | ObjectBox Sync Server — replicates to MongoDB Atlas |
| `vss-telemetry-service` | 8086 | C++ ObjectBox service — 14 typed VSS entities |
| `vss-telemetry-simulator` | 8087 | Python VSS data generator — auto-starts on launch |
| `vss-telemetry-mcp-server` | 3002 | Node.js MCP server — 9 agent tools, reads MongoDB |

Legacy telemetry stack (`telemetry-service` :8084, `telemetry-simulator` :8082, `telemetry-mcp-server` :3001) is kept for rollback but is not wired to the agent or UI.

## Prerequisites

### 1. Docker Desktop

Install [Docker Desktop](https://www.docker.com/products/docker-desktop/).

### 2. Ollama (containerised — no local install needed)

Ollama runs as a Docker container (`ollama` service, port 11434). On first `docker compose up` it automatically pulls `qwen2.5:3b`. Model weights are stored in the named volume `ollama-models` and survive restarts.

To force a re-pull (e.g. to upgrade the model):
```bash
docker volume rm sync-server-setup_ollama-models
```

To enable NVIDIA GPU acceleration, uncomment the `deploy.resources` block in `docker-compose.yml`.

### 3. MongoDB Atlas

Create a free cluster at [cloud.mongodb.com](https://cloud.mongodb.com) and collect:
- username / password
- cluster hostname (e.g. `cluster0.abc12.mongodb.net`)
- database name

## Setup

### 1. Configure credentials

Copy the example env file and fill in your values:

```bash
cd sync-server-setup
cp .env.example .env
# Edit .env with your MongoDB Atlas credentials
```

`.env` contents:

```
MONGODB_USER=your_username
MONGODB_PASS=your_password
MONGODB_CLUSTER=your_cluster.xxxxx.mongodb.net
MONGODB_DATABASE=your_database_name
```

### 2. Load the car manual into ObjectBox

Run this once to embed the car manual documents into the vector search database:

```bash
cd ..                     # back to repo root
pip install -r requirements.txt
python load_documents.py
```

### 3. Start all services

```bash
cd sync-server-setup
docker compose up
```

Open the **ObjectBox Sync Server admin UI** at [http://localhost:9980](http://localhost:9980) and activate your trial licence when prompted.

Once all containers are healthy, open the voice assistant at **[http://localhost:5000](http://localhost:5000)**.

## Using the Assistant

The web UI is a single-page dashboard with:

- **Left panel** — RPM arc gauge, engine telemetry (coolant, throttle, odometer), battery (SoC, voltage, health, range)
- **Center** — Chat interface, navigation map, mic button, online/offline mode toggle
- **Right panel** — Fuel arc gauge with gear/speed, 4-tyre pressure diagram, chassis (ABS, traction control, brake), ADAS (cruise, lane keep, collision)

All telemetry widgets poll `/api/vss/latest` every 3 seconds from the C++ service.

**Offline mode** — uses ObjectBox vector search + local Ollama only (no MongoDB, no VSS tools).

**Online mode** — adds 9 VSS telemetry MCP tools and MongoDB Atlas search to the agent.

### Example questions

- "What is my current speed and fuel level?"
- "Is the battery charging? What's the estimated range?"
- "Are any doors open?"
- "Are there any active warnings or DTCs?"
- "How has my speed changed over the last 10 minutes?"
- "How do I check the brake fluid?" *(car manual search)*
- "Navigate to the nearest service station" *(navigation)*

## VSS Telemetry Schema

The C++ service stores 14 typed entities (IDs 10–24), all sync-enabled to MongoDB Atlas.

### Metadata
| Entity | ID | Description |
|---|---|---|
| `VehicleMeta` | 10 | VIN, OEM, model, platform, software version, powertrain type, drivetrain type, fuel tank capacity, battery capacity, wheelbase, curb weight |
| `SignalDefinition` | 11 | VSS signal registry (not populated at runtime) |

### State entities — one row per vehicle, upserted on every snapshot
| Entity | ID | Key fields |
|---|---|---|
| `PowertrainState` | 13 | speed, RPM, fuel level %, fuel rate, coolant temp, throttle, gear, ignition, odometer |
| `BatteryState` | 14 | SoC%, SoH%, voltage, current, charging state, charging power, estimated range, battery temp |
| `ChassisState` | 15 | 4-tyre pressures (kPa), steering angle, brake pedal, ABS active, traction control active |
| `CabinState` | 16 | inside/outside temp, HVAC mode, fan speed, door states, locks, seatbelt |
| `LocationState` | 17 | GeoJSON Point (`locationGeoJson`), altitude, heading, GPS speed, geohash |
| `AdasState` | 18 | cruise enabled/set speed, lane keep assist, parking assist, collision warning, autopilot mode |

### Sample entities — append-only history, pruned after 24 h
`PowertrainSample` (19), `BatterySample` (20), `LocationSample` (21), `CabinSample` (22), `AdasSample` (23)

### Events
`VehicleEvent` (24) — DTC/anomaly events with severity, VSS path, code; retained 7 days.

Full field-level documentation: [`vss-data.md`](vss-data.md)

### Location format

`locationGeoJson` is a GeoJSON Point string:
```json
{"type":"Point","coordinates":[longitude, latitude]}
```
Longitude is first per the GeoJSON spec (RFC 7946). MongoDB Atlas geospatial queries work directly on this field.

## VSS Telemetry Service API

```
POST /vss/snapshot              — upsert all State entities + append all Samples
POST /vss/event                 — append a VehicleEvent
POST /vss/meta                  — upsert VehicleMeta (seeded once at startup)
GET  /vss/latest                — full current state (all domains)
GET  /vss/powertrain/history?minutes=N
GET  /vss/battery/history?minutes=N
GET  /vss/location/history?minutes=N
GET  /vss/cabin/history?minutes=N
GET  /vss/adas/history?minutes=N
GET  /vss/events?minutes=N[&severity=warning|critical]
DELETE /vss/prune?older_than_hours=N
GET  /health
```

## MCP Tools (vss-telemetry-mcp-server)

The LangChain agent calls these tools in online mode via `POST /tools/<name>`:

| Tool | Description |
|---|---|
| `get_vehicle_status` | Full snapshot — all domains + vehicle identity |
| `get_powertrain_status` | Engine, speed, fuel, gear |
| `get_battery_status` | SoC, SoH, charging state, range |
| `get_chassis_status` | Tyre pressures, ABS, traction control |
| `get_cabin_status` | Temperature, HVAC, doors, seatbelt |
| `get_location` | GPS position (lat/lon parsed from GeoJSON), heading, geohash |
| `get_adas_status` | Cruise control, lane keep assist, collision warning |
| `get_vehicle_events(minutes, severity)` | Recent DTC/anomaly events |
| `get_driving_history(domain, minutes)` | Time-series samples for any domain |

## Data Flow

```
vss-telemetry-simulator (8087)
  → POST /vss/snapshot every 2 s
    → vss-telemetry-service (8086, ObjectBox C++)   ← UI polls /vss/latest
      → ObjectBox Sync Server (9999)
        → MongoDB Atlas
          → vss-telemetry-mcp-server (3002)          ← LangChain agent (online mode)
```

State writes always read the existing entity first to preserve `syncClock`, so the Sync Server accepts updates rather than reverting them.

## Resetting the VSS Database

If the ObjectBox store fails to open after a schema change (e.g. on first run after removing entities):

```bash
# Stop services
docker compose down

# Delete the on-disk store
Remove-Item -Recurse -Force sync-server-setup/vss-telemetry-data

# Restart — service initialises a fresh store
docker compose up
```

## Troubleshooting

**Sync Server fails with "Invalid JSON"**
Check `sync-server-setup/objectbox-model.json` for trailing commas — JSON does not allow them.

**C++ service: "Can not open store" on startup**
Schema mismatch with the on-disk database. Delete `sync-server-setup/vss-telemetry-data/` and restart.

**vss-telemetry-simulator shows as Docker unhealthy**
False alarm — the `python:3.11-slim` base image lacks `wget`. Verify with `curl http://localhost:8087/health`.

**Ollama connection error from containers**
Check that the `ollama` container is healthy: `docker compose ps ollama`. If it shows unhealthy, the model may still be pulling — wait and retry.

## License

Apache 2.0
