# ObjectBox Sync Server with MongoDB Atlas

## Setup Instructions

### 1. Configure MongoDB Atlas Credentials

Edit the `.env` file with your MongoDB Atlas credentials:

```bash
MONGODB_USER=your_actual_username
MONGODB_PASS=your_actual_password
MONGODB_CLUSTER=your_cluster
MONGODB_DATABASE=your_database_name
```

**To get these values:**
1. Login to [MongoDB Atlas](https://cloud.mongodb.com)
2. Go to your cluster → "Connect" → "Connect your application"
4. Extract:
   - `MONGODB_USER` = username part
   - `MONGODB_PASS` = password part
   - `MONGODB_CLUSTER` = full hostname
   - `MONGODB_DATABASE` = database_name 

### 2. How Docker Compose Uses .env Variables

Docker Compose **automatically loads** `.env` file from the same directory:

```yaml
# In docker-compose.yml, this:
--mongo-url mongodb+srv://${MONGODB_USER}:${MONGODB_PASS}@${MONGODB_CLUSTER}.mongodb.net/...
```

**No extra configuration needed!** Docker Compose reads `.env` automatically.

### 3. Start the Services

```powershell
# Navigate to sync-server-setup directory
cd sync-server-setup

# Start both sync server and search service
docker compose up

# Or run in background
docker compose up -d
```

This will start:
- **Sync Server** (port 9980 admin, 9999 sync)
- **Search Service** (port 8080 HTTP API with HNSW vector search)

### 4. Access Services

**Admin UI:** http://localhost:9980/
- Activate your trial license when prompted

**Search Service Health:** http://localhost:8080/health
- Check if search service is running and synced

### 5. Verify Setup

Check the logs:
```powershell
docker compose logs -f sync-server
```

You should see:
```
✓ Connected to MongoDB at mongodb+srv://...
✓ Database: car_assistant_demo
✓ Sync server ready on port 9999
```


## Troubleshooting

### "Cannot connect to MongoDB"

1. Check `.env` file has correct credentials
2. Verify MongoDB Atlas IP whitelist includes your IP
3. Check cluster is running in Atlas
4. Verify connection string format

### "Environment variable not found"

Make sure `.env` file is in the same directory as `docker-compose.yml`:
```
sync-server-setup/
├── .env                    ← Must be here
└── docker-compose.yml      ← Same level
```

## Files Structure

```
sync-server-setup/
├── .env                     # Your credentials (DO NOT COMMIT)
├── .env.example            # Template (safe to commit)
├── docker-compose.yml      # Services configuration
├── objectbox-model.json    # Database schema
├── search-service-data/    # Persistent ObjectBox database (auto-created)
└── README.md              # This file
```

## Persistent Storage

The search service database is stored in `search-service-data/` directory:
- ✅ **Persists across container restarts**
- ✅ **Survives container removal**
- ✅ **Accessible from host system**
- ✅ **Automatically synced with MongoDB Atlas**

**Location on Host:**
```
sync-server-setup/search-service-data/
```

**Location in Container:**
```
/app/search-service-db
```

**To reset the database:**
```powershell
# Stop services
docker compose down

# Remove database
Remove-Item -Recurse -Force search-service-data

# Restart (will re-sync from MongoDB)
docker compose up -d
```

## Stopping the Services

```powershell
# Stop and remove containers
docker compose down

# Stop and remove everything including volumes
docker compose down -v
```

## Connecting Clients

Once the sync server is running, configure your client applications:

```python
# In your Python app
from objectbox import Store

store = Store(
    directory="car-manual-db",
    sync_server_url="ws://localhost:9999"
)
```

The client will automatically sync with MongoDB Atlas through the sync server.
