# 🚗 Car Manual Voice Assistant - Car Dashboard UI

**Authentic car dashboard interface** with touchscreen chat for your vehicle manual assistant.

Features a **realistic car interior dashboard** with center touchscreen displaying **conversation-style chat** powered by MongoDB and ObjectBox.

## 🎯 What This Does

- **Car dashboard interface** - Realistic gauges, indicators, and center touchscreen
- **Chat-style conversation** - Natural back-and-forth like messaging apps
- **Voice interaction** - Speak naturally, ask questions about your car
- **Real-time updates** - See conversation flow with timestamps
- **Source citations** - View manual chunks used for each answer
- **Natural exit** - Just say "thank you" to end the conversation

## 🏗️ Architecture

```
Backend Server (Flask) → Serves Web UI
         ↓ (User clicks button)
Voice Assistant Initialize → Python (STT/LLM/TTS)
         ↓ (WebSocket events)
Browser UI → Real-time updates
```

**Key Design:**
- Backend starts = Web server only (NO voice initialization)
- Button click = Voice assistant lazy-loaded and started
- Complete separation of concerns

## 📋 Prerequisites

1. **Search service running**:
   ```powershell
   cd sync-server-setup
   docker compose up -d
   # This starts both sync-server and search-service
   ```

2. **Ollama with models**:
   ```bash
   ollama pull nub235/voyage-4-nano
   ollama pull llama3.2
   ```

3. **Microphone access** (for voice input)

## 🚀 Quick Start

### 1. Install Dependencies

```powershell
cd voice-assistant-backend
pip install -r requirements.txt
```

### 2. Start the Demo

```powershell
python start_demo.py
```

This will:
- ✅ Start the Flask web server on port 5000
- ✅ Automatically open your browser to http://localhost:5000
- ✅ Display the car touchscreen UI
- ⏸️ **Voice assistant will NOT start until you click the button**

### 3. Use the Demo

1. **Click the microphone button** to start listening
2. **Speak your question** (e.g., "How do I change the oil?")
3. **Watch the UI update** in real-time
4. **Hear the answer** through your speakers
5. **Say "thank you"** to end the conversation naturally
6. Or click the microphone button again to stop manually

## 🎨 UI Features

### Car Dashboard Design
- **Authentic gauges** - Speedometer, fuel gauge on side panels
- **Dashboard indicators** - Battery, temperature displays
- **Center touchscreen** - Modern car infotainment style with bezel
- **Dark theme** - Realistic car interior colors
- **MongoDB branding** - Green accents throughout

### Chat Interface
- **Conversation bubbles** - User messages (right) vs Assistant (left)
- **Avatars** - 👤 (you) and 🤖 (assistant)
- **Timestamps** - Show when each message was sent
- **Source chips** - Manual chunks displayed with match scores
- **Auto-scroll** - Keeps latest messages visible
- **Status indicator** - Shows what the assistant is doing

## 🔧 Configuration

Edit `config.py` to customize search service URL, models, etc.

## 🛠️ Troubleshooting

**"Cannot connect to search service"**

Make sure both services are running:
```powershell
cd sync-server-setup
docker compose up -d

# Verify services
docker compose ps
```

Check search service health:
```powershell
Invoke-RestMethod -Uri "http://localhost:8080/health" -Method Get
```

**Microphone not working**

1. Check Windows microphone permissions
2. Install soundfile: `pip install soundfile`

**Database persists across restarts**

The search service database is now stored in:
```
sync-server-setup/search-service-data/
```

To reset and re-sync:
```powershell
docker compose down
Remove-Item -Recurse -Force search-service-data
docker compose up -d
```

## 📊 Performance

- **Total response time**: ~3-11 seconds (voice to voice)

## 🎓 Example Questions

- "How do I change the oil?"
- "What is the recommended tire pressure?"
- "How do I reset the maintenance light?"

## 📁 Project Structure

```
voice-assistant-backend/
├── start_demo.py          # Easy launcher (run this!)
├── web_server.py          # Flask + SocketIO server
├── main.py                # Voice assistant
├── templates/index.html   # Car touchscreen UI
└── static/                # CSS + JavaScript
```

---

**Ready to demo?** Run `python start_demo.py` and start talking! 🎤🚗
# 🚗 Car Manual Voice Assistant

**Offline-first voice assistant** that uses a C++ search service with ObjectBox Sync for instant access to car manual information, with automatic cloud synchronization when online.

## 🌟 Features

