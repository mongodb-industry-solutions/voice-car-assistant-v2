#!/usr/bin/env node

/**
 * VSS Telemetry MCP Server
 * Exposes 9 MCP tools for querying VSS vehicle telemetry data stored in MongoDB Atlas
 * (synced there by ObjectBox Sync Server). Runs in SSE transport mode on port 3002
 * and also provides a /tools/<name> REST bypass endpoint.
 */

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { SSEServerTransport } from "@modelcontextprotocol/sdk/server/sse.js";
import { MongoClient } from "mongodb";
import express from "express";
import { z } from "zod";

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

function fmt(obj) {
  return JSON.stringify(obj, null, 2);
}

// Strip CR/LF and other control characters from any value before it touches a log line.
function sanitizeForLog(value) {
  return String(value).replace(/[\r\n\x00-\x1f\x7f]/g, " ").slice(0, 200);
}

// ── Tool implementations ───────────────────────────────────────────────────────

async function get_vehicle_status() {
  const db = await getDb();

  const [
    meta,
    powertrain,
    battery,
    chassis,
    cabin,
    location,
    adas,
  ] = await Promise.all([
    db.collection("VehicleMeta")    .findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
    db.collection("PowertrainState").findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
    db.collection("BatteryState")   .findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
    db.collection("ChassisState")   .findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
    db.collection("CabinState")     .findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
    db.collection("LocationState")  .findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
    db.collection("AdasState")      .findOne({ vehicleId: VEHICLE_ID }, { projection: { _id: 0 } }),
  ]);

  if (!meta && !powertrain && !battery && !chassis && !cabin && !location && !adas) {
    return noData();
  }

  const summary = {
    vehicleId: VEHICLE_ID,
    meta: meta || null,
    states: {
      powertrain: powertrain || null,
      battery:    battery    || null,
      chassis:    chassis    || null,
      cabin:      cabin      || null,
      location:   location   || null,
      adas:       adas       || null,
    },
  };

  const lines = [`VSS Vehicle Status — ${VEHICLE_ID}`];
  lines.push("=".repeat(50));

  if (meta) {
    lines.push(`OEM: ${meta.oem || "N/A"}  Model: ${meta.model || "N/A"}  VIN: ${meta.vin || "N/A"}`);
    lines.push(`Powertrain: ${meta.powertrainType || "N/A"}  Drivetrain: ${meta.drivetrainType || "N/A"}  Wheelbase: ${meta.wheelbaseMm ?? "N/A"} mm  Curb Weight: ${meta.curbWeightKg ?? "N/A"} kg`);
    lines.push(`Fuel Tank: ${meta.fuelTankCapacityL ?? "N/A"} L  Battery Capacity: ${meta.batteryCapacityKwh ?? "N/A"} kWh`);
  }
  if (powertrain) {
    lines.push(`Speed: ${powertrain.speedKph ?? "N/A"} km/h  RPM: ${powertrain.engineRpm ?? "N/A"}  Fuel: ${powertrain.fuelLevelPct ?? "N/A"}%`);
  }
  if (battery) {
    const charging = battery.chargingState === "charging" ? "charging" : "not charging";
    lines.push(`Battery SOC: ${battery.socPct ?? "N/A"}%  Est. Range: ${battery.estimatedRangeKm ?? "N/A"} km  Charging: ${charging}`);
  }
  if (location) {
    const lcoords = location.locationGeoJson ? JSON.parse(location.locationGeoJson).coordinates : null;
    const llat = lcoords ? lcoords[1] : "N/A";
    const llon = lcoords ? lcoords[0] : "N/A";
    lines.push(`Location: ${llat}, ${llon}  Heading: ${location.headingDeg ?? "N/A"}°`);
  }
  if (cabin) {
    lines.push(`Interior Temp: ${cabin.insideTempC ?? "N/A"}°C  HVAC: ${cabin.hvacMode ?? "N/A"}`);
  }
  if (adas) {
    lines.push(`Cruise Control: ${adas.cruiseEnabled ?? "N/A"}  LKA: ${adas.laneKeepAssistOn ?? "N/A"}  Collision Warning: ${adas.collisionWarningActive ?? "N/A"}`);
  }

  lines.push("", "Full data:", fmt(summary));
  return lines.join("\n");
}

