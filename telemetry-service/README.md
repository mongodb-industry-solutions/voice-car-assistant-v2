# Telemetry Service

ObjectBox-based telemetry storage service with automatic sync to MongoDB Atlas.

## Architecture

```
┌──────────────────┐     HTTP POST      ┌─────────────────┐
│ Telemetry        │ ─────────────────> │  Telemetry      │
│ Simulator        │   /telemetry       │  Service        │
│ (Python/Flask)   │                    │  (C++/ObjectBox)│
└──────────────────┘                    └────────┬────────┘
                                                 │
                                                 │ ObjectBox Sync
                                                 v
                                        ┌─────────────────┐
                                        │  Sync Server    │
                                        │  (MongoDB)      │
                                        └─────────────────┘
                                                 │
                                                 v
                                        ┌─────────────────┐
                                        │ MongoDB Atlas   │
                                        │  Database       │
                                        └─────────────────┘
```

## Features

- **Real-time Storage**: Receives telemetry snapshots from simulator
- **ObjectBox Database**: Fast, embedded database optimized for edge devices
- **Automatic Sync**: Changes automatically replicate to MongoDB Atlas
- **Time-based Queries**: Retrieve telemetry by time range
- **RESTful HTTP API**: JSON-based endpoints
- **Health Monitoring**: Built-in health check endpoint

## Data Model

### TelemetrySnapshot Entity

| Field | Type | Description |
|-------|------|-------------|
| id | Long | Auto-generated ID |
| timestamp | Long | Unix timestamp (milliseconds) |
| vehicle_id | String | VIN identifier |
| telemetry_json | String | Full JSON snapshot from simulator |
| anomaly_count | Int | Number of sensors in warning/critical state |
| syncClock | Long | Managed by ObjectBox Sync |

## API Endpoints

### POST /telemetry
Save a telemetry snapshot from the simulator.

**Request Body:**
```json
{
  "timestamp": 1744704123456,
  "vehicle_id": "VIN12345678901234",
  "driving_mode": "idle",
  "anomaly_count": 0,
  "telemetry_batch": {
    "engine": { ... },
    "battery": { ... },
    ...
  }
}
```

**Response:**
```json
{
  "success": true,
  "id": 1,
  "timestamp": 1744704123456,
  "anomaly_count": 0
}
```

### GET /telemetry/latest
Get the most recent telemetry snapshot.

**Response:**
```json
{
  "timestamp": 1744704123456,
  "vehicle_id": "VIN12345678901234",
  "telemetry_batch": { ... }
}
```

### GET /telemetry/range?start=<ts>&end=<ts>
Get telemetry snapshots within a time range.

**Parameters:**
- `start`: Unix timestamp (milliseconds)
- `end`: Unix timestamp (milliseconds)

**Response:**
```json
{
  "count": 10,
  "snapshots": [
    { ... },
    { ... }
  ]
}
```

### GET /telemetry/stats
Get telemetry database statistics.

**Response:**
```json
{
  "total_snapshots": 150,
  "has_data": true,
  "latest_timestamp": 1744704123456,
  "latest_anomaly_count": 1
}
```

### GET /health
Health check endpoint.

**Response:**
```json
{
  "status": "healthy",
  "snapshot_count": 150,
  "service": "telemetry-service"
}
```

## Configuration

Environment variables:
- Database path: `/app/telemetry-db`
- Sync server: `ws://sync-server:9999`
- HTTP port: `8084`

## Sync Implementation Details

### Critical Patterns (Learned from conversation-service)

**1. Sync Client Lifetime Management**
```cpp
// CORRECT: Keep sync client alive with shared_ptr member
std::shared_ptr<obx::SyncClient> syncClient;
syncClient = obx::Sync::client(*store, url, credentials);
syncClient->start();

// WRONG: Local variable will be destroyed
{
    auto sync = obx::Sync::client(...);  // Dies when scope ends!
}
```

