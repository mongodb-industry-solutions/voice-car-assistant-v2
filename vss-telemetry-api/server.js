#!/usr/bin/env node

/**
 * VSS Telemetry API
 * REST service exposing per-domain vehicle telemetry tool calls over
 * POST /tools/:name. Reads from the unified telemetry-status collection
 * produced by the Atlas Trigger (assembleTelemetry function).
 */

import { MongoClient } from "mongodb";
import express from "express";

// ── Configuration ──────────────────────────────────────────────────────────────

const MONGODB_URI   = process.env.MONGODB_URI   || "";
const DATABASE_NAME = process.env.DATABASE_NAME || "";
const PORT          = parseInt(process.env.PORT  || "3002", 10);
const VEHICLE_ID    = process.env.VEHICLE_ID    || "VSS-DEMO-VIN-001";

// vehicleId is caller-controlled and goes into a Mongo query filter. Coerce to a string
// and allowlist it, so a non-string like { "$ne": "" } cannot become a selector operator
// (NoSQL injection). Anything invalid falls back to the default vehicle.
const VEHICLE_ID_RE = /^[A-Za-z0-9_-]{1,64}$/;
function safeVehicleId(v) {
  return typeof v === "string" && VEHICLE_ID_RE.test(v) ? v : VEHICLE_ID;
}

// ── MongoDB lazy connection ────────────────────────────────────────────────────

let _client = null;
let _db     = null;

async function getDb() {
  if (_db) return _db;
  if (!MONGODB_URI) throw new Error("MONGODB_URI environment variable is required");
  if (!DATABASE_NAME) throw new Error("DATABASE_NAME environment variable is required");

  console.log("Connecting to MongoDB Atlas...");
  _client = new MongoClient(MONGODB_URI, {
    serverSelectionTimeoutMS: 5000,
    connectTimeoutMS: 10000,
  });
  await _client.connect();
  _db = _client.db(DATABASE_NAME);
  console.log(`Connected to MongoDB — database: ${DATABASE_NAME}`);

  process.on("SIGINT",  async () => { await _client.close(); process.exit(0); });
  process.on("SIGTERM", async () => { await _client.close(); process.exit(0); });

  return _db;
}

// ── Helpers ────────────────────────────────────────────────────────────────────

function noData(domain) {
  return `No data available for vehicle ${VEHICLE_ID}${domain ? ` (${domain})` : ""}.`;
}

function sanitizeForLog(value) {
  return String(value).replace(/[\r\n\x00-\x1f\x7f]/g, " ").slice(0, 200);
}

// locationGeoJson may arrive as a parsed GeoJSON object (the Atlas assembleTelemetry
// function parses it before writing telemetry-status) or as a raw JSON string.
// Accept both and return the [lon, lat] coordinates, or null.
function geoCoords(geo) {
  if (!geo) return null;
  try {
    const obj = typeof geo === "string" ? JSON.parse(geo) : geo;
    return Array.isArray(obj?.coordinates) ? obj.coordinates : null;
  } catch (_) {
    return null;
  }
}

async function getStatus(vehicleId) {
  const db = await getDb();
  return db.collection("telemetry-status").findOne(
    { vehicleId: safeVehicleId(vehicleId) },
    { projection: { _id: 0 } }
  );
}