async function get_powertrain_status() {
  const db  = await getDb();
  const doc = await db.collection("PowertrainState").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
  if (!doc) return noData("PowertrainState");

  const lines = [
    `Powertrain Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Speed:         ${doc.speedKph             ?? "N/A"} km/h`,
    `Engine RPM:    ${doc.engineRpm            ?? "N/A"} rpm`,
    `Fuel Level:    ${doc.fuelLevelPct         ?? "N/A"} %`,
    `Coolant Temp:  ${doc.coolantTempC         ?? "N/A"} °C${doc.coolantTempC != null ? (doc.coolantTempC > 110 ? "  ⚠ OVERHEATING" : doc.coolantTempC > 95 ? "  (warm)" : "  (normal)") : ""}`,
    `Gear:          ${doc.gear                 ?? "N/A"}`,
    `Throttle:      ${doc.throttlePct          ?? "N/A"} %`,
    `Odometer:      ${doc.odometerKm           ?? "N/A"} km`,
    `Ignition On:   ${doc.ignitionOn           ?? "N/A"}`,
    `Last Updated:  ${doc.updatedAt != null ? new Date(doc.updatedAt).toISOString() : "N/A"}`,
    "",
    "Raw document:",
    fmt(doc),
  ];
  return lines.join("\n");
}

async function get_battery_status() {
  const db  = await getDb();
  const doc = await db.collection("BatteryState").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
  if (!doc) return noData("BatteryState");

  const isCharging = doc.chargingState === "charging";
  const chargingStatus = isCharging
    ? `Charging at ${doc.chargingPowerKw ?? "N/A"} kW`
    : "Not charging";

  const socLabel =
    doc.socPct != null
      ? doc.socPct < 15 ? "  ⚠ CRITICALLY LOW"
      : doc.socPct < 25 ? "  (low)"
      : doc.socPct > 90 ? "  (full)"
      : "  (normal)"
      : "";

  const lines = [
    `Battery Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `State of Charge: ${doc.socPct             ?? "N/A"} %${socLabel}`,
    `Estimated Range: ${doc.estimatedRangeKm   ?? "N/A"} km`,
    `Charging Status: ${chargingStatus}`,
    `Voltage:         ${doc.voltageV           ?? "N/A"} V`,
    `Current:         ${doc.currentA           ?? "N/A"} A`,
    `Temp:            ${doc.batteryTempC       ?? "N/A"} °C`,
    `State of Health: ${doc.sohPct             ?? "N/A"} %`,
    `Last Updated:    ${doc.updatedAt != null ? new Date(doc.updatedAt).toISOString() : "N/A"}`,
    "",
    "Raw document:",
    fmt(doc),
  ];
  return lines.join("\n");
}

async function get_chassis_status() {
  const db  = await getDb();
  const doc = await db.collection("ChassisState").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
  if (!doc) return noData("ChassisState");

  const tireSummary = (position, kpa) => {
    if (kpa == null) return `${position}: N/A`;
    const psi = (kpa / 6.895).toFixed(1);
    const status = psi < 28 ? "⚠ CRITICAL" : psi < 30 || psi > 35 ? "⚠ WARNING" : "OK";
    return `${position}: ${psi} psi (${kpa} kPa)  [${status}]`;
  };

  const lines = [
    `Chassis Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    "Tire Pressures:",
    `  ${tireSummary("Front-Left ", doc.tirePressureFlKpa)}`,
    `  ${tireSummary("Front-Right", doc.tirePressureFrKpa)}`,
    `  ${tireSummary("Rear-Left  ", doc.tirePressureRlKpa)}`,
    `  ${tireSummary("Rear-Right ", doc.tirePressureRrKpa)}`,
    `ABS Active:            ${doc.absActive              ?? "N/A"}`,
    `Traction Control:      ${doc.tractionControlActive  ?? "N/A"}`,
    `Brake Pedal:           ${doc.brakePedalPct          ?? "N/A"} %`,
    `Last Updated:          ${doc.updatedAt != null ? new Date(doc.updatedAt).toISOString() : "N/A"}`,
    "",
    "Raw document:",
    fmt(doc),
  ];
  return lines.join("\n");
}

