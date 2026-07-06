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

// ── Tool implementations ───────────────────────────────────────────────────────

async function get_vehicle_status() {
  const doc = await getStatus();
  if (!doc) return noData();

  const { meta, data } = doc;
  const pt  = data?.powertrain || {};
  const bat = data?.battery    || {};
  const loc = data?.location   || {};
  const cab = data?.cabin      || {};
  const ads = data?.adas       || {};
  const ch  = data?.chassis    || {};

  const lines = [`VSS Vehicle Status — ${VEHICLE_ID}`, "=".repeat(50)];

  if (meta) {
    lines.push(`OEM: ${meta.oem || "N/A"}  Model: ${meta.model || "N/A"}  VIN: ${meta.vin || "N/A"}`);
    lines.push(`Powertrain: ${meta.powertrainType || "N/A"}  Drivetrain: ${meta.drivetrainType || "N/A"}  Wheelbase: ${meta.wheelbaseMm ?? "N/A"} mm  Curb Weight: ${meta.curbWeightKg ?? "N/A"} kg`);
    lines.push(`Fuel Tank: ${meta.fuelTankCapacityL ?? "N/A"} L  Battery Capacity: ${meta.batteryCapacityKwh ?? "N/A"} kWh`);
  }
  if (pt.speedKph != null) {
    lines.push(`Speed: ${pt.speedKph} km/h  RPM: ${pt.engineRpm ?? "N/A"}  Fuel: ${pt.fuelLevelPct ?? "N/A"}%`);
  }
  if (bat.socPct != null) {
    const charging = (bat.chargingPowerKw ?? 0) > 0 ? "charging" : "not charging";
    lines.push(`Battery SOC: ${bat.socPct}%  Est. Range: ${bat.estimatedRangeKm ?? "N/A"} km  Charging: ${charging}`);
  }
  if (ch.tirePressureFlKpa != null) {
    lines.push(`Tires (kPa)  FL: ${ch.tirePressureFlKpa}  FR: ${ch.tirePressureFrKpa}  RL: ${ch.tirePressureRlKpa}  RR: ${ch.tirePressureRrKpa}`);
  }
  const vsCoords = geoCoords(loc.locationGeoJson);
  if (vsCoords) {
    lines.push(`Location: ${vsCoords[1]}, ${vsCoords[0]}  Heading: ${loc.headingDeg ?? "N/A"}°`);
  }
  if (cab.insideTempC != null) {
    lines.push(`Interior Temp: ${cab.insideTempC}°C  HVAC: ${cab.hvacMode ?? "N/A"}`);
  }
  if (ads.cruiseEnabled != null) {
    lines.push(`Cruise Control: ${ads.cruiseEnabled}  LKA: ${ads.laneKeepAssistOn ?? "N/A"}  Collision Warning: ${ads.collisionWarningActive ?? "N/A"}`);
  }

  return lines.join("\n");
}

async function get_powertrain_status() {
  const doc = await getStatus();
  if (!doc || !doc.data?.powertrain) return noData("powertrain");

  const pt = doc.data.powertrain;

  const lines = [
    `Powertrain Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Speed:         ${pt.speedKph      ?? "N/A"} km/h`,
    `Engine RPM:    ${pt.engineRpm     ?? "N/A"} rpm`,
    `Coolant Temp:  ${pt.coolantTempC  ?? "N/A"} °C${pt.coolantTempC != null ? (pt.coolantTempC > 110 ? "  ⚠ OVERHEATING" : pt.coolantTempC > 95 ? "  (warm)" : "  (normal)") : ""}`,
    `Gear:          ${pt.gear          ?? "N/A"}`,
    `Throttle:      ${pt.throttlePct   ?? "N/A"} %`,
    `Last Updated:  ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
  ];
  return lines.join("\n");
}

async function get_fuel_status() {
  const doc = await getStatus();
  if (!doc || !doc.data?.powertrain) return noData("powertrain");

  const pt   = doc.data.powertrain;
  const meta = doc.meta || {};
  const pct    = pt.fuelLevelPct;
  const tankL  = meta.fuelTankCapacityL ?? null;
  const litres = (pct != null && tankL != null) ? (pct / 100 * tankL).toFixed(1) : null;
  const label  = pct == null ? "" : pct < 10 ? "  ⚠ CRITICALLY LOW" : pct < 20 ? "  (low)" : "";

  const lines = [
    `Fuel Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Fuel Level:        ${pct != null ? pct.toFixed(1) : "N/A"} %${label}`,
    `Fuel Remaining:    ${litres ?? "N/A"} L${tankL != null ? `  (tank: ${tankL} L)` : ""}`,
    `Consumption Rate:  ${pt.fuelRateLph != null ? pt.fuelRateLph.toFixed(1) : "N/A"} L/h`,
    `Last Updated:      ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
    "",
    "NOTE: This is liquid fuel only. For the high-voltage drive battery (state of "
      + "charge, electric range, charging), use get_battery_status instead.",
  ];
  return lines.join("\n");
}