// OBD-II DTC descriptions — the official catalog from values-vss-data.md
// (Diagnostics.DTCReference.GenericExamples). Keep in sync with
// vss-telemetry-simulator/dtc_catalog.json (regenerate via gen_vss_spec.py).
// Unknown codes fall back to a category derived from the first character.
const DTC_DESCRIPTIONS = {
  C0021: "Wheel Speed Sensor Front Left Circuit",
  C0022: "Wheel Speed Sensor Front Left Circuit Range/Performance",
  C0025: "Wheel Speed Sensor Front Right Circuit",
  C0026: "Wheel Speed Sensor Front Right Circuit Range/Performance",
  C0029: "Wheel Speed Sensor Rear Left Circuit",
  C0030: "Wheel Speed Sensor Rear Left Circuit Range/Performance",
  C0033: "Wheel Speed Sensor Rear Right Circuit",
  C0034: "Wheel Speed Sensor Rear Right Circuit Range/Performance",
  C0035: "Left Front Wheel Speed Sensor Circuit",
  C0040: "Right Front Wheel Speed Sensor Circuit",
  P0001: "Fuel Volume Regulator Control Circuit/Open",
  P0002: "Fuel Volume Regulator Control Circuit Range/Performance",
  P0003: "Fuel Volume Regulator Control Circuit Low",
  P0004: "Fuel Volume Regulator Control Circuit High",
  P0010: "A Camshaft Position Actuator Circuit (Bank 1)",
  P0011: "A Camshaft Position Timing Over-Advanced or System Performance (Bank 1)",
  P0128: "Coolant Thermostat Below Thermostat Regulating Temperature",
  P0171: "System Too Lean (Bank 1)",
  P0172: "System Too Rich (Bank 1)",
  P0174: "System Too Lean (Bank 2)",
  P0300: "Random / multiple cylinder misfire detected",
  P0301: "Cylinder 1 misfire detected",
  P0302: "Cylinder 2 misfire detected",
  P0303: "Cylinder 3 misfire detected",
  P0304: "Cylinder 4 misfire detected",
  P0335: "Crankshaft Position Sensor A Circuit",
  P0340: "Camshaft Position Sensor Circuit",
  P0341: "Camshaft Position Sensor Circuit Range/Performance",
  P0401: "Exhaust Gas Recirculation Flow Insufficient Detected",
  P0411: "Secondary Air Injection System Incorrect Flow Detected",
  P0420: "Catalyst System Efficiency Below Threshold (Bank 1)",
  P0430: "Catalyst System Efficiency Below Threshold (Bank 2)",
  P0440: "Evaporative Emission Control System Malfunction",
  P0442: "Evaporative Emission Control System Leak Detected (Small Leak)",
  P0446: "Evaporative Emission Control System Vent Control Circuit",
  P0455: "Evaporative Emission Control System Leak Detected (Gross Leak)",
  P0456: "Evaporative Emission Control System Leak Detected (Very Small Leak)",
  P0500: "Vehicle Speed Sensor Malfunction",
  P0505: "Idle Control System Malfunction",
  P0507: "Idle Control System RPM Higher Than Expected",
  P0700: "Transmission Control System Malfunction",
  U0001: "High Speed CAN Communication Bus",
  U0002: "High Speed CAN Communication Bus Performance",
  U0003: "High Speed CAN Communication Bus Open",
  U0004: "High Speed CAN Communication Bus Low",
  U0005: "High Speed CAN Communication Bus High",
  U0006: "Medium Speed CAN Communication Bus",
  U0007: "Medium Speed CAN Communication Bus Performance",
  U0008: "Medium Speed CAN Communication Bus Open",
  U0009: "Medium Speed CAN Communication Bus Low",
  U0010: "Medium Speed CAN Communication Bus High",
};

const DTC_CATEGORY = {
  P: "Powertrain (engine, transmission, emissions)",
  C: "Chassis (brakes, steering, suspension, ABS)",
  B: "Body (airbags, climate, seats, lighting)",
  U: "Network / communication",
};

function describeDtc(code) {
  return DTC_DESCRIPTIONS[code]
    || DTC_CATEGORY[String(code)[0]]
    || "Unknown fault code";
}

// ── Tool implementations ───────────────────────────────────────────────────────
// Consumers read the full VSS `Vehicle` tree (telemetry-status.data); fields are
// addressed by exact VSS path via pick().

function pick(obj, path) {
  return path.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);
}
const fmt = (v, u = "") => (v == null ? "N/A" : `${v}${u}`);
const stamp = (doc) => `Last Updated: ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`;

