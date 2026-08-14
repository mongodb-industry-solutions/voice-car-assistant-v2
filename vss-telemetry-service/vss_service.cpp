/**
 * VSS Telemetry Service — ObjectBox + Sync
 *
 * Stores each incoming snapshot as one `objectbox_telemetry` row. The domain
 * payload (powertrain/battery/chassis/cabin/location/adas) goes into `data` and
 * the vehicle metadata into `meta` — both String properties flagged JsonToNative,
 * so the MongoDB Sync connector expands them into native nested sibling documents
 * in the `objectbox_telemetry` Atlas collection. No per-domain split, no separate
 * VehicleMeta collection (meta rides inside each snapshot).
 *
 * Endpoints:
 *   POST   /vss/snapshot          — store one snapshot (data + meta) as objectbox_telemetry
 *   GET    /vss/latest            — latest snapshot (parsed) + meta, offline-capable
 *   GET    /vss/history?minutes=N — recent snapshots (parsed)
 *   DELETE /vss/prune?older_than_hours=N
 *   GET    /health
 */

#define OBX_CPP_FILE
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <deque>
#include "objectbox.hpp"
#include "objectbox-sync.hpp"
#include "schema_vss.obx.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace httplib;

// ── Config ────────────────────────────────────────────────────────────────────
struct Config {
    std::string db_path        = "/app/vss-db";
    std::string sync_server_url= "ws://sync-server:9999";
    bool        enable_sync    = true;
    int         port           = 8086;
    int         retain_hours   = 8;   // telemetry retention; override with RETAIN_HOURS
};

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ── ObjectBox model ───────────────────────────────────────────────────────────
OBX_model* create_obx_model() {
    OBX_model* m = obx_model();
    if (!m) return nullptr;

#define DEF_ENTITY(name, eid, euid) \
    obx_model_entity(m, name, eid, euid); \
    obx_model_entity_flags(m, OBXEntityFlags_SYNC_ENABLED)

#define PROP_L(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_Long,   pid, puid)
#define PROP_S(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_String, pid, puid)
#define PROP_I(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_Int,    pid, puid)
#define PROP_F(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_Float,  pid, puid)
#define PROP_B(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_Bool,        pid, puid)
#define PROP_FV(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_FloatVector, pid, puid)
#define PROP_ID(nm, pid, puid) PROP_L(nm, pid, puid); obx_model_property_flags(m, OBXPropertyFlags_ID)
#define PROP_IDX(iid, iuid) obx_model_property_flags(m, OBXPropertyFlags_INDEXED); \
                             obx_model_property_index_id(m, iid, iuid)
#define LAST_PROP(pid, puid) obx_model_entity_last_property_id(m, pid, puid)

    // ── Shared entities registered for sync (owned by other services) ─────────

    // Entity 1: manual_chunks
    DEF_ENTITY("manual_chunks", 1, 2807783899453578393ULL);
    PROP_ID("id",          1, 871349036716677797ULL);
    PROP_S("text",         2, 6563616578029045320ULL);
    PROP_S("source_file",  3, 8818095693993590927ULL);
    PROP_I("chunk_index",  4, 6846133054869205678ULL);
    PROP_FV("embedding",   5, 6898708364220688226ULL); PROP_IDX(1, 4357812374228481003ULL);
    PROP_L("syncClock",    6, 1234567890123456789ULL);
    LAST_PROP(6, 1234567890123456789ULL);

    // Entity 3: conversations
    DEF_ENTITY("conversations", 3, 1111222233334444555ULL);
    PROP_ID("id",              1, 1111222233334444556ULL);
    PROP_S("conversation_id",  2, 2222333344445555666ULL); PROP_IDX(2, 2222222222222222222ULL);
    PROP_S("user_id",          3, 3333444455556666777ULL); PROP_IDX(3, 3333333333333333333ULL);
    PROP_L("timestamp",        4, 4444555566667777888ULL); PROP_IDX(4, 4444444444444444444ULL);
    PROP_S("role",             5, 5555666677778888999ULL);
    PROP_S("message",          6, 6666777788889999111ULL);
    PROP_S("sources",          7, 7777888899991111222ULL);
    obx_model_property_external_type(m, OBXExternalPropertyType_JsonToNative);  // JSON array → native array in Atlas
    PROP_L("syncClock",        8, 8888999911112222333ULL);
    PROP_S("tools_used",       9, 9099888877776666555ULL);
    obx_model_property_external_type(m, OBXExternalPropertyType_JsonToNative);  // JSON array → native array in Atlas
    LAST_PROP(9, 9099888877776666555ULL);

    // Entity 26: objectbox_telemetry — whole snapshot stored as JSON in `data`.
    // `data` is JsonToNative so the connector expands it to a nested Atlas document.
    DEF_ENTITY("objectbox_telemetry", 26, 6030000000000000ULL);
    PROP_ID("id",         1, 6030000000000001ULL);
    PROP_S("vehicleId",   2, 6030000000000002ULL); PROP_IDX(25, 6030000000000100ULL);
    PROP_L("ts",          3, 6030000000000003ULL); PROP_IDX(26, 6030000000000200ULL);
    PROP_S("data",        4, 6030000000000004ULL);
    obx_model_property_external_type(m, OBXExternalPropertyType_JsonToNative);
    PROP_L("syncClock",   5, 6030000000000005ULL);
    PROP_S("meta",        6, 6030000000000006ULL);
    obx_model_property_external_type(m, OBXExternalPropertyType_JsonToNative);
    LAST_PROP(6, 6030000000000006ULL);

    obx_model_last_entity_id(m, 26, 6030000000000000ULL);
    obx_model_last_index_id(m,  26, 6030000000000200ULL);

#undef DEF_ENTITY
#undef PROP_L
#undef PROP_S
#undef PROP_I
#undef PROP_F
#undef PROP_B
#undef PROP_FV
#undef PROP_ID
#undef PROP_IDX
#undef LAST_PROP

    return m;
}