- **🎙️ Voice Interaction**: Ask questions using your voice, get spoken answers
- **💬 Text Mode**: Also works in text-only mode (no microphone needed)
- **📴 Offline-First**: Works completely offline using local ObjectBox database
- **☁️ Auto-Sync**: Automatically syncs with MongoDB Atlas when online (via sync server)
- **🔍 Vector Search**: Fast semantic search using 1024-dim HNSW embeddings
- **🤖 AI-Powered**: Uses Ollama for embeddings and answer generation
- **⚡ High Performance**: C++ search service with HNSW index for millisecond searches

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  Voice Assistant (Python)                    │
│  • Whisper STT → Ollama Embeddings (1024-dim)               │
│  • HTTP POST /search → Search Service                        │
│  • Ollama LLM → pyttsx3 TTS                                  │
└────────────────────────┬────────────────────────────────────┘
                         │ HTTP REST API
                         │ (localhost:8080/search)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│            C++ Search Service (Docker)                       │
│  • ObjectBox C++ High-Level API                              │
│  • HNSW vector index (1024-dim, COSINE distance)            │
│  • HTTP server for search requests                           │
│  • ObjectBox Sync Client                                     │
└────────────────────────┬────────────────────────────────────┘
                         │ Bidirectional sync
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              ObjectBox Sync Server (Docker)                  │
│  (Handles MongoDB ↔ ObjectBox synchronization)              │
└────────────────────────┬────────────────────────────────────┘
                         │ MongoDB Atlas connection
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              MongoDB Atlas (Cloud)                           │
│  Database: car_assistant_demo                                │
│  Collections: manual_chunks (22,535), manuals (13)          │
└─────────────────────────────────────────────────────────────┘
```

**Key Benefit**: Python voice assistant communicates with C++ search service via simple HTTP API. C++ service handles fast vector search and automatic cloud sync.

## 📋 Prerequisites

### 1. Docker Services Running

The sync server and search service must be running:

```powershell
cd ../sync-server-setup
docker compose up -d

# Start the search service
cd ../search-service
docker run -d --name search-service --network sync-server-setup_default -p 8080:8080 search-service:latest
```

Verify services are healthy:

```powershell
# Check search service
Invoke-RestMethod -Uri "http://localhost:8080/health" -Method Get

# Should show: {"status":"healthy","chunk_count":22535}
```

### 2. Ollama with Models

```bash
ollama pull nub235/voyage-4-nano  # 1024-dim embeddings
ollama pull llama3.2               # LLM for answers
```

### 3. Python 3.8+

## 🚀 Setup

### Step 1: Create Virtual Environment

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
```

### Step 2: Install Dependencies

```powershell
pip install -r requirements.txt
```

**Note on PyAudio**: If it fails on Windows, download a wheel from:
https://www.lfd.uci.edu/~gohlke/pythonlibs/#pyaudio

**Note on soundfile**: Required for Whisper speech recognition. Already in requirements.txt.

### Step 3: Verify Setup

```powershell
python setup_check.py
```

This checks:
- ✅ Search service is running and has data
- ✅ Ollama is running with required models
- ✅ Microphone access (for voice mode)

## 🎯 Usage

### Voice Mode (Default)

Talk to your car manual:

```powershell
python main.py
```

**How it works**:
1. 🎤 Speak your question when prompted (e.g., "How do I change the oil?")
2. 🧮 System generates 1024-dim embedding via Ollama
3. 🔍 HTTP POST to search service with embedding
4. 📄 C++ service uses HNSW to find top 3 similar chunks
5. 🤖 LLM generates answer using retrieved context
6. 🔊 Answer is spoken back to you

### Text-Only Mode

No microphone? Use text mode:

```powershell
python main.py --text-only
```

Type your questions instead of speaking them. Perfect for testing!

### Quick Test

```powershell
python test_setup.py
```

Runs a test query without voice input/output.

## 🔧 Configuration

Edit [config.py](config.py) to customize:

- **SEARCH_SERVICE_URL**: Search service endpoint (default: http://localhost:8080)
- **AI models**: Embedding and LLM models  
- **Whisper settings**: Model size (tiny/small/medium/large), language
- **Search settings**: Number of results to retrieve

## 🗄️ Database Schema

### ManualChunk Entity
Stored in C++ search service ObjectBox database:

| Field | Type | Description |
|-------|------|-------------|
| `id` | Int64 | Primary key |
| `text` | String | Manual text chunk |
| `source_file` | String | Source PDF filename |
| `chunk_index` | Int32 | Chunk position in document |
| `embedding` | Float32Vector[1024] | HNSW vector for similarity search |
| `syncClock` | Int64 | Sync timestamp |

### Manual Entity
Stores car manual metadata:

| Field | Type | Description |
|-------|------|-------------|
| `id` | Int64 | Primary key |
| `filename` | String | Manual filename |
| `make` | String | Car manufacturer |
| `model` | String | Car model |
| `total_chunks` | Int32 | Number of chunks |
| `status` | String | Processing status |
| `syncClock` | Int64 | Sync timestamp |

## 🛠️ Troubleshooting

### "Cannot connect to search service"

The C++ search service is not running.

**Solution**:
```powershell
# Check if search service is running
docker ps | Select-String search-service

# If not running, start it
cd ../search-service
docker run -d --name search-service --network sync-server-setup_default -p 8080:8080 search-service:latest

# Verify it's healthy
Invoke-RestMethod -Uri "http://localhost:8080/health" -Method Get
```

### "No manual chunks found"

Search service has 0 chunks - sync hasn't completed yet.

**Solution**:
1. Check sync server is importing data:
   ```powershell
   cd ../sync-server-setup
   docker compose logs sync-server -f
   ```
2. Trigger MongoDB import if needed:
   ```powershell
   $body = '{"force": true, "inForeground": true}'
   Invoke-RestMethod -Uri "http://localhost:8090/admin/mongodb/import" -Method Post -Body $body -ContentType "application/json"
   ```
3. Wait for sync to complete (22,535 chunks from MongoDB → Sync Server → Search Service)

### Voice recognition not working

**Solutions**:
1. Use text-only mode: `python main.py --text-only`
2. Check microphone permissions in Windows Settings
3. Install `soundfile`: `pip install soundfile`
4. Try different Whisper model size in config.py

### TTS (voice output) not working after first answer

This is handled by creating a fresh pyttsx3 engine for each answer. If it still fails:

**Solution**:
- Use text-only mode: `python main.py --text-only`
- Or restart the Python process

### Embedding/LLM errors

**Solution**:
```bash
# Ensure Ollama is running
ollama list  # Check installed models

# Pull required models
ollama pull nub235/voyage-4-nano  # 1024-dim embeddings
ollama pull llama3.2               # LLM
```

## 📊 Performance

- **HTTP request to search service**: ~5-20ms
- **Vector search (HNSW)**: ~10-50ms for 22,535 chunks  
- **Total search time**: ~15-70ms
- **Embedding generation**: ~100-300ms (Ollama voyage-4-nano)
- **Answer generation**: ~1-5s (Ollama llama3.2, depends on context length)
- **Speech recognition**: ~1-3s (Whisper small model)
- **Text-to-speech**: ~1-2s

**Total response time**: 
- **Voice mode**: ~3-11 seconds (including STT and TTS)
- **Text-only mode**: ~1-7 seconds

## 🔐 Sync & Data Privacy

- **Local-first**: Search service runs locally in Docker (no external API calls)
- **Automatic sync**: Sync server syncs with MongoDB Atlas in background
- **Offline capability**: Once synced, works completely offline
- **No authentication**: Sync server runs in unsecured mode for demo purposes

## 📚 Related Components

- **Search Service**: `../search-service/` - C++ HTTP service with HNSW vector search
- **Sync Server**: `../sync-server-setup/` - Handles MongoDB ↔ ObjectBox synchronization  
- **MongoDB Loader**: `../mongodb-loader/` - Loads and processes car manuals into MongoDB

## 🎓 Example Questions

Try asking:
- "How do I change the oil?"
- "What is the recommended tire pressure?"
- "How do I reset the maintenance light?"
- "What kind of fuel should I use?"
- "How do I pair my phone via Bluetooth?"
- "How often should I replace the air filter?"
- "What does the check engine light mean?"

## 📝 Technical Notes

- **Architecture**: Python voice assistant → HTTP REST API → C++ search service
- **Communication**: JSON over HTTP (POST /search with embedding array)
- **Dimension match**: 1024-dim embeddings (voyage-4-nano) matching C++ HNSW index
- **Search algorithm**: HNSW (Hierarchical Navigable Small World) with Cosine distance
- **Critical fix**: HNSW configured via C API calls (`obx_model_property_index_hnsw_dimensions` + `obx_model_property_index_hnsw_distance_type`)
- **TTS fix**: Fresh pyttsx3 engine per speech to avoid Windows stuck-engine bug