async function get_vehicle_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !doc.data) return noData();
  const d = doc.data, meta = doc.meta;

  const lines = [`VSS Vehicle Status — ${VEHICLE_ID}`, "=".repeat(50)];
  if (meta) {
    lines.push(`OEM: ${meta.oem || "N/A"}  Model: ${meta.model || "N/A"}  VIN: ${meta.vin || "N/A"}`);
    lines.push(`Powertrain: ${meta.powertrainType || "N/A"}  Drivetrain: ${meta.drivetrainType || "N/A"}`);
  }
  lines.push(`Speed: ${fmt(pick(d, "Speed"))} km/h  Engine RPM: ${fmt(pick(d, "Powertrain.CombustionEngine.Speed"))}  Gear: ${fmt(pick(d, "Powertrain.Transmission.CurrentGear"))}`);
  lines.push(`Fuel: ${fmt(pick(d, "Powertrain.FuelSystem.RelativeLevel"))} %  Battery SOC: ${fmt(pick(d, "Powertrain.TractionBattery.StateOfCharge.Current"))} %  Range: ${fmt(pick(d, "Powertrain.TractionBattery.Range"))} km`);
  const chg = pick(d, "Powertrain.TractionBattery.Charging.IsCharging");
  lines.push(`Charging: ${chg == null ? "N/A" : (chg ? "yes" : "no")}`);
  lines.push(`Tires (kPa)  FL: ${fmt(pick(d, "Chassis.Axle.Row1.Wheel.Left.Tire.Pressure"))}  FR: ${fmt(pick(d, "Chassis.Axle.Row1.Wheel.Right.Tire.Pressure"))}  RL: ${fmt(pick(d, "Chassis.Axle.Row2.Wheel.Left.Tire.Pressure"))}  RR: ${fmt(pick(d, "Chassis.Axle.Row2.Wheel.Right.Tire.Pressure"))}`);
  const loc = d.CurrentLocation || {};
  const coords = geoCoords(loc.locationGeoJson);
  const lat = coords ? coords[1] : loc.Latitude;
  const lon = coords ? coords[0] : loc.Longitude;
  if (lat != null) lines.push(`Location: ${lat}, ${lon}  Heading: ${fmt(loc.Heading)}`);
  lines.push(`Cabin: ${fmt(pick(d, "Cabin.HVAC.AmbientAirTemperature"))} °C  A/C: ${fmt(pick(d, "Cabin.HVAC.IsAirConditioningActive"))}`);
  lines.push(`Cruise: ${fmt(pick(d, "ADAS.CruiseControl.IsActive"))}  Active Fault Codes: ${fmt(pick(d, "Diagnostics.DTCCount"))}`);
  return lines.join("\n");
}

async function get_powertrain_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "Powertrain")) return noData("powertrain");
  const d = doc.data;
  const coolant = pick(d, "Powertrain.CombustionEngine.EngineCoolant.Temperature");

  const lines = [
    `Powertrain Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Vehicle Speed: ${fmt(pick(d, "Speed"))} km/h`,
    `Engine RPM:    ${fmt(pick(d, "Powertrain.CombustionEngine.Speed"))} rpm`,
    `Coolant Temp:  ${fmt(coolant)} °C${coolant != null ? (coolant > 110 ? "  ⚠ OVERHEATING" : coolant > 95 ? "  (warm)" : "  (normal)") : ""}`,
    `Gear:          ${fmt(pick(d, "Powertrain.Transmission.CurrentGear"))}`,
    `Throttle:      ${fmt(pick(d, "Powertrain.CombustionEngine.TPS"))} %`,
    stamp(doc),
  ];
  return lines.join("\n");
}

async function get_fuel_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "Powertrain.FuelSystem")) return noData("fuel");
  const d = doc.data, meta = doc.meta || {};
  const pct    = pick(d, "Powertrain.FuelSystem.RelativeLevel");
  const tankL  = meta.fuelTankCapacityL ?? null;
  const litres = (pct != null && tankL != null) ? (pct / 100 * tankL).toFixed(1) : null;
  const label  = pct == null ? "" : pct < 10 ? "  ⚠ CRITICALLY LOW" : pct < 20 ? "  (low)" : "";

  const lines = [
    `Fuel Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Fuel Level:          ${fmt(pct)} %${label}`,
    `Fuel Remaining:      ${litres ?? "N/A"} L${tankL != null ? `  (tank: ${tankL} L)` : ""}`,
    `Range:               ${fmt(pick(d, "Powertrain.FuelSystem.Range"))} km`,
    `Instant Consumption: ${fmt(pick(d, "Powertrain.FuelSystem.InstantConsumption"))} l/100km`,
    stamp(doc),
    "",
    "NOTE: This is liquid fuel only. For the high-voltage drive battery (state of "
      + "charge, electric range, charging), use get_battery_status instead.",
  ];
  return lines.join("\n");
}

