#!/usr/bin/env node

/**
 * Telemetry MCP Server
 * Provides MongoDB MCP tools for querying automotive telemetry data
 * Following MongoDB Industry Solutions best practices
 */

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';
import { MongoClient } from 'mongodb';

// Server configuration from environment variables
const MONGODB_URI = process.env.MONGODB_URI || process.env.NEXT_PUBLIC_MONGODB_URI;
const DATABASE_NAME = process.env.DATABASE_NAME || 'car_assistant_demo';
const COLLECTION_NAME = 'telemetry_snapshots';

// Initialize MongoDB client
let mongoClient = null;
let db = null;
let collection = null;

/**
 * Connect to MongoDB Atlas
 */
async function connectToMongoDB() {
  try {
    if (!MONGODB_URI) {
      throw new Error('MONGODB_URI environment variable is required');
    }

    console.error('🔌 Connecting to MongoDB Atlas...');
    mongoClient = new MongoClient(MONGODB_URI, {
      serverSelectionTimeoutMS: 5000,
      connectTimeoutMS: 10000,
    });

    await mongoClient.connect();
    db = mongoClient.db(DATABASE_NAME);
    collection = db.collection(COLLECTION_NAME);
    
    console.error('✅ Connected to MongoDB Atlas');
    console.error(`   Database: ${DATABASE_NAME}`);
    console.error(`   Collection: ${COLLECTION_NAME}`);
    
    return true;
  } catch (error) {
    console.error('❌ MongoDB connection failed:', error.message);
    return false;
  }
}

/**
 * Helper function to reconstruct telemetry_batch from per-system fields (Option B)
 */
function getTelemetryData(document) {
  if (!document) return null;
  
  try {
    // Parse each system's JSON field
    const telemetry_batch = {};
    
    if (document.engine_data) {
      telemetry_batch.engine = typeof document.engine_data === 'string' 
        ? JSON.parse(document.engine_data) 
        : document.engine_data;
    }
    
    if (document.tire_data) {
      telemetry_batch.tires = typeof document.tire_data === 'string' 
        ? JSON.parse(document.tire_data) 
        : document.tire_data;
    }
    
    if (document.battery_data) {
      telemetry_batch.battery = typeof document.battery_data === 'string' 
        ? JSON.parse(document.battery_data) 
        : document.battery_data;
    }
    
    if (document.fuel_data) {
      telemetry_batch.fuel = typeof document.fuel_data === 'string' 
        ? JSON.parse(document.fuel_data) 
        : document.fuel_data;
    }
    
    if (document.transmission_data) {
      telemetry_batch.transmission = typeof document.transmission_data === 'string' 
        ? JSON.parse(document.transmission_data) 
        : document.transmission_data;
    }
    
    if (document.brake_data) {
      telemetry_batch.brakes = typeof document.brake_data === 'string' 
        ? JSON.parse(document.brake_data) 
        : document.brake_data;
    }
    
    return {
      vehicle_id: document.vehicle_id,
      timestamp: document.timestamp,
      driving_mode: document.driving_mode,
      anomaly_count: document.anomaly_count,
      telemetry_batch: telemetry_batch
    };
  } catch (error) {
    console.error('Failed to parse telemetry data:', error);
    return null;
  }
}

/**
 * Tool implementations for telemetry queries
 */
