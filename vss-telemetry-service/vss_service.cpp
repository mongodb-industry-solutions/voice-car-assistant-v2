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
    int         retain_hours   = 24;
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
    if (const char* p = std::getenv("PORT")) cfg.port = std::stoi(p);

    std::cout << "📂 DB: " << cfg.db_path << "\n";
    std::cout << "🔄 Sync: " << cfg.sync_server_url << "\n\n";

    auto store = init_store(cfg);
    if (!store) { std::cerr << "❌ Store init failed\n"; return 1; }
    std::cout << "✅ ObjectBox store ready\n";

    std::shared_ptr<obx::SyncClient> syncClient;
    if (cfg.enable_sync && obx::Sync::isAvailable()) {
        try {
            syncClient = obx::Sync::client(*store, cfg.sync_server_url, obx::SyncCredentials::none());
            syncClient->start();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "✅ Sync client started\n\n";
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
            obt_box.put(snap);
            res.set_content(json{{"success", true}, {"ts", snap.ts}, {"vehicle_id", snap.vehicleId}}.dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/latest ─────────────────────────────────────────────────────────
    // Newest snapshot: domains (from `data`) + `meta`. Served from local ObjectBox → offline-capable.
    svr.Get("/vss/latest", [&](const Request&, Response& res) {
        try {
            json out = json::object();
            if (auto r = obt_box.query(ObxTelemetry_::vehicleId.equals(VEHICLE_ID))
                    .order(ObxTelemetry_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                try { out = json::parse(r->data); } catch (...) { out = json::object(); }
                out["ts"] = r->ts;
                if (!r->meta.empty()) {
                    try { out["meta"] = json::parse(r->meta); } catch (...) {}
                }
            }
            out["vehicle_id"] = VEHICLE_ID;
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
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = obt_box.query(
                ObxTelemetry_::vehicleId.equals(VEHICLE_ID) &&
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