async function get_battery_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "Powertrain.TractionBattery")) return noData("battery");
  const d = doc.data;
  const soc      = pick(d, "Powertrain.TractionBattery.StateOfCharge.Current");
  const charging = pick(d, "Powertrain.TractionBattery.Charging.IsCharging");
  const rate     = pick(d, "Powertrain.TractionBattery.Charging.ChargeRate");
  const socLabel =
    soc == null ? ""
    : soc < 15 ? "  ⚠ CRITICALLY LOW"
    : soc < 25 ? "  (low)"
    : soc > 90 ? "  (full)"
    : "  (normal)";

  const lines = [
    `Battery Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `State of Charge: ${fmt(soc)} %${socLabel}`,
    `Estimated Range: ${fmt(pick(d, "Powertrain.TractionBattery.Range"))} km`,
    `Charging Status: ${charging == null ? "N/A" : charging ? `Charging (${fmt(rate)} kW)` : "Not charging"}`,
    `Voltage:         ${fmt(pick(d, "Powertrain.TractionBattery.CurrentVoltage"))} V`,
    `Current:         ${fmt(pick(d, "Powertrain.TractionBattery.CurrentCurrent"))} A`,
    `Temp:            ${fmt(pick(d, "Powertrain.TractionBattery.Temperature.Average"))} °C`,
    `State of Health: ${fmt(pick(d, "Powertrain.TractionBattery.StateOfHealth"))} %`,
    stamp(doc),
  ];
  return lines.join("\n");
}

async function get_chassis_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "Chassis")) return noData("chassis");
  const d = doc.data;
  const tireSummary = (position, kpa) => {
    if (kpa == null) return `${position}: N/A`;
    const psi    = (kpa / 6.895).toFixed(1);
    const status = psi < 28 ? "⚠ CRITICAL" : (psi < 30 || psi > 35) ? "⚠ WARNING" : "OK";
    return `${position}: ${psi} psi (${kpa} kPa)  [${status}]`;
  };

  const lines = [
    `Chassis Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    "Tire Pressures:",
    `  ${tireSummary("Front-Left ", pick(d, "Chassis.Axle.Row1.Wheel.Left.Tire.Pressure"))}`,
    `  ${tireSummary("Front-Right", pick(d, "Chassis.Axle.Row1.Wheel.Right.Tire.Pressure"))}`,
    `  ${tireSummary("Rear-Left  ", pick(d, "Chassis.Axle.Row2.Wheel.Left.Tire.Pressure"))}`,
    `  ${tireSummary("Rear-Right ", pick(d, "Chassis.Axle.Row2.Wheel.Right.Tire.Pressure"))}`,
    `ABS Engaged:      ${fmt(pick(d, "ADAS.ABS.IsEngaged"))}`,
    `Traction Control: ${fmt(pick(d, "ADAS.TCS.IsEngaged"))}`,
    `Brake Pedal:      ${fmt(pick(d, "Chassis.Brake.PedalPosition"))} %`,
    stamp(doc),
  ];
  return lines.join("\n");
}

async function get_cabin_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "Cabin")) return noData("cabin");
  const d = doc.data;

  const lines = [
    `Cabin Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Cabin Temp:    ${fmt(pick(d, "Cabin.HVAC.AmbientAirTemperature"))} °C`,
    `Outside Temp:  ${fmt(pick(d, "Exterior.AirTemperature"))} °C`,
    `A/C Active:    ${fmt(pick(d, "Cabin.HVAC.IsAirConditioningActive"))}`,
    `Recirculation: ${fmt(pick(d, "Cabin.HVAC.IsRecirculationActive"))}`,
    stamp(doc),
  ];
  return lines.join("\n");
}

async function get_location(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "CurrentLocation")) return noData("location");

  const loc    = doc.data.CurrentLocation;
  const coords = geoCoords(loc.locationGeoJson);
  const lat    = coords ? coords[1] : loc.Latitude;
  const lon    = coords ? coords[0] : loc.Longitude;

  const lines = [
    `Location — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Latitude:      ${fmt(lat)}`,
    `Longitude:     ${fmt(lon)}`,
    `Altitude:      ${fmt(loc.Altitude)} m`,
    `Heading:       ${fmt(loc.Heading)} °`,
    `Vehicle Speed: ${fmt(pick(doc.data, "Speed"))} km/h`,
    stamp(doc),
  ];
  return lines.join("\n");
}

async function get_adas_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "ADAS")) return noData("adas");
  const d = doc.data;
  const collision = pick(d, "ADAS.ObstacleDetection.Front.Center.IsWarning");

  const lines = [
    `ADAS Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Cruise Control:    ${fmt(pick(d, "ADAS.CruiseControl.IsActive"))}`,
    `Cruise Set Speed:  ${fmt(pick(d, "ADAS.CruiseControl.SpeedSet"))} km/h`,
    `Lane Departure:    ${fmt(pick(d, "ADAS.LaneDepartureDetection.IsEnabled"))}`,
    `Collision Warning: ${fmt(collision)}${collision ? "  ⚠ ALERT" : ""}`,
    `ABS / TCS / ESC:   ${fmt(pick(d, "ADAS.ABS.IsEngaged"))} / ${fmt(pick(d, "ADAS.TCS.IsEngaged"))} / ${fmt(pick(d, "ADAS.ESC.IsEngaged"))}`,
    stamp(doc),
  ];
  return lines.join("\n");
}