async function get_cabin_status() {
  const db  = await getDb();
  const doc = await db.collection("CabinState").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
  if (!doc) return noData("CabinState");

  const lockStatus = doc.doorsLocked != null ? (doc.doorsLocked ? "locked" : "unlocked") : "N/A";
  const doors = [
    `Driver (FL):    ${doc.driverDoorOpen    != null ? (doc.driverDoorOpen    ? "open" : "closed") : "N/A"} / ${lockStatus}`,
    `Passenger (FR): ${doc.passengerDoorOpen != null ? (doc.passengerDoorOpen ? "open" : "closed") : "N/A"} / ${lockStatus}`,
    `Rear-Left:      ${doc.rearLeftDoorOpen  != null ? (doc.rearLeftDoorOpen  ? "open" : "closed") : "N/A"} / ${lockStatus}`,
    `Rear-Right:     ${doc.rearRightDoorOpen != null ? (doc.rearRightDoorOpen ? "open" : "closed") : "N/A"} / ${lockStatus}`,
  ];

  const lines = [
    `Cabin Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Interior Temp:        ${doc.insideTempC              ?? "N/A"} °C`,
    `Outside Temp:         ${doc.outsideTempC             ?? "N/A"} °C`,
    `HVAC Mode:            ${doc.hvacMode                 ?? "N/A"}`,
    `Fan Speed:            ${doc.fanSpeed                 ?? "N/A"}`,
    `Seatbelt (Driver):    ${doc.seatbeltDriverFastened   ?? "N/A"}`,
    "Doors (open / all-locks status):",
    ...doors.map(d => `  ${d}`),
    `Last Updated: ${doc.updatedAt != null ? new Date(doc.updatedAt).toISOString() : "N/A"}`,
    "",
    "Raw document:",
    fmt(doc),
  ];
  return lines.join("\n");
}

async function get_location() {
  const db  = await getDb();
  const doc = await db.collection("LocationState").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
  if (!doc) return noData("LocationState");

  const coords = doc.locationGeoJson ? JSON.parse(doc.locationGeoJson).coordinates : null;
  const lon = coords ? coords[0] : null;
  const lat = coords ? coords[1] : null;

  const lines = [
    `Location — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Latitude:    ${lat      ?? "N/A"}`,
    `Longitude:   ${lon      ?? "N/A"}`,
    `Altitude:    ${doc.altitudeM     ?? "N/A"} m`,
    `Heading:     ${doc.headingDeg    ?? "N/A"} °`,
    `Speed (GPS): ${doc.speedKph      ?? "N/A"} km/h`,
    `Geohash:     ${doc.geohash       ?? "N/A"}`,
    `Accuracy:    ${doc.accuracyM     ?? "N/A"} m`,
    `Last Updated:${doc.updatedAt != null ? new Date(doc.updatedAt).toISOString() : "N/A"}`,
    "",
    "Raw document:",
    fmt(doc),
  ];
  return lines.join("\n");
}

async function get_adas_status() {
  const db  = await getDb();
  const doc = await db.collection("AdasState").findOne(
    { vehicleId: VEHICLE_ID },
    { projection: { _id: 0 } }
  );
  if (!doc) return noData("AdasState");

  const lines = [
    `ADAS Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Cruise Control:          ${doc.cruiseEnabled          ?? "N/A"}`,
    `Cruise Set Speed:        ${doc.cruiseSetSpeedKph      ?? "N/A"} km/h`,
    `Lane Keep Assist:        ${doc.laneKeepAssistOn       ?? "N/A"}`,
    `Collision Warning:       ${doc.collisionWarningActive ?? "N/A"}${doc.collisionWarningActive ? "  ⚠ ALERT" : ""}`,
    `Parking Assist:          ${doc.parkingAssistOn        ?? "N/A"}`,
    `Autopilot Mode:          ${doc.autopilotMode          ?? "N/A"}`,
    `Last Updated:            ${doc.updatedAt != null ? new Date(doc.updatedAt).toISOString() : "N/A"}`,
    "",
    "Raw document:",
    fmt(doc),
  ];
  return lines.join("\n");
}

