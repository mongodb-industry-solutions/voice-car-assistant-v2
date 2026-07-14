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

async function getStatus() {
  const db = await getDb();
  return db.collection("telemetry-status").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
}

// OBD-II DTC descriptions (mirrors the simulator DTC_CATALOG). Unknown codes fall
// back to a category derived from the first character.
const DTC_DESCRIPTIONS = {
  P0128: "Coolant thermostat below regulating temperature",
  P0171: "System too lean (Bank 1)",
  P0172: "System too rich (Bank 1)",
  P0300: "Random / multiple cylinder misfire detected",
  P0301: "Cylinder 1 misfire detected",
  P0302: "Cylinder 2 misfire detected",
  P0303: "Cylinder 3 misfire detected",
  P0304: "Cylinder 4 misfire detected",
  P0420: "Catalyst system efficiency below threshold (Bank 1)",
  P0442: "Evaporative emission system leak detected (small leak)",
  P0455: "Evaporative emission system leak detected (gross leak)",
  P0500: "Vehicle speed sensor malfunction",
  P0700: "Transmission control system malfunction",
  C0021: "Wheel speed sensor front left circuit",
  C0035: "Left front wheel speed sensor circuit",
  C0040: "Right front wheel speed sensor circuit",
  B0001: "Driver frontal stage 1 deployment control",
  B0020: "Left side airbag deployment control",
  U0001: "High speed CAN communication bus",
  U0002: "High speed CAN communication bus performance",
  U0006: "Medium speed CAN communication bus",
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

async function get_vehicle_status() {
  const doc = await getStatus();
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

async function get_powertrain_status() {
  const doc = await getStatus();
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

async function get_fuel_status() {
  const doc = await getStatus();
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

async function get_battery_status() {
  const doc = await getStatus();
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

async function get_chassis_status() {
  const doc = await getStatus();
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

async function get_cabin_status() {
  const doc = await getStatus();
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

async function get_location() {
  const doc = await getStatus();
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

async function get_adas_status() {
  const doc = await getStatus();
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

async function get_diagnostics_status() {
  const doc = await getStatus();
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

const toolHandlers = {
  get_vehicle_status:    (_args) => get_vehicle_status(),
  get_powertrain_status: (_args) => get_powertrain_status(),
  get_fuel_status:       (_args) => get_fuel_status(),
  get_battery_status:    (_args) => get_battery_status(),
  get_chassis_status:    (_args) => get_chassis_status(),
  get_cabin_status:      (_args) => get_cabin_status(),
  get_location:          (_args) => get_location(),
  get_adas_status:       (_args) => get_adas_status(),
  get_diagnostics_status:(_args) => get_diagnostics_status(),
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
