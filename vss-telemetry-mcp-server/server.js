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
    lines.push(`VIN: ${meta.vin || "N/A"}  Model: ${meta.model || "N/A"}  Year: ${meta.year || "N/A"}`);
  }
  if (powertrain) {
    lines.push(`Speed: ${powertrain.speedKmh ?? "N/A"} km/h  RPM: ${powertrain.engineRpm ?? "N/A"}  Fuel: ${powertrain.fuelLevelPct ?? "N/A"}%`);
  }
  if (battery) {
    lines.push(`Battery SOC: ${battery.socPct ?? "N/A"}%  Est. Range: ${battery.estimatedRangeKm ?? "N/A"} km  Charging: ${battery.isCharging ?? "N/A"}`);
  }
  if (location) {
    lines.push(`Location: ${location.lat ?? "N/A"}, ${location.lon ?? "N/A"}  Heading: ${location.headingDeg ?? "N/A"}°`);
  }
  if (cabin) {
    lines.push(`Cabin Temp: ${cabin.tempSetpointC ?? "N/A"}°C  HVAC: ${cabin.hvacOn ?? "N/A"}`);
  }
  if (adas) {
    lines.push(`Cruise Control: ${adas.cruiseControlActive ?? "N/A"}  LKA: ${adas.laneKeepActive ?? "N/A"}  Collision Warning: ${adas.collisionWarning ?? "N/A"}`);
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
    `Speed:         ${doc.speedKmh             ?? "N/A"} km/h`,
    `Engine RPM:    ${doc.engineRpm            ?? "N/A"} rpm`,
    `Fuel Level:    ${doc.fuelLevelPct         ?? "N/A"} %`,
    `Coolant Temp:  ${doc.coolantTempC         ?? "N/A"} °C${doc.coolantTempC != null ? (doc.coolantTempC > 110 ? "  ⚠ OVERHEATING" : doc.coolantTempC > 95 ? "  (warm)" : "  (normal)") : ""}`,
    `Transmission:  ${doc.transmissionGear     ?? "N/A"}`,
    `Throttle:      ${doc.throttlePct          ?? "N/A"} %`,
    `Odometer:      ${doc.odometerKm           ?? "N/A"} km`,
    `Engine On:     ${doc.engineOn             ?? "N/A"}`,
    `Last Updated:  ${doc.ts != null ? new Date(doc.ts).toISOString() : "N/A"}`,
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

  const chargingStatus = doc.isCharging
    ? `Charging at ${doc.chargeRateKw ?? "N/A"} kW`
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
    `Temp:            ${doc.tempC              ?? "N/A"} °C`,
    `State of Health: ${doc.sohPct             ?? "N/A"} %`,
    `Capacity:        ${doc.capacityKwh        ?? "N/A"} kWh`,
    `Last Updated:    ${doc.ts != null ? new Date(doc.ts).toISOString() : "N/A"}`,
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

  const tireSummary = (position, psi) => {
    if (psi == null) return `${position}: N/A`;
    const status = psi < 28 ? "⚠ CRITICAL" : psi < 30 || psi > 35 ? "⚠ WARNING" : "OK";
    return `${position}: ${psi} psi  [${status}]`;
  };

  const lines = [
    `Chassis Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    "Tire Pressures:",
    `  ${tireSummary("Front-Left ", doc.tirePressureFLPsi)}`,
    `  ${tireSummary("Front-Right", doc.tirePressureFRPsi)}`,
    `  ${tireSummary("Rear-Left  ", doc.tirePressureRLPsi)}`,
    `  ${tireSummary("Rear-Right ", doc.tirePressureRRPsi)}`,
    `ABS Active:       ${doc.absActive        ?? "N/A"}`,
    `ESC Active:       ${doc.escActive        ?? "N/A"}`,
    `Brake Fluid:      ${doc.brakeFluidPct    ?? "N/A"} %`,
    `Suspension Mode:  ${doc.suspensionMode   ?? "N/A"}`,
    `Last Updated:     ${doc.ts != null ? new Date(doc.ts).toISOString() : "N/A"}`,
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

  const doors = [
    `FL: ${doc.doorFLOpen != null ? (doc.doorFLOpen ? "open" : "closed") : "N/A"} / ${doc.doorFLLocked != null ? (doc.doorFLLocked ? "locked" : "unlocked") : "N/A"}`,
    `FR: ${doc.doorFROpen != null ? (doc.doorFROpen ? "open" : "closed") : "N/A"} / ${doc.doorFRLocked != null ? (doc.doorFRLocked ? "locked" : "unlocked") : "N/A"}`,
    `RL: ${doc.doorRLOpen != null ? (doc.doorRLOpen ? "open" : "closed") : "N/A"} / ${doc.doorRLLocked != null ? (doc.doorRLLocked ? "locked" : "unlocked") : "N/A"}`,
    `RR: ${doc.doorRROpen != null ? (doc.doorRROpen ? "open" : "closed") : "N/A"} / ${doc.doorRRLocked != null ? (doc.doorRRLocked ? "locked" : "unlocked") : "N/A"}`,
  ];

  const lines = [
    `Cabin Status — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Temperature Setpoint: ${doc.tempSetpointC   ?? "N/A"} °C`,
    `Interior Temp:        ${doc.interiorTempC   ?? "N/A"} °C`,
    `HVAC On:              ${doc.hvacOn          ?? "N/A"}`,
    `Fan Speed:            ${doc.fanSpeed        ?? "N/A"}`,
    `Seat Heating (Driver):${doc.seatHeatDriver  ?? "N/A"}`,
    `Sunroof Open:         ${doc.sunroofOpen     ?? "N/A"}`,
    `Windows:              FL=${doc.windowFLPct  ?? "N/A"}%  FR=${doc.windowFRPct ?? "N/A"}%  RL=${doc.windowRLPct ?? "N/A"}%  RR=${doc.windowRRPct ?? "N/A"}%`,
    "Doors (open / lock status):",
    ...doors.map(d => `  ${d}`),
    `Last Updated: ${doc.ts != null ? new Date(doc.ts).toISOString() : "N/A"}`,
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

  const lines = [
    `Location — ${VEHICLE_ID}`,
    "=".repeat(50),
    `Latitude:    ${doc.lat          ?? "N/A"}`,
    `Longitude:   ${doc.lon          ?? "N/A"}`,
    `Altitude:    ${doc.altitudeM    ?? "N/A"} m`,
    `Heading:     ${doc.headingDeg   ?? "N/A"} °`,
    `Speed (GPS): ${doc.speedKmh     ?? "N/A"} km/h`,
    `Geohash:     ${doc.geohash      ?? "N/A"}`,
    `GPS Fix:     ${doc.gpsFix       ?? "N/A"}`,
    `Accuracy:    ${doc.accuracyM    ?? "N/A"} m`,
    `Last Updated:${doc.ts != null ? new Date(doc.ts).toISOString() : "N/A"}`,
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
    `Cruise Control Active:   ${doc.cruiseControlActive    ?? "N/A"}`,
    `Cruise Set Speed:        ${doc.cruiseSetSpeedKmh      ?? "N/A"} km/h`,
    `Lane Keep Assist:        ${doc.laneKeepActive         ?? "N/A"}`,
    `Lane Departure Warning:  ${doc.laneDepartureWarning   ?? "N/A"}`,
    `Collision Warning:       ${doc.collisionWarning       ?? "N/A"}${doc.collisionWarning ? "  ⚠ ALERT" : ""}`,
    `Forward Collision TTC:   ${doc.collisionTtcS          ?? "N/A"} s`,
    `Blind Spot Warning L:    ${doc.blindSpotLeft          ?? "N/A"}`,
    `Blind Spot Warning R:    ${doc.blindSpotRight         ?? "N/A"}`,
    `Parking Sensors Active:  ${doc.parkingSensorsActive   ?? "N/A"}`,
    `Auto Emergency Braking:  ${doc.aebActive              ?? "N/A"}`,
    `Last Updated:            ${doc.ts != null ? new Date(doc.ts).toISOString() : "N/A"}`,
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
    console.log(`SSE client disconnected (${transport.sessionId})`);
  });

  await server.connect(transport);
  console.log(`SSE client connected (${transport.sessionId})`);
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
    console.error(`Tool error [${toolName}]:`, e);
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