async function get_vehicle_events({ minutes = 30, severity = "all" } = {}) {
  const db      = await getDb();
  const cutoff  = Date.now() - minutes * 60 * 1000;

  const filter = { vehicleId: VEHICLE_ID, ts: { $gte: cutoff } };
  if (severity && severity !== "all") {
    filter.severity = severity;
  }

  const events = await db.collection("VehicleEvent")
    .find(filter, { projection: { _id: 0 } })
    .sort({ ts: -1 })
    .limit(50)
    .toArray();

  if (events.length === 0) {
    return `No events found for vehicle ${VEHICLE_ID} in the last ${minutes} minute(s)${severity !== "all" ? ` with severity "${severity}"` : ""}.`;
  }

  const lines = [
    `Vehicle Events — ${VEHICLE_ID}`,
    `Query: last ${minutes} min  |  severity: ${severity}  |  found: ${events.length}`,
    "=".repeat(50),
    ...events.map(e => {
      const ts = e.ts != null ? new Date(e.ts).toISOString() : "N/A";
      return `[${ts}] [${(e.severity || "info").toUpperCase()}] ${e.eventType || "event"}: ${e.message || fmt(e)}`;
    }),
    "",
    "Raw events:",
    fmt(events),
  ];
  return lines.join("\n");
}

async function get_driving_history({ domain = "powertrain", minutes = 10 } = {}) {
  const domainToCollection = {
    powertrain: "PowertrainSample",
    battery:    "BatterySample",
    location:   "LocationSample",
    cabin:      "CabinSample",
    adas:       "AdasSample",
  };

  const collectionName = domainToCollection[domain];
  if (!collectionName) {
    return `Unknown domain "${domain}". Valid options: ${Object.keys(domainToCollection).join(", ")}.`;
  }

  const db     = await getDb();
  const cutoff = Date.now() - minutes * 60 * 1000;

  const samples = await db.collection(collectionName)
    .find({ vehicleId: VEHICLE_ID, ts: { $gte: cutoff } }, { projection: { _id: 0 } })
    .sort({ ts: -1 })
    .limit(100)
    .toArray();

  if (samples.length === 0) {
    return `No ${domain} history found for vehicle ${VEHICLE_ID} in the last ${minutes} minute(s).`;
  }

  const lines = [
    `Driving History — ${domain} — ${VEHICLE_ID}`,
    `Query: last ${minutes} min  |  collection: ${collectionName}  |  samples: ${samples.length}`,
    "=".repeat(50),
    fmt(samples),
  ];
  return lines.join("\n");
}

// ── Tool dispatch map ──────────────────────────────────────────────────────────

const toolHandlers = {
  get_vehicle_status:   (_args) => get_vehicle_status(),
  get_powertrain_status:(_args) => get_powertrain_status(),
  get_battery_status:   (_args) => get_battery_status(),
  get_chassis_status:   (_args) => get_chassis_status(),
  get_cabin_status:     (_args) => get_cabin_status(),
  get_location:         (_args) => get_location(),
  get_adas_status:      (_args) => get_adas_status(),
  get_vehicle_events:   (args)  => get_vehicle_events(args),
  get_driving_history:  (args)  => get_driving_history(args),
};

// ── MCP Server setup ───────────────────────────────────────────────────────────

const server = new McpServer({
  name: "vss-telemetry-mcp-server",
  version: "1.0.0",
});