**2. Entity Sync Flags**
```cpp
// CRITICAL: Enable sync for telemetry_snapshots entity
obx_model_entity(model, "telemetry_snapshots", 4, ...);
obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);  // ✅ Required!
```

**3. Sync Clock Property**
```cpp
// Every sync-enabled entity MUST have a syncClock property
obx_model_property(model, "syncClock", OBXPropertyType_Long, 6, ...);
```

### Sync Behavior

- **Automatic**: All `PUT` operations automatically trigger sync
- **Bi-directional**: Can receive updates from other clients (future)
- **Conflict Resolution**: Last-write-wins by default
- **Persistence**: Data persists locally even if sync server is down

## Building

### Docker Build
```bash
docker build -t telemetry-service .
```

### Local Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./telemetry_service /path/to/db ws://sync-server:9999 true
```

## Testing

### 1. Start the service
```bash
docker run -p 8084:8084 telemetry-service
```

### 2. Check health
```bash
curl http://localhost:8084/health
```

### 3. Send test telemetry
```bash
curl -X POST http://localhost:8084/telemetry \
  -H "Content-Type: application/json" \
  -d '{
    "timestamp": 1744704123456,
    "vehicle_id": "VIN12345678901234",
    "driving_mode": "idle",
    "anomaly_count": 0,
    "telemetry_batch": {
      "engine": {
        "oil_pressure": {"value": 35.0, "unit": "psi", "status": "normal"}
      }
    }
  }'
```

### 4. Get latest snapshot
```bash
curl http://localhost:8084/telemetry/latest
```

### 5. Get statistics
```bash
curl http://localhost:8084/telemetry/stats
```

## Integration with Simulator

The telemetry simulator (`telemetry-simulator`) automatically sends snapshots to this service:

```python
# In simulator.py
response = requests.post(
    "http://telemetry-service:8084/telemetry",
    json=snapshot
)
```

If the service is unavailable, the simulator gracefully continues without errors.

## MongoDB Atlas Sync

Data sent to this service will automatically appear in MongoDB Atlas:

1. **Database**: ObjectBox Sync Server configuration
2. **Collection**: `telemetry_snapshots`
3. **Documents**: JSON representation of ObjectBox entities

### Viewing Synced Data

**MongoDB Shell:**
```javascript
db.telemetry_snapshots.find().sort({timestamp: -1}).limit(10)
```

**Sync Server Admin UI:**
Navigate to `http://localhost:8090` (if admin UI is enabled)

## Dependencies

- **ObjectBox C/C++**: v5.1.0 (with sync support)
- **cpp-httplib**: v0.14.3 (HTTP server)
- **nlohmann/json**: v3.11.2 (JSON parsing)
- **CMake**: 3.15+
- **C++ Compiler**: C++17 support required

## Troubleshooting

### Sync not working
1. Check sync client is kept alive (not destroyed after scope)
2. Verify `OBXEntityFlags_SYNC_ENABLED` is set
3. Ensure fresh database (can't enable sync on existing DB)
4. Check sync server is running: `docker ps | grep sync-server`

### No data in MongoDB
1. Wait 2-3 seconds after first PUT (sync needs time to establish)
2. Check sync server logs: `docker logs sync-server`
3. Verify MongoDB connection in sync server config

### Service crashes
1. Check logs: `docker logs telemetry-service`
2. Verify libobjectbox.so is in LD_LIBRARY_PATH
3. Ensure data directory has write permissions

## Files

```
telemetry-service/
├── telemetry_service.cpp   # Main C++ HTTP server
├── schema.fbs              # FlatBuffers schema definition
├── schema.obx.hpp          # Generated ObjectBox bindings (auto)
├── schema.obx.cpp          # Generated ObjectBox bindings (auto)
├── CMakeLists.txt          # Build configuration
├── Dockerfile              # Container build
└── README.md               # This file
```

## Next Steps

- **Part 3**: UI Dashboard integration (React components)
- **Part 4**: WebSocket for live updates
- **Part 5**: MCP service for natural language queries
