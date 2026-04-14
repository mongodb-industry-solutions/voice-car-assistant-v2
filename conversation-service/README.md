# Conversation Service

C++ ObjectBox-based HTTP service for storing and retrieving conversation history with automatic sync to MongoDB Atlas.

## Overview

The Conversation Service stores all voice assistant interactions (user questions and assistant answers) locally in ObjectBox and automatically syncs them to MongoDB Atlas via the ObjectBox Sync Server.

## Features

- **HTTP REST API** for saving and retrieving conversations
- **ObjectBox local storage** with persistence
- **Automatic sync** to MongoDB Atlas
- **Session tracking** with unique user IDs and conversation IDs
- **Source tracking** for assistant answers (which manual chunks were used)
- **CORS enabled** for web UI integration

## Architecture

```
Voice Assistant (Flask)
    ↓ POST /conversations/message
Conversation Service (C++ :8081)
    ↓ ObjectBox Local DB
Sync Server (ws://sync-server:9999)
    ↓ Sync Protocol
MongoDB Atlas (conversations collection)
```

## Data Model

```cpp
struct Conversation {
    int64_t id;                    // Auto-generated unique ID
    std::string conversation_id;   // UUID grouping messages in same conversation
    std::string user_id;           // Browser session ID
    int64_t timestamp;             // Unix timestamp (milliseconds)
    std::string role;              // "user" or "assistant"
    std::string message;           // The actual message text
    std::string sources;           // JSON array of source chunks (assistant only)
    int64_t syncClock;             // Managed by ObjectBox Sync
};
```

**Indexes:**
- `conversation_id` - Fast retrieval of full conversations
- `user_id` - Fast retrieval of user's conversation history
- `timestamp` - Chronological ordering

## API Endpoints

### POST /conversations/message
Save a message (user question or assistant answer).

**Request:**
```json
{
  "conversation_id": "550e8400-e29b-41d4-a716-446655440000",
  "user_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "role": "user",
  "message": "How do I change the oil?",
  "sources": ""
}
```

**Response:**
```json
{
  "success": true,
  "id": 12345,
  "timestamp": 1744531200000
}
```

### GET /conversations/:conversation_id
Retrieve full conversation by ID.

**Response:**
```json
{
  "conversation_id": "550e8400-e29b-41d4-a716-446655440000",
  "count": 4,
  "messages": [
    {
      "id": 1,
      "role": "user",
      "message": "How do I change the oil?",
      "sources": "",
      "timestamp": 1744531200000
    },
    {
      "id": 2,
      "role": "assistant",
      "message": "To change the oil...",
      "sources": "[{\"source_file\":\"manual_chunk_123.md\",\"chunk_index\":45,\"score\":0.92}]",
      "timestamp": 1744531205000
    }
  ]
}
```

### GET /conversations/user/:user_id
Retrieve all messages for a user.

**Response:**
```json
{
  "user_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "count": 8,
  "messages": [...]
}
```

### GET /health
Health check endpoint.

**Response:**
```json
{
  "status": "healthy",
  "message_count": 142,
  "service": "conversation-service"
}
```

## Session Management

- **User ID**: Generated per browser session using UUID
  - Created when voice assistant initializes
  - Different browser sessions = different user IDs
  
- **Conversation ID**: Generated per demo session using UUID
  - One conversation_id for entire demo session
  - All questions/answers in that session share the same conversation_id

## MongoDB Collection Structure

Collection: `conversations`

Each document contains:
- User questions (role="user")
- Assistant answers (role="assistant")
- Sources used for each answer (JSON array)
- Timestamps for chronological ordering

## Building

```bash
# Build with Docker
docker build -t conversation-service .

# Build locally
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

## Running

### With Docker Compose (Recommended)
```bash
cd sync-server-setup
docker compose up -d conversation-service
```

### Standalone
```bash
./conversation_service [db_path] [sync_url] [enable_sync]

# Example
./conversation_service /app/conversation-db ws://sync-server:9999 true
```

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| DB Path | `/app/conversation-db` | ObjectBox database directory |
| Sync URL | `ws://sync-server:9999` | Sync server WebSocket URL |
| Enable Sync | `true` | Enable automatic sync to MongoDB |
| HTTP Port | `8081` | HTTP API port |

## Persistent Storage

Database files are stored in a Docker volume:
- **Container**: `/app/conversation-db`
- **Host**: `sync-server-setup/conversation-data/`

This ensures conversations survive container restarts.

## Integration with Voice Assistant

The voice assistant (`web_server.py`) automatically:
1. Generates unique `user_id` when session starts
2. Generates `conversation_id` per demo session
3. Saves user questions after each question
4. Saves assistant answers with sources after each response

## Testing

```bash
# Check service health
curl http://localhost:8081/health

# Save a message
curl -X POST http://localhost:8081/conversations/message \
  -H "Content-Type: application/json" \
  -d '{
    "conversation_id": "test-conv-1",
    "user_id": "test-user-1",
    "role": "user",
    "message": "Test question",
    "sources": ""
  }'

# Retrieve conversation
curl http://localhost:8081/conversations/test-conv-1

# Retrieve user's messages
curl http://localhost:8081/conversations/user/test-user-1
```