// 1. get_vehicle_status
server.tool(
  "get_vehicle_status",
  "Get a full snapshot of the vehicle: VehicleMeta plus all current State entities (powertrain, battery, chassis, cabin, location, ADAS).",
  {},
  async () => {
    try {
      const text = await get_vehicle_status();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 2. get_powertrain_status
server.tool(
  "get_powertrain_status",
  "Get current powertrain state: speed, RPM, fuel level, coolant temperature, transmission gear, throttle, and odometer.",
  {},
  async () => {
    try {
      const text = await get_powertrain_status();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 3. get_battery_status
server.tool(
  "get_battery_status",
  "Get current battery state: state of charge (SOC%), estimated range, charging status, voltage, current, temperature, and state of health.",
  {},
  async () => {
    try {
      const text = await get_battery_status();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 4. get_chassis_status
server.tool(
  "get_chassis_status",
  "Get current chassis state: tire pressures for all four tires (with warnings), ABS status, ESC status, and brake fluid level.",
  {},
  async () => {
    try {
      const text = await get_chassis_status();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 5. get_cabin_status
server.tool(
  "get_cabin_status",
  "Get current cabin state: door open/lock status for all doors, temperature setpoint, interior temperature, HVAC, fan speed, and windows.",
  {},
  async () => {
    try {
      const text = await get_cabin_status();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 6. get_location
server.tool(
  "get_location",
  "Get current vehicle location: latitude, longitude, altitude, heading, GPS speed, and geohash.",
  {},
  async () => {
    try {
      const text = await get_location();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 7. get_adas_status
server.tool(
  "get_adas_status",
  "Get current ADAS state: cruise control, lane keep assist, lane departure warning, collision warning, blind spot warnings, and automatic emergency braking.",
  {},
  async () => {
    try {
      const text = await get_adas_status();
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 8. get_vehicle_events
server.tool(
  "get_vehicle_events",
  "Query recent vehicle events (warnings, alerts, critical notices). Filter by time window and optionally by severity.",
  {
    minutes:  z.number().optional().default(30).describe("How many minutes back to search (default 30)"),
    severity: z.enum(["warning", "critical", "all"]).optional().default("all").describe("Filter by severity: 'warning', 'critical', or 'all' (default)"),
  },
  async ({ minutes, severity }) => {
    try {
      const text = await get_vehicle_events({ minutes, severity });
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// 9. get_driving_history
server.tool(
  "get_driving_history",
  "Retrieve time-series samples from a specific telemetry domain over the past N minutes (up to 100 samples).",
  {
    domain:  z.enum(["powertrain", "battery", "location", "cabin", "adas"]).describe("Which sample domain to query"),
    minutes: z.number().optional().default(10).describe("How many minutes back to search (default 10)"),
  },
  async ({ domain, minutes }) => {
    try {
      const text = await get_driving_history({ domain, minutes });
      return { content: [{ type: "text", text }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

// ── Express app ────────────────────────────────────────────────────────────────

const app = express();

// SSE transport — one transport per connected client
const transports = {};

app.get("/sse", async (req, res) => {
  const transport = new SSEServerTransport("/messages", res);
  transports[transport.sessionId] = transport;

  res.on("close", () => {
    delete transports[transport.sessionId];
    console.log(`SSE client disconnected (${sanitizeForLog(transport.sessionId)})`);
  });

  await server.connect(transport);
  console.log(`SSE client connected (${sanitizeForLog(transport.sessionId)})`);
});

app.post("/messages", express.json(), async (req, res) => {
  const sessionId   = req.query.sessionId;
  const transport   = transports[sessionId];
  if (!transport) {
    res.status(404).json({ error: "Session not found" });
    return;
  }
  await transport.handlePostMessage(req, res);
});

// REST bypass endpoint — allows non-MCP clients (e.g. LangChain agent) to call
// tools directly via HTTP POST /tools/<toolName>
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

// Health endpoint
app.get("/health", async (req, res) => {
  let dbStatus = "unknown";
  let dbLatencyMs = null;

  try {
    const t0 = Date.now();
    const db = await getDb();
    await db.command({ ping: 1 });
    dbLatencyMs = Date.now() - t0;
    dbStatus = "connected";
  } catch (e) {
    dbStatus = `error: ${e.message}`;
  }

  res.json({
    status:     "healthy",
    service:    "vss-telemetry-mcp-server",
    version:    "1.0.0",
    transport:  "sse",
    port:       PORT,
    vehicleId:  VEHICLE_ID,
    database:   DATABASE_NAME || "(not set)",
    db:         { status: dbStatus, latencyMs: dbLatencyMs },
    tools:      Object.keys(toolHandlers),
    timestamp:  new Date().toISOString(),
  });
});

// ── Start ──────────────────────────────────────────────────────────────────────

app.listen(PORT, () => {
  console.log(`VSS Telemetry MCP Server running on port ${PORT}`);
  console.log(`  Vehicle ID : ${VEHICLE_ID}`);
  console.log(`  Database   : ${DATABASE_NAME || "(set DATABASE_NAME env var)"}`);
  console.log(`  Endpoints  : GET /sse  POST /messages  POST /tools/:name  GET /health`);
  console.log(`  Tools      : ${Object.keys(toolHandlers).join(", ")}`);
});
