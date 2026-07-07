# Atlas App Services — VSS Telemetry Triggers

Transforms individual ObjectBox-synced entity collections into two unified collections used by the telemetry API.

## How it works

The trigger fires on every `PowertrainState` write (the simulator's heartbeat — always present in every snapshot). When it fires, `assembleTelemetry` reads all six state collections in parallel and writes two unified documents:

- **`telemetry-data`** — inserts a new record on every trigger fire (~every 2 s). Append-only time-series history.
- **`telemetry-status`** — upserts the current state at most once every 10 s. This is what the telemetry API reads.

```
ObjectBox Sync Server
        │
        ▼  (all 6 state entities synced every ~2s)
MongoDB Atlas
  PowertrainSample ◄── trigger watches this collection only
  BatterySample        (function reads latest from each when triggered)
  ChassisSample
  CabinSample
  LocationSample
  AdasSample
  VehicleMeta
        │
        ▼  assembleTelemetry function
        ├──▶  telemetry-data   (INSERT every ~2s)
        └──▶  telemetry-status (UPSERT every ~10s)
```

## Unified document structure

```json
{
  "vehicleId": "VSS-DEMO-VIN-001",
  "timestamp": "<ISO date>",
  "lastUpdated": "<ISO date>",
  "meta": { "vin": "...", "oem": "...", "model": "...", ... },
  "data": {
    "powertrain": { "speedKph": 85.2, "engineRpm": 2400, ... },
    "battery":    { "socPct": 68.3, "estimatedRangeKm": 280, ... },
    "chassis":    { "tirePressureFlKpa": 230, ... },
    "cabin":      { "insideTempC": 21, "hvacMode": "auto", ... },
    "location":   { "locationGeoJson": "...", "headingDeg": 135, ... },
    "adas":       { "cruiseEnabled": true, "collisionWarningActive": false, ... }
  }
}
```

## One-time setup

### 1. Create the target collections

Copy `.env.example` to `.env` and fill in your Atlas credentials, then run:

```powershell
cd atlas-app
npm install
node setup_collections.js
```

This creates:
- `telemetry-data` — native time-series collection (`timeField: "timestamp"`, `metaField: "vehicleId"`)
- `telemetry-status` — standard collection with a unique index on `vehicleId`

> **Important:** time-series collection options cannot be changed after creation. Do this before starting the stack.

---

### 2. Create an App Services application

Atlas UI → **App Services** → **Create a New App**

| Field | Value |
|-------|-------|
| App name | `vss-telemetry-triggers` |
| Linked cluster | your cluster |
| Deployment model | Local |
| Region | closest to your cluster |

---

### 3. Add the database as a data source

If not linked automatically, go to **App Services → your app → Linked Data Sources** and link your Atlas cluster under the name `mongodb-atlas`.

---

### 4. Create the App Services Value

**App Services → your app → Values → Create New Value**

| Field | Value |
|-------|-------|
| Name | `DATABASE_NAME` |
| Type | Value |
| Value | your database name (same as `MONGODB_DATABASE` in `sync-server-setup/.env`) |

---

### 5. Create the function

**App Services → your app → Functions → Create New Function**

| Field | Value |
|-------|-------|
| Name | `assembleTelemetry` |
| Authentication | System |
| Private | No |

Paste the contents of `functions/assembleTelemetry/source.js` as the function body and save.

---

### 6. Create the trigger

**App Services → your app → Triggers → Add a Trigger**

| Field | Value |
|-------|-------|
| Trigger type | Database |
| Name | `PowertrainStateTrigger` |
| Enabled | Yes |
| Cluster | your cluster |
| Database | your database name |
| Collection | `PowertrainSample` |
| Operation type | Insert |
| Full document | On |
| Document preimage | Off |
| Select an event type | Function |
| Function | `assembleTelemetry` |

> The trigger watches only `PowertrainSample` because it is appended on every simulator snapshot and acts as the heartbeat. The function reads the latest sample from each companion collection (`BatterySample`, `ChassisSample`, `CabinSample`, `LocationSample`, `AdasSample`, `VehicleMeta`) in the same invocation — one trigger fire assembles all domains.

Save the trigger. Atlas will deploy the app automatically.