const tools = {
  /**
   * Get the latest telemetry snapshot with all systems
   */
  async get_latest_telemetry() {
    const snapshot = await collection.findOne(
      {},
      {
        sort: { timestamp: -1 },
        projection: { _id: 0 }
      }
    );

    if (!snapshot) {
      return { error: 'No telemetry data found' };
    }

    const telemetry = getTelemetryData(snapshot);
    if (!telemetry) {
      return { error: 'Failed to parse telemetry data' };
    }

    return {
      vehicle_id: telemetry.vehicle_id || snapshot.vehicle_id,
      timestamp: telemetry.timestamp || snapshot.timestamp,
      driving_mode: telemetry.driving_mode,
      anomaly_count: telemetry.anomaly_count || snapshot.anomaly_count,
      systems: telemetry.telemetry_batch
    };
  },

  /**
   * Check specific system status (engine, battery, fuel, tires, transmission, brakes)
   */
  async check_system_status({ system_name }) {
    const validSystems = ['engine', 'battery', 'fuel', 'tires', 'transmission', 'brakes'];
    
    if (!validSystems.includes(system_name)) {
      return { error: `Invalid system. Must be one of: ${validSystems.join(', ')}` };
    }

    const snapshot = await collection.findOne(
      {},
      {
        sort: { timestamp: -1 },
        projection: { _id: 0 }
      }
    );

    if (!snapshot) {
      return { error: 'No telemetry data found' };
    }

    const telemetry = getTelemetryData(snapshot);
    if (!telemetry || !telemetry.telemetry_batch) {
      return { error: 'Failed to parse telemetry data' };
    }

    const systemData = telemetry.telemetry_batch[system_name];
    if (!systemData) {
      return { error: `System '${system_name}' not found in telemetry data` };
    }

    return {
      vehicle_id: telemetry.vehicle_id,
      timestamp: telemetry.timestamp,
      system: system_name,
      sensors: systemData
    };
  },

  /**
   * Get tire pressure for all four tires with latest readings
   */
  async get_tire_pressure() {
    const snapshot = await collection.findOne(
      {},
      {
        sort: { timestamp: -1 },
        projection: { _id: 0 }
      }
    );

    if (!snapshot) {
      return { error: 'No tire data found' };
    }

    const telemetry = getTelemetryData(snapshot);
    if (!telemetry || !telemetry.telemetry_batch?.tires) {
      return { error: 'No tire data found in telemetry' };
    }

    const tires = telemetry.telemetry_batch.tires;
    const tire_pressures = {};

    // Extract pressure and temperature for each tire
    const tirePositions = ['front_left', 'front_right', 'rear_left', 'rear_right'];
    for (const position of tirePositions) {
      if (tires[position]) {
        const tireData = tires[position];
        
        // Determine status based on pressure thresholds
        let status = 'normal';
        if (tireData.pressure < 28) {
          status = 'critical';
        } else if (tireData.pressure < 30 || tireData.pressure > 35) {
          status = 'warning';
        }

        tire_pressures[position] = {
          pressure: tireData.pressure,
          temperature: tireData.temp,
          unit: 'psi',
          temp_unit: '°C',
          status: status
        };
      }
    }

    return {
      vehicle_id: telemetry.vehicle_id,
      timestamp: telemetry.timestamp,
      tire_pressures: tire_pressures
    };
  },

  /**
   * Get current anomalies (warnings and critical issues across all systems)
   */
  async get_anomalies() {
    const snapshot = await collection.findOne(
      {},
      {
        sort: { timestamp: -1 },
        projection: { _id: 0 }
      }
    );

    if (!snapshot) {
      return { error: 'No telemetry data found' };
    }

    const telemetry = getTelemetryData(snapshot);
    if (!telemetry || !telemetry.telemetry_batch) {
      return { error: 'Failed to parse telemetry data' };
    }

    const anomalies = [];
    const systems = telemetry.telemetry_batch;

    // Check each system for abnormal sensor readings
    for (const [systemName, systemData] of Object.entries(systems)) {
      const systemIssues = [];

      // Special handling for tires - check pressure thresholds directly
      if (systemName === 'tires') {
        for (const [tireName, tireData] of Object.entries(systemData)) {
          if (typeof tireData === 'object' && tireData.pressure) {
            if (tireData.pressure < 28) {
              systemIssues.push({
                sensor: `${tireName}_pressure`,
                value: tireData.pressure,
                temperature: tireData.temp,
                unit: 'psi',
                status: 'critical'
              });
            } else if (tireData.pressure < 30 || tireData.pressure > 35) {
              systemIssues.push({
                sensor: `${tireName}_pressure`,
                value: tireData.pressure,
                temperature: tireData.temp,
                unit: 'psi',
                status: 'warning'
              });
            }
          }
        }
      } else {
        // For other systems, check sensor status fields
        for (const [sensorName, sensorData] of Object.entries(systemData)) {
          if (typeof sensorData === 'object' && sensorData.status) {
            if (sensorData.status === 'warning' || sensorData.status === 'critical') {
              systemIssues.push({
                sensor: sensorName,
                value: sensorData.value,
                unit: sensorData.unit,
                status: sensorData.status,
                pid: sensorData.pid
              });
            }
          }
        }
      }

      if (systemIssues.length > 0) {
        anomalies.push({
          system: systemName,
          issue_count: systemIssues.length,
          issues: systemIssues
        });
      }
    }

    return {
      vehicle_id: telemetry.vehicle_id,
      timestamp: telemetry.timestamp,
      total_anomalies: anomalies.reduce((sum, sys) => sum + sys.issue_count, 0),
      systems_affected: anomalies.length,
      anomalies: anomalies
    };
  },

  /**
   * Query telemetry history for a specific time range
   */
  async query_telemetry_history({ minutes = 10, system = null }) {
    const startTime = Date.now() - (minutes * 60 * 1000);
    
    const query = { timestamp: { $gte: startTime } };
    const projection = {
      _id: 0,
      vehicle_id: 1,
      timestamp: 1,
      telemetry_json: 1,
      anomaly_count: 1
    };

    const snapshots = await collection.find(query, { projection })
      .sort({ timestamp: -1 })
      .limit(100)
      .toArray();

    // Parse and filter telemetry data
    const parsedSnapshots = snapshots.map(snap => {
      const telemetry = getTelemetryData(snap);
      
      if (!telemetry) {
        return {
          timestamp: snap.timestamp,
          vehicle_id: snap.vehicle_id,
          error: 'Failed to parse'
        };
      }

      const result = {
        timestamp: telemetry.timestamp,
        vehicle_id: telemetry.vehicle_id,
        driving_mode: telemetry.driving_mode,
        anomaly_count: telemetry.anomaly_count
      };

      // Include specific system or all systems
      if (system && telemetry.telemetry_batch) {
        result.system_data = telemetry.telemetry_batch[system];
      } else if (telemetry.telemetry_batch) {
        result.systems = telemetry.telemetry_batch;
      }

      return result;
    });

    return {
      query: {
        time_range_minutes: minutes,
        system: system || 'all',
        start_timestamp: startTime
      },
      count: parsedSnapshots.length,
      snapshots: parsedSnapshots
    };
  },

  /**
   * Get statistics for database
   */
  async get_telemetry_stats() {
    const totalSnapshots = await collection.countDocuments();
    const latestSnapshot = await collection.findOne({}, { sort: { timestamp: -1 } });
    const oldestSnapshot = await collection.findOne({}, { sort: { timestamp: 1 } });

    // Count anomalies in last 100 snapshots
    const recentSnapshots = await collection.find({}, { 
      projection: { anomaly_count: 1 },
      sort: { timestamp: -1 },
      limit: 100
    }).toArray();

    const totalAnomalies = recentSnapshots.reduce((sum, s) => sum + (s.anomaly_count || 0), 0);

    return {
      total_snapshots: totalSnapshots,
      recent_anomalies: totalAnomalies,
      latest_timestamp: latestSnapshot?.timestamp,
      oldest_timestamp: oldestSnapshot?.timestamp,
      time_range_ms: latestSnapshot && oldestSnapshot 
        ? latestSnapshot.timestamp - oldestSnapshot.timestamp 
        : 0
    };
  }
};

