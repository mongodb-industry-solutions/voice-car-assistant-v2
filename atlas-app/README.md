# Atlas App Services — VSS Telemetry Triggers

Converts each raw `objectbox_telemetry` snapshot into two app-facing collections used by the telemetry API.

## How it works

The vss-telemetry-service stores every snapshot as one `objectbox_telemetry` document with two `JsonToNative` fields — `data` (the full VSS `Vehicle` tree, exact VSS paths) and `meta` (vehicle metadata) — which the MongoDB connector expands into nested sibling documents. The trigger fires on every `objectbox_telemetry` **insert** (~every 2 s); `assembleTelemetry` passes the VSS tree through unchanged (only enriching `CurrentLocation` with a GeoJSON Point) and writes two unified documents:

- **`telemetry-data`** — inserts a new record on every trigger fire (~every 2 s). Append-only time-series history.
- **`telemetry-status`** — upserts the current state at most once every 10 s. This is what the telemetry API reads.

```
ObjectBox Sync Server
        │
        ▼  (one objectbox_telemetry doc per snapshot, ~every 2s)
MongoDB Atlas
  objectbox_telemetry ◄── trigger watches this collection (insert)
    { vehicleId, ts, data: { …full VSS Vehicle tree… }, meta: { … } }
        │
        ▼  assembleTelemetry function (passes VSS tree through, adds CurrentLocation GeoJSON)
        ├──▶  telemetry-data   (INSERT every ~2s)
        └──▶  telemetry-status (UPSERT every ~10s)
```

## Unified document structure

`data` is the full VSS `Vehicle` tree (exact VSS paths). The trigger passes it through
verbatim and only adds `CurrentLocation.locationGeoJson`.

```json
{
  "vehicleId": "VSS-DEMO-VIN-001",
  "ts": "<ISO date>",          // time-series timeField (BSON Date, from the snapshot's ts)
  "timestamp": "<ISO date>",   // friendly alias
  "lastUpdated": "<ISO date>", // telemetry-status only
  "meta": { "vin": "...", "oem": "...", "model": "...", ... },
  "data": {
    "Powertrain":      { "CombustionEngine": { "Speed": 1820, ... }, "TractionBattery": { "StateOfCharge": { "Current": 68.3 }, ... }, ... },
    "Chassis":         { "Axle": { "Row1": { "Wheel": { "Left": { "Tire": { "Pressure": 230 } } } } }, ... },
    "ADAS":            { "CruiseControl": { "IsActive": true, ... }, ... },
    "CurrentLocation": { "Latitude": 48.85, "Longitude": 2.35, "locationGeoJson": { "type": "Point", "coordinates": [2.35, 48.85] }, ... },
    "Diagnostics":     { "DTCCount": 1, "DTCList": ["P0128"] }
    // … 45 top-level VSS domains total
  }
}
```

## One-time setup

### 1. Create the target collections

Create these two collections **once**, before starting the stack. Use the Atlas UI or
mongosh — whichever you prefer.

- `telemetry-data` — native **time-series** collection (`timeField: "ts"`, `metaField: "vehicleId"`)
- `telemetry-status` — standard collection with a **unique index** on `vehicleId`

**mongosh:**
```js
use car_assistant_demo   // your DATABASE_NAME

db.createCollection("telemetry-data", {
  timeseries: { timeField: "ts", metaField: "vehicleId", granularity: "seconds" }
  // , expireAfterSeconds: 86400   // optional: auto-expire history after 24h
});

db.createCollection("telemetry-status");
db["telemetry-status"].createIndex({ vehicleId: 1 }, { unique: true, name: "vehicleId_unique" });
```

**Atlas UI:** Collections → Create Collection → name `telemetry-data`, tick **Time Series**,
set Timefield `ts`, Metafield `vehicleId`, Granularity `seconds`. Then create `telemetry-status`
and add a unique index on `{ vehicleId: 1 }`.

> **Important:** time-series options (`timeField`/`metaField`/`granularity`) **cannot be changed
> after creation** — if `telemetry-data` was made wrong (e.g. wrong timeField or an unwanted TTL),
> drop it and recreate. `telemetry-data` must exist as time-series *before* the trigger's first
> insert, or MongoDB will auto-create it as a plain collection.

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
| Name | `ObjectboxTelemetryTrigger` |
| Enabled | Yes |
| Cluster | your cluster |
| Database | your database name |
| Collection | `objectbox_telemetry` |
| Operation type | Insert |
| Full document | On |
| Document preimage | Off |
| Select an event type | Function |
| Function | `assembleTelemetry` |

> The trigger watches `objectbox_telemetry` — one document is inserted per snapshot (~every 2 s). The function reads that document's nested `data` and `meta` and assembles all domains in a single invocation.

Save the trigger. Atlas will deploy the app automatically.
