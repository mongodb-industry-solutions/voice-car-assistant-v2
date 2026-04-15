# Telemetry MCP Server - Part 5

MongoDB MCP (Model Context Protocol) Server for querying automotive telemetry data using natural language.

## Architecture

```
GitHub Copilot / LLM
        ↓
Next.js API Routes (/api/telemetry-mcp/*)
        ↓
TelemetryMCPManager (lib/telemetry-mcp.js)
        ↓
MCP Server Process (JSON-RPC over stdio)
        ↓
MongoDB Atlas (automotive_telemetry database)
```

## Available Tools

### 1. `get_latest_telemetry`
Get the most recent telemetry snapshot with all vehicle systems data.

**Parameters:** None

**Example Response:**
```json
{
  "vehicle_id": "DEMO-001",
  "timestamp": 1776164537650,
  "driving_mode": "normal",
  "anomaly_count": 2,
  "systems": {
    "engine": { "status": "normal", "sensors": {...} },
    "battery": { "status": "normal", "sensors": {...} },
    "fuel": { "status": "normal", "sensors": {...} },
    "tires": { "status": "warning", "sensors": {...} },
    "transmission": { "status": "normal", "sensors": {...} },
    "brakes": { "status": "normal", "sensors": {...} }
  }
}
```

### 2. `check_system_status`
Check the status of a specific vehicle system.

**Parameters:**
- `system_name` (required): One of `engine`, `battery`, `fuel`, `tires`, `transmission`, `brakes`

**Example:**
```json
{
  "system_name": "tires"
}
```

### 3. `get_tire_pressure`
Get tire pressure readings for all four tires.

**Parameters:** None

**Example Response:**
```json
{
  "vehicle_id": "DEMO-001",
  "timestamp": 1776164537650,
  "tire_pressures": {
    "front_left": { "pressure": 32.0, "unit": "psi", "status": "normal" },
    "front_right": { "pressure": 32.0, "unit": "psi", "status": "normal" },
    "rear_left": { "pressure": 31.8, "unit": "psi", "status": "normal" },
    "rear_right": { "pressure": 28.5, "unit": "psi", "status": "warning" }
  }
}
```

### 4. `get_anomalies`
Get all current warnings and critical issues across all vehicle systems.

**Parameters:** None

### 5. `query_telemetry_history`
Query historical telemetry data for a specific time range.

**Parameters:**
- `minutes` (optional, default: 10): Number of minutes of history to retrieve
- `system` (optional): Filter by specific system name

### 6. `get_telemetry_stats`
Get statistics about the telemetry database.

**Parameters:** None

## API Endpoints

### GET `/api/telemetry-mcp/health`
Check MCP server health status.

### GET `/api/telemetry-mcp/tools`
List all available tools.

### POST `/api/telemetry-mcp/call`
Call a specific tool.

**Request Body:**
```json
{
  "tool": "get_tire_pressure",
  "parameters": {}
}
```

### GET `/api/telemetry-mcp/history`
Get tool call history for debugging.

## Setup

### 1. Install Dependencies
```bash
cd telemetry-mcp-server
npm install
```

### 2. Configure Environment
Add to `.env.local`:
```env
NEXT_PUBLIC_MONGODB_URI=mongodb+srv://user:pass@cluster.mongodb.net/
```

### 3. Start MCP Server (Automatic)
The MCP server starts automatically when Next.js initializes the TelemetryMCPManager.

### 4. Test with API
```bash
# Health check
curl http://localhost:3001/api/telemetry-mcp/health

# List tools
curl http://localhost:3001/api/telemetry-mcp/tools

# Call a tool
curl -X POST http://localhost:3001/api/telemetry-mcp/call \
  -H "Content-Type: application/json" \
  -d '{"tool":"get_tire_pressure","parameters":{}}'
```

## Usage with GitHub Copilot

Once the MCP server is running, you can ask GitHub Copilot natural language questions:

- "What's my current tire pressure?"
- "Are there any warnings in my vehicle?"
- "Show me the engine status"
- "What anomalies have occurred in the last 10 minutes?"

Copilot will use the MCP tools to query MongoDB Atlas and provide real-time answers!

## Implementation Notes

- **Read-only**: All tools are read-only queries - no data modification
- **JSON-RPC 2.0**: Standard protocol for MCP communication  
- **Singleton pattern**: One MCP server instance per Next.js process
- **Auto-reconnect**: MongoDB client handles reconnection automatically
- **Timeouts**: 15-second timeout on all tool calls
- **Logging**: Full console logging for debugging

## Troubleshooting

**MCP Server not starting:**
- Check `NEXT_PUBLIC_MONGODB_URI` is set in `.env.local`
- Verify MongoDB Atlas network access allows your IP
- Check Next.js terminal for error logs

**Tool calls timing out:**
- Ensure simulation is running (data exists in MongoDB)
- Check MongoDB Atlas cluster performance
- Verify network connectivity

**No data returned:**
- Start the telemetry simulator: `POST http://localhost:8082/simulator/start`
- Ensure ObjectBox Sync has synced data to MongoDB Atlas
- Check collection name is `telemetry_snapshots`

## Best Practices (from MongoDB Industry Solutions)

✅ **Singleton pattern** for MCP server instance  
✅ **Separate API routes** for each concern (health, tools, call, history)  
✅ **Dynamic routes** (`export const dynamic = 'force-dynamic'`)  
✅ **Tool call tracking** for analytics and debugging  
✅ **Console logging system** for observability  
✅ **Graceful degradation** when MCP unavailable  
✅ **Build-time detection** to skip during SSG  
✅ **JSON-RPC 2.0 protocol** over stdin/stdout  
✅ **Request/response tracking** with message IDs  

## Files Structure

```
voice-car-assistant-v2/
  telemetry-mcp-server/
    package.json          ← MCP server dependencies
    server.js             ← Main MCP server with 6 tools
    README.md             ← This file

genai-in-car-voice-assistant/
  src/
    lib/
      telemetry-mcp.js    ← MCP server manager (singleton)
    app/
      api/
        telemetry-mcp/
          health/route.js  ← Health check endpoint
          tools/route.js   ← List tools endpoint
          call/route.js    ← Execute tool endpoint
          history/route.js ← Tool call history endpoint
```

## Next Steps (Part 6 - Polish)

- [ ] Add UI panel for MCP tool testing
- [ ] Create React hook for MCP tool calls
- [ ] Add real-time MCP call visualization  
- [ ] Implement caching for frequently accessed data
- [ ] Add more advanced query tools (aggregations, trends)
- [ ] Create custom Copilot chat interface