async function get_diagnostics_status(vehicleId) {
  const doc = await getStatus(vehicleId);
  if (!doc || !pick(doc.data, "Diagnostics")) return noData("diagnostics");

  const dg    = doc.data.Diagnostics;
  const codes = Array.isArray(dg.DTCList) ? dg.DTCList : [];
  const count = dg.DTCCount ?? codes.length;

  const lines = [
    `Diagnostics — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Active Fault Codes: ${count}`,
  ];
  if (codes.length === 0) {
    lines.push("No active fault codes. All systems nominal.");
  } else {
    for (const code of codes) lines.push(`  ${code} — ${describeDtc(code)}`);
    lines.push("", "NOTE: For what a specific code or warning light means and how to "
      + "respond, consult the owner's manual (search_car_manual).");
  }
  lines.push(stamp(doc));
  return lines.join("\n");
}

// ── Tool dispatch map ──────────────────────────────────────────────────────────

// Each tool takes the caller's vehicleId (per-session vehicle); falls back to VEHICLE_ID.
const toolHandlers = {
  get_vehicle_status:    (args) => get_vehicle_status(args.vehicleId),
  get_powertrain_status: (args) => get_powertrain_status(args.vehicleId),
  get_fuel_status:       (args) => get_fuel_status(args.vehicleId),
  get_battery_status:    (args) => get_battery_status(args.vehicleId),
  get_chassis_status:    (args) => get_chassis_status(args.vehicleId),
  get_cabin_status:      (args) => get_cabin_status(args.vehicleId),
  get_location:          (args) => get_location(args.vehicleId),
  get_adas_status:       (args) => get_adas_status(args.vehicleId),
  get_diagnostics_status:(args) => get_diagnostics_status(args.vehicleId),
};

// ── Express app ────────────────────────────────────────────────────────────────

const app = express();

app.post("/tools/:toolName", express.json(), async (req, res) => {
  const { toolName } = req.params;
  const handler      = toolHandlers[toolName];

  if (!handler) {
    res.status(404).json({ error: `Unknown tool: ${toolName}. Available: ${Object.keys(toolHandlers).join(", ")}` });
    return;
  }

  try {
    const resultText = await handler(req.body || {});
    res.json({ result: resultText });
  } catch (e) {
    console.error(`Tool error [${sanitizeForLog(toolName)}]:`, e);
    res.status(500).json({ error: e.message });
  }
});

// Cloud-side document counts — the Atlas end of the sync, for the live sync panel.
app.get("/cloud/counts", async (req, res) => {
  try {
    const db = await getDb();
    const [objectbox, tsData, tsStatus] = await Promise.all([
      // exact count so it converges visibly with the edge count in the sync panel
      db.collection("objectbox_telemetry").countDocuments(),
      db.collection("telemetry-data").estimatedDocumentCount().catch(() => null),
      db.collection("telemetry-status").estimatedDocumentCount().catch(() => null),
    ]);
    res.json({ objectbox_telemetry: objectbox, "telemetry-data": tsData, "telemetry-status": tsStatus });
  } catch (e) {
    res.status(503).json({ error: e.message });
  }
});

app.get("/health", async (req, res) => {
  let dbStatus    = "unknown";
  let dbLatencyMs = null;

  try {
    const t0 = Date.now();
    const db = await getDb();
    await db.command({ ping: 1 });
    dbLatencyMs = Date.now() - t0;
    dbStatus    = "connected";
  } catch (e) {
    dbStatus = `error: ${e.message}`;
  }

  res.json({
    status:    "healthy",
    service:   "vss-telemetry-api",
    version:   "2.0.0",
    port:      PORT,
    vehicleId: VEHICLE_ID,
    database:  DATABASE_NAME || "(not set)",
    db:        { status: dbStatus, latencyMs: dbLatencyMs },
    tools:     Object.keys(toolHandlers),
    timestamp: new Date().toISOString(),
  });
});

// ── Start ──────────────────────────────────────────────────────────────────────

app.listen(PORT, () => {
  console.log(`VSS Telemetry API running on port ${PORT}`);
  console.log(`  Vehicle ID : ${VEHICLE_ID}`);
  console.log(`  Database   : ${DATABASE_NAME || "(set DATABASE_NAME env var)"}`);
  console.log(`  Endpoints  : POST /tools/:name  GET /health`);
  console.log(`  Tools      : ${Object.keys(toolHandlers).join(", ")}`);
});
