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

### 3. Start the Sync Server

```powershell
# Navigate to sync-server-setup directory
cd sync-server-setup

# Start the server
docker compose up

# Or run in background
docker compose up -d
```

### 4. Access Admin UI

Open: http://localhost:9980/

Activate your trial license when prompted.

### 5. Verify MongoDB Connection

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
├── .env                   # Your credentials (DO NOT COMMIT)
├── .env.example          # Template (safe to commit)
├── docker-compose.yml    # Sync server configuration
├── objectbox-model.json  # Your database schema
└── README.md            # This file
```

## Stopping the Server

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
