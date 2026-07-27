# VSS Telemetry Data Model

All entities are sync-enabled and replicated to MongoDB Atlas via the shared ObjectBox Sync Server.

Authoritative source of truth: [`sync-server-setup/objectbox-model.json`](sync-server-setup/objectbox-model.json).
Every sync client (search-service, conversation-service, vss-telemetry-service) must present this
**identical** model or the Sync Server rejects the connection with `UNSUPPORTED_DATA_MODEL`.

The live model has three entities:

| Entity | ID | Owner service | Purpose |
|---|---|---|---|
| `manual_chunks` | 1 | search-service | Car-manual chunks + 1024-d embedding (HNSW vector search) |
| `conversations` | 3 | conversation-service | Chat history (user + assistant turns) |
| `objectbox_telemetry` | 26 | vss-telemetry-service | Full VSS snapshot per row (`data` + `meta` as JSON) |

---

## manual_chunks (entity 1)

Car-manual text chunks, each with a 1024-d embedding indexed for HNSW cosine vector search.
Written by `mongodb-loader/load_documents.py` through the search-service, synced to Atlas.

| Field | Type | Description |
|---|---|---|
| `id` | Long | ObjectBox internal ID |
| `text` | String | Chunk text |
| `source_file` | String | Origin document filename |
| `chunk_index` | Int | Ordinal position within the source document |
| `embedding` | FloatVector[1024] | voyage-4-nano embedding; HNSW index, cosine distance |
| `syncClock` | Long | Sync bookkeeping field |

---

## conversations (entity 3)

One row per chat message. Written by conversation-service; synced to Atlas.
`sources` and `tools_used` are `JsonToNative`, so the connector expands each JSON string into a
native BSON array in the `conversations` Atlas collection.

| Field | Type | Description |
|---|---|---|
| `id` | Long | ObjectBox internal ID |
| `conversation_id` | String (indexed) | Conversation/thread identifier |
| `user_id` | String (indexed) | User identifier |
| `timestamp` | Long (indexed) | Unix epoch ms |
| `role` | String | `user` or `assistant` |
| `message` | String | Message text |
| `sources` | String → `JsonToNative` | `[{score, text}]` per manual chunk returned |
| `syncClock` | Long | Sync bookkeeping field |
| `tools_used` | String → `JsonToNative` | Tool names called on the assistant turn |

---

## objectbox_telemetry (entity 26)

Each VSS snapshot is stored **verbatim as one append-only row** (`id = 0` on insert), pruned after
24 h. `data` holds the complete VSS `Vehicle` tree JSON (~1300 signals; see
[`values-vss-data.md`](values-vss-data.md) for the signal hierarchy) and `meta` holds the vehicle
metadata JSON. Both are `JsonToNative`, so the connector expands them into nested BSON documents in
the `objectbox_telemetry` Atlas collection.

| Field | Type | Description |
|---|---|---|
| `id` | Long | ObjectBox internal ID (`0` on insert → append-only) |
| `vehicleId` | String (indexed) | Application-level vehicle identifier (`VSS-DEMO-VIN-001`) |
| `ts` | Long (indexed) | Unix epoch ms |
| `data` | String → `JsonToNative` | Full VSS `Vehicle` tree |
| `syncClock` | Long | Sync bookkeeping field |
| `meta` | String → `JsonToNative` | Vehicle metadata (VIN, OEM, model, capacities, …) |

---

## Read paths

- **Local / offline** — vss-telemetry-service (:8086) serves `GET /vss/latest` (newest snapshot,
  parsed) and `GET /vss/history?minutes=N`. The dashboard polls `/vss/latest` every ~3 s.
- **Cloud / online** — `objectbox_telemetry` syncs to Atlas, where the `assembleTelemetry` trigger
  (see [`atlas-app/`](atlas-app/)) fans each insert out into two unified collections:
  - `telemetry-data` — time-series (`timeField: "ts"`, `metaField: "vehicleId"`), appended every ~2 s
  - `telemetry-status` — one document per vehicle, upserted at most every ~10 s

  The `vss-telemetry-api` REST tools (`get_*`) read `telemetry-status`.