async function get_battery_status() {
  const doc = await getStatus();
  if (!doc || !doc.data?.battery) return noData("battery");

  const bat = doc.data.battery;
  const isCharging    = (bat.chargingPowerKw ?? 0) > 0;
  const chargingStatus = isCharging ? `Charging at ${bat.chargingPowerKw ?? "N/A"} kW` : "Not charging";
  const socLabel =
    bat.socPct != null
      ? bat.socPct < 15 ? "  ⚠ CRITICALLY LOW"
      : bat.socPct < 25 ? "  (low)"
      : bat.socPct > 90 ? "  (full)"
      : "  (normal)"
      : "";

  const lines = [
    `Battery Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `State of Charge: ${bat.socPct           ?? "N/A"} %${socLabel}`,
    `Estimated Range: ${bat.estimatedRangeKm ?? "N/A"} km`,
    `Charging Status: ${chargingStatus}`,
    `Voltage:         ${bat.voltageV         ?? "N/A"} V`,
    `Current:         ${bat.currentA         ?? "N/A"} A`,
    `Temp:            ${bat.batteryTempC     ?? "N/A"} °C`,
    `State of Health: ${bat.sohPct           ?? "N/A"} %`,
    `Last Updated:    ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
  ];
  return lines.join("\n");
}

async function get_chassis_status() {
  const doc = await getStatus();
  if (!doc || !doc.data?.chassis) return noData("chassis");

  const ch = doc.data.chassis;
  const tireSummary = (position, kpa) => {
    if (kpa == null) return `${position}: N/A`;
    const psi    = (kpa / 6.895).toFixed(1);
    const status = psi < 28 ? "⚠ CRITICAL" : psi < 30 || psi > 35 ? "⚠ WARNING" : "OK";
    return `${position}: ${psi} psi (${kpa} kPa)  [${status}]`;
  };

  const lines = [
    `Chassis Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    "Tire Pressures:",
    `  ${tireSummary("Front-Left ", ch.tirePressureFlKpa)}`,
    `  ${tireSummary("Front-Right", ch.tirePressureFrKpa)}`,
    `  ${tireSummary("Rear-Left  ", ch.tirePressureRlKpa)}`,
    `  ${tireSummary("Rear-Right ", ch.tirePressureRrKpa)}`,
    `ABS Active:       ${ch.absActive             ?? "N/A"}`,
    `Traction Control: ${ch.tractionControlActive ?? "N/A"}`,
    `Brake Pedal:      ${ch.brakePedalPct         ?? "N/A"} %`,
    `Last Updated:     ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
  ];
  return lines.join("\n");
}

async function get_cabin_status() {
  const doc = await getStatus();
  if (!doc || !doc.data?.cabin) return noData("cabin");

  const cab = doc.data.cabin;

  const lines = [
    `Cabin Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Interior Temp:        ${cab.insideTempC            ?? "N/A"} °C`,
    `Outside Temp:         ${cab.outsideTempC           ?? "N/A"} °C`,
    `HVAC Mode:            ${cab.hvacMode               ?? "N/A"}`,
    `Fan Speed:            ${cab.fanSpeed               ?? "N/A"}`,
    `Last Updated: ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
  ];
  return lines.join("\n");
}

async function get_location() {
  const doc = await getStatus();
  if (!doc || !doc.data?.location) return noData("location");

  const loc    = doc.data.location;
  const coords = geoCoords(loc.locationGeoJson);
  const lon    = coords ? coords[0] : null;
  const lat    = coords ? coords[1] : null;

  const lines = [
    `Location — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Latitude:    ${lat               ?? "N/A"}`,
    `Longitude:   ${lon               ?? "N/A"}`,
    `Altitude:    ${loc.altitudeM     ?? "N/A"} m`,
    `Heading:     ${loc.headingDeg    ?? "N/A"} °`,
    `Speed (GPS): ${loc.speedKph      ?? "N/A"} km/h`,
    `Accuracy:    ${loc.accuracyM     ?? "N/A"} m`,
    `Last Updated:${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
  ];
  return lines.join("\n");
}

async function get_adas_status() {
  const doc = await getStatus();
  if (!doc || !doc.data?.adas) return noData("adas");

  const ads = doc.data.adas;

  const lines = [
    `ADAS Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Cruise Control:    ${ads.cruiseEnabled          ?? "N/A"}`,
    `Cruise Set Speed:  ${ads.cruiseSetSpeedKph      ?? "N/A"} km/h`,
    `Lane Keep Assist:  ${ads.laneKeepAssistOn       ?? "N/A"}`,
    `Collision Warning: ${ads.collisionWarningActive ?? "N/A"}${ads.collisionWarningActive ? "  ⚠ ALERT" : ""}`,
    `Last Updated:      ${doc.lastUpdated != null ? new Date(doc.lastUpdated).toISOString() : "N/A"}`,
  ];
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
