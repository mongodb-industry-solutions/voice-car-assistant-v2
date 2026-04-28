#!/usr/bin/env node

/**
 * Telemetry MCP Server
 * Provides MongoDB MCP tools for querying automotive telemetry data.
 * Supports two transports:
 *   MCP_TRANSPORT=sse  (default in Docker) — HTTP/SSE on PORT (default 3001)
 *   MCP_TRANSPORT=stdio                    — stdin/stdout for Claude Desktop etc.
 */

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { SSEServerTransport } from '@modelcontextprotocol/sdk/server/sse.js';
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';
import { MongoClient } from 'mongodb';
import express from 'express';

// ── Configuration ─────────────────────────────────────────────────────────────

const MONGODB_URI    = process.env.MONGODB_URI || process.env.NEXT_PUBLIC_MONGODB_URI;
const DATABASE_NAME  = process.env.DATABASE_NAME  || 'car_assistant_demo';
const COLLECTION_NAME = 'telemetry_snapshots';
const MCP_TRANSPORT  = process.env.MCP_TRANSPORT  || 'stdio';
const PORT           = parseInt(process.env.PORT   || '3001', 10);

// ── MongoDB ───────────────────────────────────────────────────────────────────

let collection = null;

async function connectToMongoDB() {
  if (!MONGODB_URI) throw new Error('MONGODB_URI environment variable is required');

  console.error('🔌 Connecting to MongoDB Atlas...');
  const client = new MongoClient(MONGODB_URI, {
    serverSelectionTimeoutMS: 5000,
    connectTimeoutMS: 10000,
  });
  await client.connect();
  collection = client.db(DATABASE_NAME).collection(COLLECTION_NAME);
  console.error(`✅ Connected — ${DATABASE_NAME}.${COLLECTION_NAME}`);

  process.on('SIGINT',  async () => { await client.close(); process.exit(0); });
  process.on('SIGTERM', async () => { await client.close(); process.exit(0); });
}

// ── Telemetry helpers ─────────────────────────────────────────────────────────

function getTelemetryData(doc) {
  if (!doc) return null;
  try {
    const parse = (v) => (typeof v === 'string' ? JSON.parse(v) : v);
    const batch = {};
    if (doc.engine_data)       batch.engine       = parse(doc.engine_data);
    if (doc.tire_data)         batch.tires        = parse(doc.tire_data);
    if (doc.battery_data)      batch.battery      = parse(doc.battery_data);
    if (doc.fuel_data)         batch.fuel         = parse(doc.fuel_data);
    if (doc.transmission_data) batch.transmission = parse(doc.transmission_data);
    if (doc.brake_data)        batch.brakes       = parse(doc.brake_data);
    return {
      vehicle_id: doc.vehicle_id,
      timestamp:  doc.timestamp,
      driving_mode: doc.driving_mode,
      anomaly_count: doc.anomaly_count,
      telemetry_batch: batch,
    };
  } catch (e) {
    console.error('Failed to parse telemetry data:', e);
    return null;
  }
}

// ── Tool implementations ──────────────────────────────────────────────────────