## Logs

Monitor conversation saves:
```bash
docker logs conversation-service -f
```

Expected output:
```
💬 Conversation Service - ObjectBox + Sync
📂 Database: /app/conversation-db
🔄 Sync URL: ws://sync-server:9999
✅ ObjectBox store initialized
📊 Current conversations: 0
🚀 Starting HTTP server on port 8081...
💾 Saved user message in 550e8400-e29b-41d4-a716-446655440000
💾 Saved assistant message in 550e8400-e29b-41d4-a716-446655440000
```

## Troubleshooting

### Service won't start
- Check if port 8081 is available: `netstat -an | findstr 8081`
- Verify sync server is running: `docker ps | findstr sync-server`

### Messages not syncing to MongoDB
- Check sync server logs: `docker logs sync-server`
- Verify MongoDB connection string in `sync-server-setup/docker-compose.yml`
- Ensure Conversation entity is in `sync-server-setup/objectbox-model.json`

### "Connection refused" errors
- Verify conversation service is running: `docker ps | findstr conversation`
- Check service health: `curl localhost:8081/health`
- Inspect network: `docker network inspect sync-server-setup_default`

## Sync Implementation Details

### Critical Requirements for ObjectBox Sync

When implementing ObjectBox Sync in C++, two critical requirements must be met for data to sync properly:

#### 1. Sync Client Lifetime Management

**The sync client MUST remain alive for the entire program lifetime.** If the sync client goes out of scope and is destroyed, sync will stop working even though the initial connection succeeded.

**❌ WRONG - Client destroyed after connection:**
```cpp
if (enable_sync) {
    auto syncClient = obx::Sync::client(*store, url, credentials);
    syncClient->start();
    // ❌ syncClient goes out of scope here and is destroyed!
}
// HTTP server runs but sync is dead
```

**✅ CORRECT - Client alive for program lifetime:**
```cpp
// Declare as member variable or in main scope
std::shared_ptr<obx::SyncClient> syncClient;

if (enable_sync) {
    syncClient = obx::Sync::client(*store, url, credentials);
    syncClient->start();
    // ✅ Client reference kept alive throughout execution
}
// HTTP server runs with active sync
```

#### 2. Entity Sync Flags

**Entities must be explicitly marked as sync-enabled** in the model creation. Without this flag, ObjectBox will not sync the entity's data.

**Required flag in model creation:**
```cpp
// Create entity
obx_model_entity(model, "conversations", 3, 1111222233334444555);

// ✅ CRITICAL: Enable sync for this entity
obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);

// Define properties...
obx_model_property(model, "id", OBXPropertyType_Long, 1, uid);
obx_model_property_flags(model, OBXPropertyFlags_ID);
// ... rest of properties
```

**Verification in model JSON:**
```json
{
  "id": "3:1111222233334444555",
  "name": "conversations",
  "flags": 2,  // ← This indicates sync is enabled
  "properties": [...]
}
```

#### 3. Database Migration Limitation

**⚠️ IMPORTANT:** You cannot convert an existing local entity to a synced entity. If you add sync flags to an entity after the database has been created, you'll get this error:

```
Can not open store: Turning an existing local entity type into a synced one is not allowed: conversations
```

**Solution:** Delete the database directory and recreate it fresh with sync enabled:
```bash
# Stop service
docker compose stop conversation-service

# Delete database
rm -rf conversation-data/

# Restart with sync-enabled model
docker compose up -d conversation-service
```

### Implementation Reference

This implementation follows the pattern from the [ObjectBox Sync C++ example](https://github.com/objectbox/objectbox-sync-examples/tree/main/tasks/client-cpp):

- **TasklistCmdlineApp.hpp**: Shows sync client as class member
- **main.cpp**: Demonstrates proper sync client lifetime management
- Uses simple `box.put()` - sync happens automatically once configured correctly

### Debugging Sync Issues

To verify sync is working:

1. **Check sync availability:**
   ```cpp
   bool isSyncAvailable = obx::Sync::isAvailable();
   std::cout << "Sync available: " << isSyncAvailable << std::endl;
   ```

2. **Monitor sync server logs:**
   ```bash
   docker logs sync-server -f | grep -E "APPLY_TX|ACK_TX|Client.*connected"
   ```
   
   Look for:
   - `Client connected` - Client connects to server
   - `APPLY_TX` - Client sending data to server ✅
   - `ACK_TX` - Server acknowledging transactions ✅

3. **Check client connection:**
   ```bash
   docker logs sync-server | grep "New client"
   # Should show: New client (#1) accepted with permissions 3
   ```

If you see connection logs but **no APPLY_TX messages**, check:
- Sync client lifetime (not destroyed)
- Entity sync flags (OBXEntityFlags_SYNC_ENABLED)
- Model consistency between client and server

## License

Part of ObjectBox Automotive Demo project.
