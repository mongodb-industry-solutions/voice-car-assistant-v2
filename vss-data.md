# VSS Telemetry Data Model

All entities are sync-enabled and replicated to MongoDB Atlas via the shared ObjectBox Sync Server.
Entity IDs 10–24 belong to this telemetry stack; IDs 1–4 belong to the original services
(`manual_chunks`, `manuals`, `conversations`, `telemetry_snapshots`).

The model is **metadata + append-only Sample entities**. "Current status" is derived by reading the
newest Sample of each domain — locally via `GET /vss/latest` on the C++ service, and in the cloud via
the Atlas `assembleTelemetry` trigger (see [Read paths](#read-paths)).

Authoritative source of truth: `sync-server-setup/objectbox-model.json`.

---

## Metadata Entities

### VehicleMeta (entity 10)
One row per vehicle. Seeded once via `POST /vss/meta`. Describes the physical vehicle identity and static attributes.

| Field | Type | Description |
|---|---|---|
| `id` | Long | ObjectBox internal ID |
| `vehicleId` | String | Application-level vehicle identifier (`VSS-DEMO-VIN-001`) |
| `vin` | String | Vehicle Identification Number |
| `oem` | String | Manufacturer name (`MongoDB`) |
| `model` | String | Vehicle model name (`Leafy 1.0`) |
| `platform` | String | Platform / generation code |
| `softwareVersion` | String | Current software stack version |
| `createdAt` | Long | Unix epoch ms — first time seeded |
| `updatedAt` | Long | Unix epoch ms — last update |
| `syncClock` | Long | Sync bookkeeping field |
| `fuelTankCapacityL` | Float | Total fuel tank capacity in litres |
| `batteryCapacityKwh` | Float | High-voltage battery gross capacity in kWh |
| `wheelbaseMm` | Int | Wheelbase in millimetres |
| `curbWeightKg` | Int | Kerb weight in kilograms |
| `powertrainType` | String | `ICE`, `BEV`, `HEV`, `PHEV`, `FCEV` |
| `drivetrainType` | String | `FWD`, `RWD`, `AWD` |

**Used by:** `get_vehicle_status` (identity + static attributes), `get_fuel_status` (`fuelTankCapacityL`).

---

### SignalDefinition (entity 11)
A registry of every VSS signal the vehicle exposes. One row per signal path. Not written at runtime in the current implementation — intended as a future configuration source for dynamic sampling.

| Field | Type | Description |
|---|---|---|
| `id` | Long | ObjectBox internal ID |
| `vssPath` | String | Full VSS path, e.g. `Vehicle.Powertrain.TractionBattery.StateOfCharge.Displayed` |
| `component` | String | Domain shorthand (`powertrain`, `battery`, etc.) |
| `signalKind` | String | VSS kind: `sensor`, `actuator`, or `attribute` |
| `valueType` | String | Data type of the value (`float`, `bool`, `string`, etc.) |
| `unit` | String | Unit string per VSS spec (`km/h`, `kPa`, `%`, etc.) |
| `writable` | Bool | Whether this signal can be commanded (actuator) |
| `latestGroup` | String | Logical grouping label for the latest value |
| `historyGroup` | String | Which Sample entity stores historical readings |
| `historyMode` | String | Append mode: `always`, `on_change`, `none` |
| `samplePeriodMs` | Int | Target sampling interval in milliseconds |
| `retainHours` | Int | How many hours of history to keep |
| `enabled` | Bool | Whether this signal is actively sampled |
| `syncClock` | Long | Sync bookkeeping field |

**Used by:** Not queried at runtime. Reserved for future dynamic signal configuration.

---

## Sample Entities
Append-only time-series records written on every snapshot (`POST /vss/snapshot`, ~every 2 s).
Pruned automatically after **24 hours** (background thread, hourly). Indexed on both `vehicleId`
and `ts` for efficient range queries. Every sample is inserted with `id = 0`, so each write appends
a new row. `syncClock` is a sync bookkeeping field, left `0` for these append-only rows.

Every Sample shares the same first four columns: `id` (Long), `vehicleId` (String, indexed),
`ts` (Long, indexed, Unix epoch ms), `tripId` (String). Only the domain-specific fields are listed below.

### PowertrainSample (entity 19)
Time-series powertrain readings.

| Field | Type | Description |
|---|---|---|
| `speedKph` | Float | Vehicle speed in km/h |
| `engineRpm` | Float | Engine / motor RPM |
| `fuelLevelPct` | Float | Fuel level as percentage of tank capacity |
| `fuelRateLph` | Float | Instantaneous fuel consumption in litres/hour |
| `coolantTempC` | Float | Coolant temperature in °C |
| `throttlePct` | Float | Throttle position 0–100% |
| `gear` | Int | Current engaged gear (0 = neutral, -1 = reverse) |
| `syncClock` | Long | Sync bookkeeping field |

**Used by:** `get_powertrain_status`, `get_fuel_status`, `get_vehicle_status`; `GET /vss/powertrain/history`.

---

### BatterySample (entity 20)
Time-series high-voltage battery readings.

| Field | Type | Description |
|---|---|---|
| `socPct` | Float | State of Charge — displayed value 0–100% |
| `sohPct` | Float | State of Health — battery degradation indicator 0–100% |
| `batteryTempC` | Float | Battery pack temperature in °C |
| `chargingPowerKw` | Float | Active charging power in kW (0 when not charging; charging is inferred from `> 0`) |
| `estimatedRangeKm` | Float | Estimated remaining range in km |
| `voltageV` | Float | Battery pack voltage in V |
| `currentA` | Float | Battery pack current in A (positive = discharge) |
| `syncClock` | Long | Sync bookkeeping field |

**Used by:** `get_battery_status`, `get_vehicle_status`; `GET /vss/battery/history`.

---

### LocationSample (entity 21)
Time-series GPS track. Position is stored as a GeoJSON Point string (`locationGeoJson`).

| Field | Type | Description |
|---|---|---|
| `altitudeM` | Float | Altitude above sea level in metres |
| `headingDeg` | Float | True heading in degrees (0 = north, clockwise) |
| `speedKph` | Float | GPS-derived speed in km/h |
| `accuracyM` | Float | Horizontal position accuracy in metres |
| `syncClock` | Long | Sync bookkeeping field |
| `locationGeoJson` | String | GeoJSON Point: `{"type":"Point","coordinates":[lng,lat]}` (lng-first, RFC 7946) |

**Used by:** `get_location`, `get_vehicle_status`; `GET /vss/location/history`.

---

### CabinSample (entity 22)
Time-series cabin environment readings.

| Field | Type | Description |
|---|---|---|
| `insideTempC` | Float | Cabin air temperature in °C |
| `outsideTempC` | Float | Ambient outside temperature in °C |
| `hvacMode` | String | HVAC operating mode: `auto`, `heat`, `cool`, `off` |
| `fanSpeed` | Int | Fan speed level 0–10 |
| `syncClock` | Long | Sync bookkeeping field |

**Used by:** `get_cabin_status`, `get_vehicle_status`; `GET /vss/cabin/history`.

---

### AdasSample (entity 23)
Time-series Advanced Driver Assistance Systems status.

| Field | Type | Description |
|---|---|---|
| `cruiseEnabled` | Bool | Cruise control active |
| `cruiseSetSpeedKph` | Float | Cruise control target speed in km/h |
| `laneKeepAssistOn` | Bool | Lane-keep assist enabled |
| `collisionWarningActive` | Bool | Forward collision warning currently triggered |
| `syncClock` | Long | Sync bookkeeping field |

**Used by:** `get_adas_status`, `get_vehicle_status`; `GET /vss/adas/history`.

---

### ChassisSample (entity 24)
Time-series chassis dynamics readings.

| Field | Type | Description |
|---|---|---|
| `steeringAngleDeg` | Float | Steering wheel angle in degrees (positive = right) |
| `brakePedalPct` | Float | Brake pedal pressure 0–100% |
| `tirePressureFlKpa` | Float | Front-left tyre pressure in kPa |
| `tirePressureFrKpa` | Float | Front-right tyre pressure in kPa |
| `tirePressureRlKpa` | Float | Rear-left tyre pressure in kPa |
| `tirePressureRrKpa` | Float | Rear-right tyre pressure in kPa |
| `absActive` | Bool | ABS currently intervening |
| `tractionControlActive` | Bool | Traction control currently intervening |
| `syncClock` | Long | Sync bookkeeping field |

**Used by:** `get_chassis_status`, `get_vehicle_status`; `GET /vss/chassis/history`.

---

## Read paths

The Sample entities are the single source of truth; "current status" is derived, not stored:

- **Local / offline** — the C++ service (`vss-telemetry-service`, :8086) serves `GET /vss/latest`,
  which reads the newest row of each Sample box + `VehicleMeta` and returns a unified snapshot.
  The dashboard polls this every 3 s. `GET /vss/{domain}/history?minutes=N` serves raw sample ranges.
- **Cloud / online** — each Sample entity syncs to a MongoDB Atlas collection of the same name
  (`PowertrainSample`, `BatterySample`, …). An Atlas trigger (`assembleTelemetry`) fires on every
  `PowertrainSample` insert, reads the latest of each companion collection, and writes:
  - `telemetry-data` — appended every ~2 s (time-series history)
  - `telemetry-status` — upserted at most every ~10 s (one document per vehicle)

  The `vss-telemetry-api` REST tools (`get_*`) read `telemetry-status`.

---

## Summary

| Entity | ID | Type | Written by | Queried by |
|---|---|---|---|---|
| VehicleMeta | 10 | Metadata | `POST /vss/meta` (once) | `get_vehicle_status`, `get_fuel_status` |
| SignalDefinition | 11 | Metadata | — (not populated) | — |
| PowertrainSample | 19 | Sample (24h) | Simulator → C++ service | `get_powertrain_status`, `get_fuel_status`, `/vss/powertrain/history` |
| BatterySample | 20 | Sample (24h) | Simulator → C++ service | `get_battery_status`, `/vss/battery/history` |
| LocationSample | 21 | Sample (24h) | Simulator → C++ service | `get_location`, `/vss/location/history` |
| CabinSample | 22 | Sample (24h) | Simulator → C++ service | `get_cabin_status`, `/vss/cabin/history` |
| AdasSample | 23 | Sample (24h) | Simulator → C++ service | `get_adas_status`, `/vss/adas/history` |
| ChassisSample | 24 | Sample (24h) | Simulator → C++ service | `get_chassis_status`, `/vss/chassis/history` |