// ── Store init ────────────────────────────────────────────────────────────────
std::shared_ptr<obx::Store> init_store(const Config& cfg) {
    OBX_model* m = create_obx_model();
    if (!m) return nullptr;
    try {
        obx::Options opts(m);
        opts.directory(cfg.db_path.c_str());
        opts.maxDbSizeInKb(1572864);  // 1.5 GiB (default is 1 GiB)
        return std::make_shared<obx::Store>(opts);
    } catch (const std::exception& e) {
        std::cerr << "Store init failed: " << e.what() << "\n";
        return nullptr;
    }
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static std::string js(const json& j, const char* k, const std::string& def = "") {
    return j.contains(k) && j[k].is_string() ? j[k].get<std::string>() : def;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::cout << "=============================================================\n";
    std::cout << "🚗 VSS Telemetry Service — ObjectBox + Sync\n";
    std::cout << "=============================================================\n\n";

    Config cfg;
    if (argc > 1) cfg.db_path         = argv[1];
    if (argc > 2) cfg.sync_server_url  = argv[2];
    if (argc > 3) cfg.enable_sync      = (std::string(argv[3]) == "true");
    // Env overrides for the single-pod deploy (unset locally → keep args/defaults).
    if (const char* s = std::getenv("SYNC_SERVER_URL")) cfg.sync_server_url = s;
    if (const char* p = std::getenv("PORT")) {
        try { cfg.port = std::stoi(p); }
        catch (const std::exception&) {
            std::cerr << "Invalid PORT='" << p << "'; using default " << cfg.port << std::endl;
        }
    }
    if (const char* rh = std::getenv("RETAIN_HOURS")) {
        try { cfg.retain_hours = std::stoi(rh); }
        catch (const std::exception&) {
            std::cerr << "Invalid RETAIN_HOURS='" << rh << "'; using default " << cfg.retain_hours << std::endl;
        }
    }

    std::cout << "📂 DB: " << cfg.db_path << "\n";
    std::cout << "🔄 Sync: " << cfg.sync_server_url << "\n\n";

    auto store = init_store(cfg);
    if (!store) { std::cerr << "❌ Store init failed\n"; return 1; }
    std::cout << "✅ ObjectBox store ready\n";

    std::shared_ptr<obx::SyncClient> syncClient;
    // Captured once at startup: the deployment configured sync AND the linked ObjectBox
    // build supports it. `/sync/status` and `/sync/reconnect` gate on this. The client is
    // started exactly once here and then lives for the whole process — offline/online is
    // toggled at the network layer (toxiproxy), never by stopping/restarting this client.
    const bool syncConfigured = cfg.enable_sync && obx::Sync::isAvailable();
    if (syncConfigured) {
        try {
            syncClient = obx::Sync::client(*store, cfg.sync_server_url, obx::SyncCredentials::none());
            syncClient->start();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "✅ Sync client started (state=" << (int)syncClient->state() << ")\n\n";
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Sync start failed: " << e.what() << "\n\n";
        }
    }

    const std::string VEHICLE_ID = "VSS-DEMO-VIN-001";

    // Box
    auto obt_box = store->box<ObxTelemetry>();

    // Background pruning thread — runs every hour
    std::thread prune_thread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::hours(1));
            int64_t cutoff = now_ms() - (int64_t)cfg.retain_hours * 3600 * 1000;
            try {
                obt_box.query(ObxTelemetry_::ts.lessThan(cutoff)).build().remove();
                std::cout << "🗑️  Pruned snapshots older than " << cfg.retain_hours << "h\n";
            } catch (const std::exception& e) {
                std::cerr << "Prune error: " << e.what() << "\n";
            }
        }
    });
    prune_thread.detach();

    Server svr;
    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });
    svr.Options("/(.*)", [](const Request&, Response& res){ res.status = 204; });

    // ── Per-vehicle offline buffering (SYNC_SCOPE=session) ───────────────────────
    // The sync client stays global and always connected. Taking a vehicle "offline" just
    // routes its snapshots to an in-memory buffer instead of the synced store, so nothing
    // for it reaches Atlas; /vss/latest serves the buffer so the edge keeps working; on
    // resume the buffer flushes into the synced store and syncs up (catch-up). This is only
    // exercised when the backend runs in session scope. In global scope (local) no vehicle
    // is ever paused, so this is inert and the write path is byte-for-byte unchanged.
    static std::mutex sessionMutex;
    static std::unordered_map<std::string, bool> sessionPaused;
    static std::unordered_map<std::string, std::deque<ObxTelemetry>> sessionBuffer;
    static const size_t SESSION_BUFFER_MAX = 5000;  // ~2.7 h at 2 s/snapshot; drop oldest beyond

    // ── POST /vss/snapshot ────────────────────────────────────────────────────
    // Store the snapshot as one row: domains → `data`, metadata → `meta`
    // (both JsonToNative → nested sibling documents in Atlas). No per-domain split.
    svr.Post("/vss/snapshot", [&](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            ObxTelemetry snap;
            snap.vehicleId = js(body, "vehicle_id", VEHICLE_ID);
            snap.ts        = body.value("ts", now_ms());
            // `meta` becomes its own JsonToNative column (sibling of data).
            if (body.contains("meta") && !body["meta"].is_null())
                snap.meta = body["meta"].dump();
            // Envelope + meta live in their own columns; `data` keeps only the domain payload.
            body.erase("vehicle_id");
            body.erase("ts");
            body.erase("trip_id");
            body.erase("meta");
            snap.data      = body.dump();
            // If this vehicle is offline (session paused), buffer instead of writing to the
            // synced store, so nothing reaches Atlas until resume. Otherwise write as usual.
            bool buffered = false;
            {
                std::lock_guard<std::mutex> lk(sessionMutex);
                auto pit = sessionPaused.find(snap.vehicleId);
                buffered = (pit != sessionPaused.end() && pit->second);
                if (buffered) {
                    auto& buf = sessionBuffer[snap.vehicleId];
                    buf.push_back(snap);
                    while (buf.size() > SESSION_BUFFER_MAX) buf.pop_front();
                }
            }
            if (!buffered) obt_box.put(snap);
            res.set_content(json{{"success", true}, {"ts", snap.ts}, {"vehicle_id", snap.vehicleId}, {"buffered", buffered}}.dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/latest ─────────────────────────────────────────────────────────
    // Newest snapshot: domains (from `data`) + `meta`. Served from local ObjectBox → offline-capable.
    svr.Get("/vss/latest", [&](const Request& req, Response& res) {
        try {
            // Optional ?vehicleId= selects a per-session vehicle; default keeps single-vehicle behaviour.
            std::string vid = req.has_param("vehicleId") ? req.get_param_value("vehicleId") : VEHICLE_ID;
            if (vid.empty()) vid = VEHICLE_ID;
            // If the vehicle is offline (session paused), its newest snapshot lives in the
            // buffer (not the synced store), so serve that to keep the edge live offline.
            {
                std::lock_guard<std::mutex> lk(sessionMutex);
                auto pit = sessionPaused.find(vid);
                if (pit != sessionPaused.end() && pit->second) {
                    auto bit = sessionBuffer.find(vid);
                    if (bit != sessionBuffer.end() && !bit->second.empty()) {
                        const ObxTelemetry& r = bit->second.back();
                        json out = json::object();
                        try { out = json::parse(r.data); } catch (...) { out = json::object(); }
                        out["ts"] = r.ts;
                        if (!r.meta.empty()) { try { out["meta"] = json::parse(r.meta); } catch (...) {} }
                        out["vehicle_id"] = vid;
                        res.set_content(out.dump(), "application/json");
                        return;
                    }
                }
            }
            json out = json::object();
            if (auto r = obt_box.query(ObxTelemetry_::vehicleId.equals(vid))
                    .order(ObxTelemetry_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                try { out = json::parse(r->data); } catch (...) { out = json::object(); }
                out["ts"] = r->ts;
                if (!r->meta.empty()) {
                    try { out["meta"] = json::parse(r->meta); } catch (...) {}
                }
            }
            out["vehicle_id"] = vid;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/history?minutes=N ──────────────────────────────────────────────
    // Recent snapshots (parsed). Per-domain history endpoints are intentionally
    // dropped for now; revisit alongside the agent tools.
    svr.Get("/vss/history", [&](const Request& req, Response& res) {
        try {
            std::string vid = req.has_param("vehicleId") ? req.get_param_value("vehicleId") : VEHICLE_ID;
            if (vid.empty()) vid = VEHICLE_ID;
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = obt_box.query(
                ObxTelemetry_::vehicleId.equals(vid) &&
                ObxTelemetry_::ts.greaterOrEq(cutoff))
                .order(ObxTelemetry_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o;
                o["ts"] = r.ts;
                try { o["data"] = json::parse(r.data); } catch (...) { o["data"] = r.data; }
                arr.push_back(o);
            }
            res.set_content(json{{"count", (int)arr.size()}, {"snapshots", arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── DELETE /vss/prune ─────────────────────────────────────────────────────
    svr.Delete("/vss/prune", [&](const Request& req, Response& res) {
        try {
            int hours = std::stoi(req.get_param_value("older_than_hours").empty() ?
                std::to_string(cfg.retain_hours) : req.get_param_value("older_than_hours"));
            int64_t cutoff = now_ms() - (int64_t)hours * 3600 * 1000;
            obt_box.query(ObxTelemetry_::ts.lessThan(cutoff)).build().remove();
            res.set_content(json{{"success",true},{"pruned_older_than_hours",hours}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── Sync control ────────────────────────────────────────────────────────────
    // Offline/online is driven at the NETWORK layer — a toggleable proxy (toxiproxy) sits
    // in front of the Sync Server — NOT by stopping this client. ObjectBox v5.1.0 forbids
    // restarting a client (start() throws "startedOnce: State condition failed") and
    // close()+recreate loses the sync cursor (the server then replays its whole dataset →
    // local wipe + full re-download). So the client stays up for the entire process
    // lifetime; when the proxy cuts the connection it buffers local writes in its outgoing
    // queue and auto-reconnects once the proxy is restored. `syncMutex` guards `syncClient`
    // because cpp-httplib serves requests on multiple threads.
    static std::mutex syncMutex;
    // POST /sync/reconnect → force an immediate reconnect attempt. After the proxy is
    // re-enabled the client would otherwise wait out its increasing backoff; triggerReconnect()
    // reconnects promptly. It is allowed on an already-started client (unlike start(), it does
    // not trip the startedOnce guard), so it's safe to call any time.
    svr.Post("/sync/reconnect", [&](const Request&, Response& res) {
        std::lock_guard<std::mutex> lk(syncMutex);
        if (!syncClient) {
            res.status = 409;
            res.set_content(json{{"success", false}, {"available", false},
                {"error", "sync not available in this deployment"}}.dump(), "application/json");
            return;
        }
        try {
            syncClient->triggerReconnect();
            std::cout << "🔁 Sync reconnect triggered (state=" << (int)syncClient->state()
                      << ", buffered=" << syncClient->outgoingMessageCount() << ")\n";
            res.set_content(json{{"success", true}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500; res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    // GET /sync/status → { available, local_count, buffered, sync_state }
    svr.Get("/sync/status", [&](const Request&, Response& res) {
        // available = did this deployment configure sync at all. buffered = outgoing-queue
        // depth (writes not yet acknowledged by the server) — rises while the proxy is cut,
        // drains on reconnect; this is the authoritative "not yet in Atlas" backlog.
        // sync_state = live OBXSyncState (diagnostic). The user-facing paused/connected flags
        // are owned by the proxy layer and are added by the backend (/api/sync/state).
        int64_t buffered = 0;
        int sync_state   = 0;
        {
            std::lock_guard<std::mutex> lk(syncMutex);
            if (syncClient) {
                buffered   = (int64_t)syncClient->outgoingMessageCount();
                sync_state = (int)syncClient->state();
            }
        }
        res.set_content(json{
            {"available", syncConfigured},
            {"local_count", (int64_t)obt_box.count()},
            {"buffered", buffered},
            {"sync_state", sync_state}
        }.dump(), "application/json");
    });
    // GET /vss/count → local objectbox_telemetry row count (edge side of the sync)
    svr.Get("/vss/count", [&](const Request&, Response& res) {
        res.set_content(json{{"count", (int64_t)obt_box.count()}}.dump(), "application/json");
    });

    // ── Per-vehicle session offline/online (used by the backend in session scope) ──
    // These drive Option B: pause routes a vehicle's snapshots to the buffer; resume flushes
    // the buffer into the synced store (which then syncs to Atlas); status reports the panel's
    // per-vehicle numbers. The sync client is never touched, so this coexists with the global
    // toxiproxy path used in global scope.
    auto sessionVid = [&](const Request& req) {
        std::string vid = req.has_param("vehicleId") ? req.get_param_value("vehicleId") : "";
        return vid.empty() ? VEHICLE_ID : vid;
    };
    svr.Post("/session/pause", [&](const Request& req, Response& res) {
        std::string vid = sessionVid(req);
        int64_t buffered;
        {
            std::lock_guard<std::mutex> lk(sessionMutex);
            sessionPaused[vid] = true;
            auto it = sessionBuffer.find(vid);
            buffered = (it != sessionBuffer.end()) ? (int64_t)it->second.size() : 0;
        }
        std::cout << "⏸  Session offline: " << vid << "\n";
        res.set_content(json{{"success", true}, {"paused", true}, {"vehicle_id", vid}, {"buffered", buffered}}.dump(), "application/json");
    });
    svr.Post("/session/resume", [&](const Request& req, Response& res) {
        std::string vid = sessionVid(req);
        std::deque<ObxTelemetry> to_flush;
        {
            std::lock_guard<std::mutex> lk(sessionMutex);
            // Erase both entries (a missing vid already means "online/not paused"), so the maps
            // stay bounded by the number of *currently* offline vehicles, not cumulative sessions.
            sessionPaused.erase(vid);
            auto it = sessionBuffer.find(vid);
            if (it != sessionBuffer.end()) {
                to_flush.swap(it->second);   // take the buffer locally first
                sessionBuffer.erase(it);
            }
        }
        for (auto& r : to_flush) obt_box.put(r);   // flush buffered snapshots → sync to Atlas
        std::cout << "▶  Session online: " << vid << " (flushed " << to_flush.size() << ")\n";
        res.set_content(json{{"success", true}, {"paused", false}, {"vehicle_id", vid}, {"flushed", (int64_t)to_flush.size()}}.dump(), "application/json");
    });
    svr.Get("/session/status", [&](const Request& req, Response& res) {
        std::string vid = sessionVid(req);
        bool paused; int64_t buffered;
        {
            std::lock_guard<std::mutex> lk(sessionMutex);
            auto pit = sessionPaused.find(vid);
            paused = (pit != sessionPaused.end() && pit->second);
            auto bit = sessionBuffer.find(vid);
            buffered = (bit != sessionBuffer.end()) ? (int64_t)bit->second.size() : 0;
        }
        int64_t synced = (int64_t)obt_box.query(ObxTelemetry_::vehicleId.equals(vid)).build().count();
        res.set_content(json{
            {"available", true},
            {"paused", paused},
            {"connected", !paused},
            {"local_count", synced + buffered},   // edge has synced rows + buffered (offline) rows
            {"buffered", buffered}
        }.dump(), "application/json");
    });

    // ── GET /health ───────────────────────────────────────────────────────────
    svr.Get("/health", [&](const Request&, Response& res) {
        res.set_content(json{
            {"status","healthy"},{"service","vss-telemetry-service"},
            {"vehicle_id",VEHICLE_ID},
            {"snapshots", (int64_t)obt_box.count()}
        }.dump(), "application/json");
    });

    std::cout << "🚀 Listening on port " << cfg.port << "\n\n";
    svr.listen("0.0.0.0", cfg.port);
    return 0;
}