const tools = {
  async get_latest_telemetry() {
    const snap = await collection.findOne({}, { sort: { timestamp: -1 }, projection: { _id: 0 } });
    if (!snap) return { error: 'No telemetry data found' };
    const t = getTelemetryData(snap);
    if (!t) return { error: 'Failed to parse telemetry data' };
    return {
      vehicle_id:    t.vehicle_id,
      timestamp:     t.timestamp,
      driving_mode:  t.driving_mode,
      anomaly_count: t.anomaly_count,
      systems:       t.telemetry_batch,
    };
  },

  async check_system_status({ system_name }) {
    const valid = ['engine', 'battery', 'fuel', 'tires', 'transmission', 'brakes'];
    if (!valid.includes(system_name))
      return { error: `Invalid system. Must be one of: ${valid.join(', ')}` };

    const snap = await collection.findOne({}, { sort: { timestamp: -1 }, projection: { _id: 0 } });
    if (!snap) return { error: 'No telemetry data found' };
    const t = getTelemetryData(snap);
    if (!t?.telemetry_batch) return { error: 'Failed to parse telemetry data' };
    const data = t.telemetry_batch[system_name];
    if (!data) return { error: `System '${system_name}' not found` };
    return { vehicle_id: t.vehicle_id, timestamp: t.timestamp, system: system_name, sensors: data };
  },

  async get_tire_pressure() {
    const snap = await collection.findOne({}, { sort: { timestamp: -1 }, projection: { _id: 0 } });
    if (!snap) return { error: 'No tire data found' };
    const t = getTelemetryData(snap);
    if (!t?.telemetry_batch?.tires) return { error: 'No tire data in telemetry' };

    const pressures = {};
    for (const pos of ['front_left', 'front_right', 'rear_left', 'rear_right']) {
      const d = t.telemetry_batch.tires[pos];
      if (!d) continue;
      const status = d.pressure < 28 ? 'critical' : (d.pressure < 30 || d.pressure > 35) ? 'warning' : 'normal';
      pressures[pos] = { pressure: d.pressure, temperature: d.temp, unit: 'psi', temp_unit: '°C', status };
    }
    return { vehicle_id: t.vehicle_id, timestamp: t.timestamp, tire_pressures: pressures };
  },

  async get_anomalies() {
    const snap = await collection.findOne({}, { sort: { timestamp: -1 }, projection: { _id: 0 } });
    if (!snap) return { error: 'No telemetry data found' };
    const t = getTelemetryData(snap);
    if (!t?.telemetry_batch) return { error: 'Failed to parse telemetry data' };

    const anomalies = [];
    for (const [sysName, sysData] of Object.entries(t.telemetry_batch)) {
      const issues = [];
      if (sysName === 'tires') {
        for (const [tireName, tireData] of Object.entries(sysData)) {
          if (typeof tireData === 'object' && tireData.pressure) {
            const status = tireData.pressure < 28 ? 'critical' : (tireData.pressure < 30 || tireData.pressure > 35) ? 'warning' : null;
            if (status) issues.push({ sensor: `${tireName}_pressure`, value: tireData.pressure, unit: 'psi', status });
          }
        }
      } else {
        for (const [sensor, data] of Object.entries(sysData)) {
          if (typeof data === 'object' && (data.status === 'warning' || data.status === 'critical'))
            issues.push({ sensor, value: data.value, unit: data.unit, status: data.status, pid: data.pid });
        }
      }
      if (issues.length) anomalies.push({ system: sysName, issue_count: issues.length, issues });
    }
    return {
      vehicle_id: t.vehicle_id,
      timestamp: t.timestamp,
      total_anomalies: anomalies.reduce((s, a) => s + a.issue_count, 0),
      systems_affected: anomalies.length,
      anomalies,
    };
  },

  async query_telemetry_history({ minutes = 10, system = null }) {
    const startTime = Date.now() - minutes * 60_000;
    const snaps = await collection
      .find({ timestamp: { $gte: startTime } }, { projection: { _id: 0, vehicle_id: 1, timestamp: 1, anomaly_count: 1, engine_data: 1, tire_data: 1, battery_data: 1, fuel_data: 1, transmission_data: 1, brake_data: 1, driving_mode: 1 } })
      .sort({ timestamp: -1 })
      .limit(100)
      .toArray();

    const parsed = snaps.map(s => {
      const t = getTelemetryData(s);
      if (!t) return { timestamp: s.timestamp, vehicle_id: s.vehicle_id, error: 'parse failed' };
      const r = { timestamp: t.timestamp, vehicle_id: t.vehicle_id, driving_mode: t.driving_mode, anomaly_count: t.anomaly_count };
      if (system && t.telemetry_batch) r.system_data = t.telemetry_batch[system];
      else if (t.telemetry_batch) r.systems = t.telemetry_batch;
      return r;
    });

    return { query: { time_range_minutes: minutes, system: system || 'all', start_timestamp: startTime }, count: parsed.length, snapshots: parsed };
  },

  async get_telemetry_stats() {
    const total   = await collection.countDocuments();
    const latest  = await collection.findOne({}, { sort: { timestamp: -1 } });
    const oldest  = await collection.findOne({}, { sort: { timestamp:  1 } });
    const recent  = await collection.find({}, { projection: { anomaly_count: 1 }, sort: { timestamp: -1 }, limit: 100 }).toArray();
    const anomSum = recent.reduce((s, d) => s + (d.anomaly_count || 0), 0);
    return {
      total_snapshots:    total,
      recent_anomalies:   anomSum,
      latest_timestamp:   latest?.timestamp,
      oldest_timestamp:   oldest?.timestamp,
      time_range_ms:      (latest && oldest) ? latest.timestamp - oldest.timestamp : 0,
    };
  },
};