/**
 * Initialize and run the MCP server
 */
async function main() {
  console.error('🚗 Telemetry MCP Server starting...');
  
  // Connect to MongoDB
  const connected = await connectToMongoDB();
  if (!connected) {
    console.error('❌ Failed to connect to MongoDB. Exiting.');
    process.exit(1);
  }

  // Create MCP server
  const server = new Server(
    {
      name: 'telemetry-mcp-server',
      version: '1.0.0',
    },
    {
      capabilities: {
        tools: {},
      },
    }
  );

  // Register tool list handler
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    return {
      tools: [
        {
          name: 'get_latest_telemetry',
          description: 'Get the most recent telemetry snapshot with all vehicle systems data',
          inputSchema: {
            type: 'object',
            properties: {},
          },
        },
        {
          name: 'check_system_status',
          description: 'Check the status of a specific vehicle system (engine, battery, fuel, tires, transmission, brakes)',
          inputSchema: {
            type: 'object',
            properties: {
              system_name: {
                type: 'string',
                description: 'Name of the system to check',
                enum: ['engine', 'battery', 'fuel', 'tires', 'transmission', 'brakes'],
              },
            },
            required: ['system_name'],
          },
        },
        {
          name: 'get_tire_pressure',
          description: 'Get tire pressure readings for all four tires',
          inputSchema: {
            type: 'object',
            properties: {},
          },
        },
        {
          name: 'get_anomalies',
          description: 'Get all current warnings and critical issues across all vehicle systems',
          inputSchema: {
            type: 'object',
            properties: {},
          },
        },
        {
          name: 'query_telemetry_history',
          description: 'Query historical telemetry data for a specific time range',
          inputSchema: {
            type: 'object',
            properties: {
              minutes: {
                type: 'number',
                description: 'Number of minutes of history to retrieve (default: 10)',
                default: 10,
              },
              system: {
                type: 'string',
                description: 'Optional: filter by specific system name',
                enum: ['engine', 'battery', 'fuel', 'tires', 'transmission', 'brakes'],
              },
            },
          },
        },
        {
          name: 'get_telemetry_stats',
          description: 'Get statistics about the telemetry database (count, anomalies, time range)',
          inputSchema: {
            type: 'object',
            properties: {},
          },
        },
      ],
    };
  });

  // Register tool call handler
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;

    console.error(`🔧 Tool call: ${name}`, args);

    try {
      if (!tools[name]) {
        throw new Error(`Unknown tool: ${name}`);
      }

      const result = await tools[name](args || {});
      
      console.error(`✅ Tool result for ${name}:`, JSON.stringify(result).substring(0, 200));

      return {
        content: [
          {
            type: 'text',
            text: JSON.stringify(result, null, 2),
          },
        ],
      };
    } catch (error) {
      console.error(`❌ Tool error for ${name}:`, error);
      return {
        content: [
          {
            type: 'text',
            text: JSON.stringify({ error: error.message }),
          },
        ],
        isError: true,
      };
    }
  });

  // Start server with stdio transport
  const transport = new StdioServerTransport();
  await server.connect(transport);

  console.error('✅ Telemetry MCP Server ready');
  console.error(`   Available tools: ${Object.keys(tools).length}`);
}

// Handle cleanup
process.on('SIGINT', async () => {
  console.error('\n🛑 Shutting down Telemetry MCP Server...');
  if (mongoClient) {
    await mongoClient.close();
  }
  process.exit(0);
});

// Start the server
main().catch((error) => {
  console.error('❌ Fatal error:', error);
  process.exit(1);
});
