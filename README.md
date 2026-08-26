# Voice Car Assistant v2

A multi-service, fully Dockerised **offline-first** in-vehicle assistant that combines live VSS telemetry, car-manual vector search, voice I/O, and a LangChain ReAct agent. A Next.js cockpit UI talks to a REST/SSE Python backend; edge data lives in ObjectBox and replicates to MongoDB Atlas via ObjectBox Sync, with a toggle that pauses/resumes that replication to demonstrate offline buffering and catch-up.

## Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│  Browser  →  http://localhost:5000                                       │
│  voice-assistant-frontend  (Next.js cockpit "Leafy Assistant")           │
│  RPM/speed gauges · chat (SSE) · DTC ticker · map · live sync panel      │
│  Browser talks ONLY to same-origin /api/* → proxied server-side          │
└───────────────────────────────┬──────────────────────────────────────────┘
                                 │ REST + SSE  (BACKEND_URL)
┌────────────────────────────────▼─────────────────────────────────────────┐
│  voice-assistant-backend  :8000  (Flask — REST/SSE, no UI)                 │
│  Agent relay · Whisper STT · Piper TTS · telemetry/sim/nav proxies         │
│  Online/offline toggle = enable/disable the sync proxy (toxiproxy ctl API) │
└──┬──────────────┬───────────────────┬────────────────────┬────────────────┘
   │ chat (SSE)    │ conversations     │ navigation         │ sync pause/resume
   │               ▼                   ▼                    ▼
   │      conversation-service   navigation-service   toxiproxy :8474 (control)
   │      :8081  ObjectBox       :5001 Ollama + OSRM  :9998 (sync proxy) ──┐
   │                                                                        │ ws
┌──▼───────────────────────────────────────────────┐                       │
│  LangChain ReAct Agent  :5002  (Ollama qwen2.5:3b)│                       │
│  offline graph → local tools                      │                       │
│  online  graph → Atlas / telemetry-api tools      │                       │
└──┬──────────────────────────┬─────────────────────┘                       │
   │ car-manual search         │ live telemetry                             │
   │  · offline → search-service :8080 (ObjectBox HNSW)                      │
   │  · online  → mongodb-search-service :8085 (Atlas Vector Search)         │
   │  · offline telemetry → vss-telemetry-service :8086 /vss/latest (local)  │
   │  · online  telemetry → vss-telemetry-api :3002 (Atlas telemetry-status) │
   │                                                                         │
   ·  Telemetry ingest + sync path:                                         │
      vss-telemetry-simulator :8087                                          │
        → POST /vss/snapshot every 2 s                                       │
          → vss-telemetry-service :8086 (C++ ObjectBox)                      │
              ← UI & offline agent read /vss/latest (local store)            │
            → ObjectBox Sync client ──ws://toxiproxy:9998──────────────────┘
              → ObjectBox Sync Server :9999 (admin :9980)
                → MongoDB Atlas: objectbox_telemetry (nested)
                  → Atlas Trigger assembleTelemetry
                      ├── telemetry-data   (time-series)
                      └── telemetry-status (current) ← vss-telemetry-api (online)
```

## Services

All services are defined in [`sync-server-setup/docker-compose.yml`](sync-server-setup/docker-compose.yml).

| Service | Port | Description |
|---|---|---|
| `voice-assistant-frontend` | 5000 | Next.js cockpit UI ("Leafy Assistant"); browser calls same-origin `/api/*` only |
| `voice-assistant-backend` | 8000 | Flask REST/SSE API (no UI); orchestration; Whisper STT; Piper TTS; sync toggle |
| `langchain-agent-service` | 5002 | LangChain ReAct agent (Ollama qwen2.5:3b) |
| `navigation-service` | 5001 | LLM-powered navigation + OSRM routing |
| `ollama` | 11434 | Ollama server — serves qwen2.5:3b; GPU-ready |
| `search-service` | 8080 | ObjectBox HNSW vector search on car-manual chunks |
| `mongodb-search-service` | 8085 | MongoDB Atlas vector search (voyage-4-nano) |
| `conversation-service` | 8081 | ObjectBox conversation history |
| `sync-server` | 9980 / 9999 | ObjectBox Sync Server — replicates to MongoDB Atlas |
| `toxiproxy` | 8474 / 9998 | Toggleable proxy in front of the Sync Server — the online/offline lever |
| `vss-telemetry-service` | 8086 | C++ ObjectBox service — stores snapshots as `objectbox_telemetry` |
| `vss-telemetry-simulator` | 8087 | Python VSS data generator — start/stop from the dashboard |
| `vss-telemetry-api` | 3002 | Node.js REST API — 9 tools, reads unified Atlas collections |

The browser only ever reaches the **frontend** (`:5000`); every backend call is a same-origin
Next.js `/api/*` route proxied server-side to `voice-assistant-backend` (`:8000`). All other
services are internal.

## Agent Service

The `langchain-agent-service` uses LangGraph `create_react_agent` and is optimised for low-latency responses on CPU-only inference.

### Design principles

- **Two pre-compiled agents** (`_OFFLINE_AGENT`, `_ONLINE_AGENT`) compiled once at startup — zero compilation cost per request. The agent is selected by `network_mode` only; `lat`/`lon` are injected via `RunnableConfig` and never affect graph structure.
- **Streaming responses** — `/agent/chat/stream` returns a `text/event-stream` SSE response. Tokens stream as they are generated; a `status` event is emitted immediately when the model commits to a tool call so the UI shows progress during the prefill phase. The blocking `/agent/chat` endpoint is kept for the voice loop (TTS needs the full answer before speaking).
- **Shared conversation memory** — `MemorySaver` keyed by `conversation_id`; history is token-aware trimmed to a ~600-token budget before each LLM call (`trim_messages`, `start_on="human"` so tool-call/response pairs stay intact).
- **Source threading** — when `search_car_manual` returns results, the tool appends a `[SOURCES:<json>]` marker (stripped before the LLM sees it) that the streaming handler extracts and returns as a `sources` array alongside the answer.
- **Tool tracking** — every assistant turn records which tools were called in `tools_used`, persisted to the `conversations` entity and synced to Atlas as a native BSON array (`JsonToNative`).
- **Embedding cache** — query embeddings are LRU-cached (256 entries); repeated queries skip the encode step.
- **Context budget** — system prompt + mode hint (~250 tokens) + tool schemas + capped tool results (600 chars max) + token-trimmed history stay within `num_ctx=1536`, keeping per-token generation fast.
- **Mode-aware** — both agents have car-manual search, navigation, and live telemetry; the only difference is the *source*. Offline reads the manual and telemetry from the on-edge ObjectBox store (works with no connection); online reads the manual from Atlas Vector Search and telemetry from the Atlas `telemetry-status` collection. Either way the agent must call the tools and never guess live values.

### Model config

| Parameter | Value | Reason |
|---|---|---|
| Model | `qwen2.5:3b` | No thinking architecture — all generated tokens are useful output; reliable tool calling |
| `num_ctx` | 1536 | Smaller KV cache → faster per-token generation; context budget fits comfortably |
| `num_predict` | 400 | Covers tool call JSON (~30 tokens) + preamble + 3-sentence answer with headroom |
| `temperature` | 0 | Deterministic tool routing |
| `keep_alive` | -1 (integer) | Model never unloaded; no cold-start penalty between requests |

### Tool routing

Tools are labelled explicitly to prevent routing errors with a small model:

- `search_car_manual` — **PROCEDURES & INSTRUCTIONS**: how-to guides, repair steps, warning light meanings, owner's manual content. Use for "how do I" or "what does X mean" questions.
- Telemetry tools — **LIVE READING**: current sensor values only. Never used for procedures. (`get_diagnostics` reports which fault codes/warning lights are active *now*; "what a light means / how to fix" routes to `search_car_manual`.)

Both agents expose the same tool *names*; only the backing data source differs.

| Agent | Tools | Telemetry / manual source |
|---|---|---|
| Offline | `search_car_manual`, `navigate_to`, `get_vehicle_status`, `get_powertrain_status`, `get_fuel_status`, `get_battery_status`, `get_chassis_status`, `get_diagnostics` | On-edge ObjectBox — `search-service` (:8080) + `vss-telemetry-service` `/vss/latest` (:8086) |
| Online | same tool set | Cloud — `mongodb-search-service` (:8085, Atlas Vector Search) + `vss-telemetry-api` (:3002, Atlas `telemetry-status`) |

Navigation (`navigate_to`) calls external routing/geocoding APIs, so it needs internet in both modes — "offline" here means the Atlas *sync* is paused, not that the device has no network.

### Navigation

The `navigate_to` tool strips turn-by-turn steps from the route data before the LLM sees the tool result. The LLM confirms destination + ETA only. The full route geometry is forwarded to the frontend for map rendering via a `[ROUTE_DATA:...]` marker in the tool return string, which is extracted by the streaming handler and removed from LLM context. The map opens as a full-width overlay covering the entire cockpit main area (RPM gauge + chat + speedometer columns) and closes with the ✕ button.

### Agent endpoints

```
POST /agent/chat           — blocking, returns full JSON response
POST /agent/chat/stream    — SSE stream: status events, tokens, then a final done event
POST /debug/search         — direct search with timing breakdown
GET  /health
```

**SSE stream event format:**
```
data: {"status": "Checking battery…"}     ← emitted when tool call starts (UI feedback)
data: {"status": null}                     ← emitted when tool result arrives (clears UI)
data: {"token": "Your battery is at..."}  ← one per generated token
data: {"token": " 42%..."}
...
data: {"done": true, "answer": "...", "tools_used": ["get_battery_status"], "sources": [], "navigation": null, "conversation_id": "..."}
```

## Prerequisites

### 1. Docker Desktop

Install [Docker Desktop](https://www.docker.com/products/docker-desktop/).

### 2. Ollama (containerised — no local install needed)

Ollama runs as a Docker container (`ollama` service, port 11434). On first `docker compose up` it automatically pulls `qwen2.5:3b`. Model weights are stored in the named volume `ollama-models` and survive restarts.

To switch models or force a re-pull:
```bash
docker volume rm sync-server-setup_ollama-models
# then update LLM_MODEL env var in docker-compose.yml and docker compose up
```

To enable NVIDIA GPU acceleration, uncomment the `deploy.resources` block in `docker-compose.yml`.

The **embedding model** (`voyageai/voyage-4-nano`, 1024-d) is separate from the LLM: it is downloaded automatically from Hugging Face the first time the loader or agent runs (no Ollama pull needed). It ships custom model code and must be loaded with `trust_remote_code=True`, pinned to `transformers==4.57.1` — newer transformers break its bundled remote code.

### 3. MongoDB Atlas

Create a free cluster at [cloud.mongodb.com](https://cloud.mongodb.com) and collect:
- username / password
- cluster hostname (e.g. `cluster0.abc12.mongodb.net`)
- database name

## Setup

### 1. Configure credentials

Copy the example env file and fill in your values:

```bash
cd sync-server-setup
cp .env.example .env
# Edit .env with your MongoDB Atlas credentials
```

`.env` contents:

```
MONGODB_USER=your_username
MONGODB_PASS=your_password
MONGODB_CLUSTER=your_cluster.xxxxx.mongodb.net
MONGODB_DATABASE=your_database_name
```

### 2. Set up Atlas unified collections (one-time)

The telemetry API reads from two unified Atlas collections assembled by an Atlas App Services trigger. Before starting the stack, create the two collections (once) — `telemetry-data` as a **time-series** collection (`timeField: "ts"`, `metaField: "vehicleId"`) and `telemetry-status` with a **unique index** on `vehicleId` — via the Atlas UI or mongosh, then create the `assembleTelemetry` function and a database trigger on `objectbox_telemetry` inserts. Full step-by-step (collection recipe, function body, trigger config) is in [`atlas-app/README.md`](atlas-app/README.md).

### 3. Start all services

```bash
cd sync-server-setup
docker compose up
```

Open the **ObjectBox Sync Server admin UI** at [http://localhost:9980](http://localhost:9980) and activate your trial licence when prompted.

### 4. Load the car manual into the search-service (once)

Run this **after** the stack is up, when `search-service` is healthy (from the repo root):

```bash
pip install -r mongodb-loader/requirements.txt
python mongodb-loader/load_documents.py
```

What happens:

1. `load_documents.py` chunks [`mongodb-loader/documents/mongodb_leafy_car_manual.txt`](mongodb-loader/documents/mongodb_leafy_car_manual.txt)
   on heading boundaries (~379 chunks) and embeds each chunk with
   `voyageai/voyage-4-nano` (1024-d, `trust_remote_code=True`, the model's `document` prompt).
2. It POSTs the chunks to the search-service via `POST /chunks` in batches.
   (There is no clear/delete endpoint — to reload from scratch, wipe the store
   first; see "Resetting the search index" below. Otherwise chunks are appended.)
3. The search-service writes them into its local ObjectBox store as the
   **sync-enabled** `manual_chunks` entity, over its active Sync client.
4. The Sync Server replicates them to MongoDB Atlas.

This is the same producer → C++-service-with-sync → Sync Server → Atlas path the telemetry stack uses, just run once instead of continuously.

> **Why over HTTP, not a direct ObjectBox write?** The Python ObjectBox SDK has no
> Sync support, so writing the store directly only produces local-only rows that
> never reach the Sync Server. The chunks must be written by the sync-connected
> search-service, and `manual_chunks` must be `SYNC_ENABLED` in its model
> (`OBXEntityFlags_SYNC_ENABLED`, matching `sync-server-setup/objectbox-model.json`).

> **The embedding model must match on both sides.** The agent embeds queries with
> the same model and dimension (using the `query` prompt). voyage-4-nano is a
> Matryoshka model loaded at its native 1024-d; it is pinned to
> `transformers==4.57.1` because newer transformers break its bundled remote code.

If you change the search schema (e.g. the entity's sync flag or properties),
delete the on-disk store before reloading so it reinitialises cleanly:

```bash
cd sync-server-setup
docker compose down
Remove-Item -Force .\search-service-data\*.mdb   # PowerShell; or rm on Linux/macOS
docker compose up -d --build search-service
# wait until healthy, then re-run: python mongodb-loader/load_documents.py
```

Once all containers are healthy, open the voice assistant at **[http://localhost:5000](http://localhost:5000)**.

## Using the Assistant

The web UI is a full-screen car cockpit dashboard ("Leafy Assistant"), served by the Next.js
frontend. All telemetry widgets poll `/api/vss/latest` every 3 seconds (proxied to the C++
`vss-telemetry-service`, i.e. the local edge store), so the gauges stay live in both modes.

The **ONLINE / OFFLINE** toggle in the header switches the agent's data source **and** pauses/resumes
the ObjectBox → Atlas replication (see *Online / Offline & Live Sync* below):

**Offline mode** — car-manual search and live telemetry are served from the on-edge ObjectBox
store, and Atlas replication is paused (the edge keeps writing locally and buffers the backlog).
An amber pulsing border highlights the screen.

**Online mode** — the agent reads the manual from Atlas Vector Search and telemetry from the Atlas
`telemetry-status` collection, and replication resumes so the buffered backlog flushes to Atlas.

### Example questions

- "What is my current fuel level?" *(live telemetry)*
- "Is the battery charging? What's the estimated range?" *(live telemetry)*
- "Any fault codes active?" *(live diagnostics — reads DTCList)*
- "What does the check engine light mean?" *(car manual search)*
- "How do I check the brake fluid?" *(car manual search)*
- "Navigate to the nearest service station" *(navigation — shows route map overlay, no turn-by-turn narration)*

## Online / Offline & Live Sync

The offline/online toggle is a **network-level** switch, not a client stop. Every C++ sync client
connects to the Sync Server **through toxiproxy** (`SYNC_SERVER_URL=ws://toxiproxy:9998` →
`sync-server:9999`), and the backend enables/disables that proxy via its control API (`:8474`):

- **Go offline** → backend disables the `sync` proxy. The clients disconnect but keep running, so
  the edge stores keep accepting writes; the changes queue in each client's outgoing sync buffer.
  Nothing reaches Atlas.
- **Go online** → backend re-enables the proxy and pings each C++ service's `POST /sync/reconnect`
  (which calls the ObjectBox client's `triggerReconnect()`), so the clients reconnect immediately
  instead of waiting out their backoff, and the buffered backlog flushes up to Atlas.

Why a proxy instead of stopping the client? ObjectBox (v5.1.0) forbids restarting a sync client
(`start()` after a stop throws `startedOnce`), and closing + recreating one loses the sync cursor,
which triggers a full re-download. Cutting the connection underneath a permanently-running client
avoids both and is a faithful "offline-first buffer, then catch up" demonstration.

The **live sync panel** (header → *Sync & Data*) polls `GET /api/sync/state`, which merges the edge
side (`vss-telemetry-service` `/sync/status`: `local_count`, `buffered` = outgoing-queue depth,
`sync_state`) with the cloud side (`vss-telemetry-api` `/cloud/counts`) and the proxy's enabled
state (`paused` / `connected`). Watch `buffered` climb while offline and drain on resume.

Backend endpoints (all proxied from the frontend under `/api/sync/*`):

```
GET  /api/sync/state    — edge counts + buffered backlog + cloud counts + paused/connected
POST /api/sync/pause    — go offline (disable the sync proxy)
POST /api/sync/resume   — go online (enable the proxy + nudge every client to reconnect)
```

## VSS Telemetry Schema

Each snapshot is stored **verbatim as one row** — no per-domain split.

### Entities
| Entity | ID | Description |
|---|---|---|
| `objectbox_telemetry` | 26 | `id`, `vehicleId` (idx), `ts` (idx), **`data`** (full VSS Vehicle tree JSON), **`meta`** (vehicle metadata JSON), `syncClock`. Both `data` and `meta` use external type `JsonToNative`. Append-only, pruned after 8 h (`RETAIN_HOURS` env, default 8). |

The shared sync model has three live entities — `manual_chunks` (1), `conversations` (3), `objectbox_telemetry` (26). See [`vss-data.md`](vss-data.md) for the full data model.

`data` is the complete VSS `Vehicle` tree (~1300 signals, 45 top-level domains, exact VSS paths such as `Powertrain.CombustionEngine.Speed`). It is generated by `vss-telemetry-simulator` from the committed spec (`vss-telemetry-simulator/vss_model.json`) with a realistic drive-cycle physics core plus correlated OBD-II fault episodes (~20% of ticks). Active fault codes are drawn from the catalog in `vss-telemetry-simulator/dtc_catalog.json` (mirrored to `voice-assistant-backend/static/dtc_catalog.json` for the dashboard).

The catalog contains **standard OBD-II codes** (C0xxx, P0xxx, U0xxx) plus **custom codes** added for dashboard tell-tale demonstration — these are not standard OBD-II and are marked `[custom]` in the catalog:

| Code | Description | Tell-tale |
|---|---|---|
| `P1217` | Engine Coolant Over Temperature Condition | TEMP (amber/red) |
| `P1001` | Fuel Level Low Warning | FUEL (amber/red) |
| `P1002` | High Voltage Battery State of Charge Low | BATT (amber/red) |
| `C1001` | Tire Pressure Below Minimum Threshold | TPMS (amber/red) |
| `B1001` | Occupant Seat Belt Not Fastened | BELT (red) |
| `P0520` | Engine Oil Pressure Sensor/Switch Circuit | OIL (amber/red) |

Each custom code is backed by a fault episode in the simulator that also pushes the matching sensor signal into its warning range, so the tell-tale lights via both the DTC code and the live sensor value simultaneously.

`data` and `meta` are plain strings in ObjectBox (which stores no nested objects), but the `JsonToNative` external type makes the MongoDB connector expand each into a **native nested document** in the `objectbox_telemetry` Atlas collection — so there is no separate `VehicleMeta` collection; the metadata rides inside each snapshot.

### Conversations entity

| Property | ID | Type | Notes |
|---|---|---|---|
| `id` | 1 | Int64 | |
| `conversation_id` | 2 | String | indexed |
| `user_id` | 3 | String | indexed |
| `timestamp` | 4 | Int64 | indexed |
| `role` | 5 | String | `user` or `assistant` |
| `message` | 6 | String | |
| `sources` | 7 | String | `JsonToNative=112` → native BSON array in Atlas; `[{score, text}]` per manual chunk returned |
| `syncClock` | 8 | Int64 | |
| `tools_used` | 9 | String | `JsonToNative=112` → native BSON array in Atlas; list of tool names called on each assistant turn |

### Unified Atlas collections (produced by Atlas Trigger)

The Atlas App Services trigger (`atlas-app/`) fires on every `objectbox_telemetry` insert, enriches the snapshot's `CurrentLocation` with a GeoJSON Point, and writes two unified collections:

| Collection | Type | Write cadence | Purpose |
|---|---|---|---|
| `telemetry-data` | Native time-series (`timeField: "ts"`, `metaField: "vehicleId"`) | Every ~2 s | Historical telemetry |
| `telemetry-status` | Standard, unique index on `vehicleId` | Every ~10 s (debounced) | Current state — telemetry API reads from here |

> **Important:** `timeField` is `"ts"` (epoch-ms Long converted to BSON Date by the trigger). Do not use `"timestamp"` — it will fail silently.

### Location format

`CurrentLocation.locationGeoJson` is a GeoJSON Point added by the Atlas trigger:
```json
{"type":"Point","coordinates":[longitude, latitude]}
```
Longitude is first per the GeoJSON spec (RFC 7946). MongoDB Atlas geospatial queries work directly on this field.

## Search Service API

The C++ search-service (:8080) owns the sync-enabled `manual_chunks` ObjectBox store and a Sync client. `load_documents.py` writes through it; the agent queries it.

```
POST   /chunks   — ingest chunks (single object, or {"chunks":[ ... ]});
                   each item: {text, source_file, chunk_index, embedding[1024]}.
                   Writes flow through Sync to MongoDB Atlas.
POST   /search   — vector search; body {embedding[1024], limit}; returns chunks + scores
GET    /health   — status + chunk_count
```

## VSS Telemetry Service API

```
POST   /vss/snapshot          — store one snapshot (data + meta) as objectbox_telemetry
GET    /vss/latest            — newest snapshot (parsed) + meta   ← UI & offline agent read this
GET    /vss/history?minutes=N — recent snapshots (parsed)
GET    /vss/count             — local objectbox_telemetry row count
DELETE /vss/prune?older_than_hours=N   — default N = RETAIN_HOURS (8)
GET    /sync/status           — { available, local_count, buffered, sync_state }
POST   /sync/reconnect        — force an immediate reconnect (used by the backend on resume)
GET    /health
```

`search-service` and `conversation-service` also expose `POST /sync/reconnect` for the same
resume nudge. The online/offline toggle itself lives on the backend (`/api/sync/*`), not here —
these services never stop their sync client; the toxiproxy layer cuts the connection instead.

## Telemetry Tools (vss-telemetry-api)

The telemetry API reads exclusively from `telemetry-status` (one `findOne` per tool call) and exposes all tools via `POST /tools/<name>`. The LangChain agent uses the following subset in online mode:

| Tool | Description |
|---|---|
| `get_vehicle_status` | Full snapshot — all domains + meta in one call |
| `get_powertrain_status` | Speed, RPM, coolant temp, gear, throttle |
| `get_fuel_status` | Fuel level %, litres remaining, consumption rate (+ tank capacity from `meta`) |
| `get_battery_status` | SoC%, SoH%, charging (inferred from charging power), range, voltage, current, temp |
| `get_chassis_status` | Tyre pressures (all four), ABS, traction control, brake pedal |
| `get_diagnostics` | Active OBD-II fault codes (DTCs) with descriptions — the "which warning lights are on now" tool (API name `get_diagnostics_status`) |

Additional tools on the server but not wired to the agent: `get_cabin_status`, `get_location`, `get_adas_status`. Each adds ~30 tokens to every LLM call prefill — re-add to `_TELEMETRY_TOOLS` in `agent_service.py` if needed.

## Data Flow

```
vss-telemetry-simulator (8087)
  → POST /vss/snapshot every 2 s (complete VSS Vehicle tree JSON)
    → vss-telemetry-service (8086, ObjectBox C++)   ← UI & offline agent poll /vss/latest
        one objectbox_telemetry row (data = verbatim JSON, JsonToNative)
      → ObjectBox Sync client ──ws──▶ toxiproxy (9998)   ← online/offline lever (backend toggles)
        → ObjectBox Sync Server (9999)
          → MongoDB Atlas: objectbox_telemetry collection (data expanded to nested doc)
            → Atlas Trigger on objectbox_telemetry insert (assembleTelemetry)
                ├── INSERT telemetry-data (time-series, every ~2 s)
                └── UPSERT telemetry-status (every ~10 s)
                      → vss-telemetry-api (3002)   ← LangChain agent (online mode)
```

When the backend disables the toxiproxy `sync` proxy (offline), the snapshot write path above
still runs up to `vss-telemetry-service`; the hop to the Sync Server is cut, so rows accumulate
locally and flush once the proxy is re-enabled.

Snapshots are inserted with `id = 0` (append-only), so each one creates a new `objectbox_telemetry` row that replicates to Atlas; there is no read-modify-write.

## Resetting the VSS Database

If the ObjectBox store fails to open after a schema change (e.g. on first run after removing entities):

```bash
# Stop services
docker compose down

# Delete the on-disk store
Remove-Item -Recurse -Force sync-server-setup/vss-telemetry-data

# Restart — service initialises a fresh store
docker compose up
```

## Troubleshooting

**Sync Server fails with "Invalid JSON"**
Check `sync-server-setup/objectbox-model.json` for trailing commas — JSON does not allow them.

**C++ service: "Can not open store: Turning an existing local entity type into a synced one"**
The on-disk store was created before the entity had sync enabled. Delete the store directory and restart:
```powershell
Remove-Item -Recurse -Force sync-server-setup\search-service-data   # search-service
Remove-Item -Recurse -Force sync-server-setup\vss-telemetry-data     # vss-telemetry-service
Remove-Item -Recurse -Force sync-server-setup\conversation-data      # conversation-service
```
After restart, re-run `python mongodb-loader/load_documents.py` to reload manual chunks into the search-service.

**C++ service: "Permission denied" (code 13) on startup**
The host bind-mount directory is owned by root; the container process runs as `appuser` and cannot write to it. The `conversation-service` Dockerfile includes an `entrypoint.sh` that fixes ownership at startup via `gosu`. If other C++ services show the same error, apply the same entrypoint pattern to their Dockerfiles.

**`telemetry-data` is empty / Atlas trigger silent failures**
The trigger logs every step with the `[assembleTelemetry]` prefix. Check **App Services → Logs** for errors. The most common cause is the `telemetry-data` time-series collection not existing before the first trigger fire (MongoDB auto-creates it as a plain collection, which cannot accept time-series inserts). Drop and recreate it as time-series (`timeField: "ts"`) via mongosh, then the trigger will succeed on the next snapshot. A `testTelemetryWrite` function in `atlas-app/functions/` can be run manually from the App Services UI to verify database connectivity and write paths.

**Agent answers from memory without calling a tool (online mode)**
Two likely causes:
1. `num_predict` too low — if the model exhausts its token budget on implicit preamble before emitting the tool call JSON, LangGraph interprets the truncated output as a final answer. Default is 400; increase if the problem persists.
2. Tool description overlap — if a user asks "how do I check tire pressure?", the model may route to `get_chassis_status` (LIVE READING) instead of `search_car_manual` (PROCEDURES). The `LIVE READING:` / `PROCEDURES & INSTRUCTIONS:` prefixes in tool descriptions prevent this; do not shorten them.

**vss-telemetry-simulator shows as Docker unhealthy**
False alarm — the `python:3.11-slim` base image lacks `wget`. Verify with `curl http://localhost:8087/health`.

**Ollama connection error from containers**
Check that the `ollama` container is healthy: `docker compose ps ollama`. If it shows unhealthy, the model may still be pulling — wait and retry.

**Agent first response after long idle is slow**
`keep_alive=-1` keeps the model loaded indefinitely. If you restarted Ollama, the first request after idle will reload the model (~5–10 s). Subsequent requests are fast.

**Response times on CPU-only machines**
qwen2.5:3b on CPU runs at ~8–15 tokens/s generation and ~50 tokens/s prefill. A single-tool query (one LLM call to select tool + one to generate answer) typically takes 15–30 s end-to-end. The UI shows a tool progress indicator ("Checking battery…") during the prefill phase so users see feedback immediately. For consistently faster responses, enable GPU acceleration (see Ollama section above).

## License

Apache 2.0