// ── MCP server factory ────────────────────────────────────────────────────────

const TOOL_SCHEMAS = [
  { name: 'get_latest_telemetry',    description: 'Get the most recent telemetry snapshot with all vehicle systems data', inputSchema: { type: 'object', properties: {} } },
  { name: 'check_system_status',     description: 'Check the status of a specific vehicle system (engine, battery, fuel, tires, transmission, brakes)', inputSchema: { type: 'object', properties: { system_name: { type: 'string', enum: ['engine','battery','fuel','tires','transmission','brakes'], description: 'Name of the system to check' } }, required: ['system_name'] } },
  { name: 'get_tire_pressure',       description: 'Get tire pressure readings for all four tires', inputSchema: { type: 'object', properties: {} } },
  { name: 'get_anomalies',           description: 'Get all current warnings and critical issues across all vehicle systems', inputSchema: { type: 'object', properties: {} } },
  { name: 'query_telemetry_history', description: 'Query historical telemetry data for a specific time range', inputSchema: { type: 'object', properties: { minutes: { type: 'number', description: 'Minutes of history to retrieve (default 10)', default: 10 }, system: { type: 'string', enum: ['engine','battery','fuel','tires','transmission','brakes'], description: 'Optional system filter' } } } },
  { name: 'get_telemetry_stats',     description: 'Get statistics about the telemetry database', inputSchema: { type: 'object', properties: {} } },
];

function createMcpServer() {
  const server = new Server(
    { name: 'telemetry-mcp-server', version: '1.0.0' },
    { capabilities: { tools: {} } },
  );

  server.setRequestHandler(ListToolsRequestSchema, async () => ({ tools: TOOL_SCHEMAS }));

  server.setRequestHandler(CallToolRequestSchema, async (req) => {
    const { name, arguments: args } = req.params;
    console.error(`🔧 Tool call: ${name}`, args);
    try {
      if (!tools[name]) throw new Error(`Unknown tool: ${name}`);
      const result = await tools[name](args || {});
      console.error(`✅ ${name} →`, JSON.stringify(result).substring(0, 150));
      return { content: [{ type: 'text', text: JSON.stringify(result, null, 2) }] };
    } catch (e) {
      console.error(`❌ ${name} error:`, e);
      return { content: [{ type: 'text', text: JSON.stringify({ error: e.message }) }], isError: true };
    }
  });

  return server;
}

// ── Entry point ───────────────────────────────────────────────────────────────

async function main() {
  console.error('🚗 Telemetry MCP Server starting...');
  await connectToMongoDB();

  if (MCP_TRANSPORT === 'sse') {
    // ── HTTP/SSE mode (used by LangChain agent and Docker) ──────────────────
    const app = express();
    const activeTransports = {};

    app.get('/sse', async (req, res) => {
      const transport = new SSEServerTransport('/messages', res);
      activeTransports[transport.sessionId] = transport;

      res.on('close', () => {
        delete activeTransports[transport.sessionId];
        console.error(`🔌 SSE client disconnected (${transport.sessionId})`);
      });

      const server = createMcpServer();
      await server.connect(transport);
      console.error(`🔌 SSE client connected (${transport.sessionId})`);
    });

    app.post('/messages', express.json(), async (req, res) => {
      const { sessionId } = req.query;
      const transport = activeTransports[sessionId];
      if (!transport) { res.status(404).json({ error: 'Session not found' }); return; }
      await transport.handlePostMessage(req, res);
    });

    app.get('/health', (_req, res) => res.json({ status: 'ok', service: 'telemetry-mcp-server', transport: 'sse' }));

    app.listen(PORT, () => {
      console.error(`✅ Telemetry MCP SSE server listening on port ${PORT}`);
      console.error(`   Available tools: ${TOOL_SCHEMAS.length}`);
    });

  } else {
    // ── stdio mode (Claude Desktop / GitHub Copilot / CLI) ─────────────────
    const server = createMcpServer();
    const transport = new StdioServerTransport();
    await server.connect(transport);
    console.error(`✅ Telemetry MCP Server ready (stdio) — ${TOOL_SCHEMAS.length} tools`);
  }
}

main().catch((e) => { console.error('❌ Fatal error:', e); process.exit(1); });
