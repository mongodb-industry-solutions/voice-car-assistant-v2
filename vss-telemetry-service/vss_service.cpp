/**
 * VSS Telemetry Service — ObjectBox + Sync
 *
 * Endpoints:
 *   POST /vss/snapshot           — append samples for all domains
 *   POST /vss/meta               — upsert VehicleMeta
 *   GET  /vss/latest             — unified latest snapshot (all domains + meta), offline-capable
 *   GET  /vss/powertrain/history?minutes=N
 *   GET  /vss/battery/history?minutes=N
 *   GET  /vss/location/history?minutes=N
 *   GET  /vss/cabin/history?minutes=N
 *   GET  /vss/adas/history?minutes=N
 *   GET  /vss/chassis/history?minutes=N
 *   DELETE /vss/prune?older_than_hours=N
 *   GET  /health
 */

#define OBX_CPP_FILE
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
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
#define PROP_D(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_Double, pid, puid)
#define PROP_B(nm, pid, puid)  obx_model_property(m, nm, OBXPropertyType_Bool,        pid, puid)
#define PROP_FV(nm, pid, puid) obx_model_property(m, nm, OBXPropertyType_FloatVector, pid, puid)
#define PROP_ID(nm, pid, puid) PROP_L(nm, pid, puid); obx_model_property_flags(m, OBXPropertyFlags_ID)
#define PROP_IDX(iid, iuid) obx_model_property_flags(m, OBXPropertyFlags_INDEXED); \
                             obx_model_property_index_id(m, iid, iuid)
#define LAST_PROP(pid, puid) obx_model_entity_last_property_id(m, pid, puid)

    // ── Shared entities registered for sync

    // Entity 1: manual_chunks
    DEF_ENTITY("manual_chunks", 1, 2807783899453578393ULL);
    PROP_ID("id",          1, 871349036716677797ULL);
    PROP_S("text",         2, 6563616578029045320ULL);
    PROP_S("source_file",  3, 8818095693993590927ULL);
    PROP_I("chunk_index",  4, 6846133054869205678ULL);
    PROP_FV("embedding",   5, 6898708364220688226ULL); PROP_IDX(1, 4357812374228481003ULL);
    PROP_L("syncClock",    6, 1234567890123456789ULL);
    LAST_PROP(6, 1234567890123456789ULL);

    // Entity 2: manuals
    DEF_ENTITY("manuals", 2, 3456789012345678901ULL);
    PROP_ID("id",          1, 2345678901234567890ULL);
    PROP_S("filename",     2, 3456789012345678902ULL);
    PROP_S("make",         3, 4567890123456789013ULL);
    PROP_S("model",        4, 5678901234567890124ULL);
    PROP_I("total_chunks", 5, 6789012345678901235ULL);
    PROP_S("status",       6, 7890123456789012346ULL);
    PROP_L("syncClock",    7, 9876543210987654321ULL);
    LAST_PROP(7, 9876543210987654321ULL);

    // Entity 3: conversations
    DEF_ENTITY("conversations", 3, 1111222233334444555ULL);
    PROP_ID("id",              1, 1111222233334444556ULL);
    PROP_S("conversation_id",  2, 2222333344445555666ULL); PROP_IDX(2, 2222222222222222222ULL);
    PROP_S("user_id",          3, 3333444455556666777ULL); PROP_IDX(3, 3333333333333333333ULL);
    PROP_L("timestamp",        4, 4444555566667777888ULL); PROP_IDX(4, 4444444444444444444ULL);
    PROP_S("role",             5, 5555666677778888999ULL);
    PROP_S("message",          6, 6666777788889999111ULL);
    PROP_S("sources",          7, 7777888899991111222ULL);
    PROP_L("syncClock",        8, 8888999911112222333ULL);
    LAST_PROP(8, 8888999911112222333ULL);

    // Entity 4: telemetry_snapshots
    DEF_ENTITY("telemetry_snapshots", 4, 2222333344445555777ULL);
    PROP_ID("id",                 1, 2222333344445555778ULL);
    PROP_L("timestamp",           2, 3333444455556666888ULL); PROP_IDX(5, 5555555555555555555ULL);
    PROP_S("vehicle_id",          3, 4444555566667777999ULL);
    PROP_S("driving_mode",        4, 5555666677778888000ULL);
    PROP_I("anomaly_count",       5, 6666777788889999222ULL);
    PROP_S("engine_data",         6, 7777888899990000333ULL);
    PROP_S("tire_data",           7, 8888999900001111444ULL);
    PROP_S("battery_data",        8, 9999000011112222555ULL);
    PROP_S("fuel_data",           9, 1111222233334444666ULL);
    PROP_S("transmission_data",  10, 2222333344445555888ULL);
    PROP_S("brake_data",         11, 3333444455556666999ULL);
    PROP_L("syncClock",          12, 4444555566668888111ULL);
    LAST_PROP(12, 4444555566668888111ULL);

    // Entity 10: VehicleMeta
    DEF_ENTITY("VehicleMeta", 10, 6010000000000000);
    PROP_ID("id",               1, 6010000000000001);
    PROP_S("vehicleId",         2, 6010000000000002);
    PROP_S("vin",               3, 6010000000000003);
    PROP_S("oem",               4, 6010000000000004);
    PROP_S("model",             5, 6010000000000005);
    PROP_S("platform",          6, 6010000000000006);
    PROP_S("softwareVersion",   7, 6010000000000007);
    PROP_L("createdAt",         8, 6010000000000008);
    PROP_L("updatedAt",         9, 6010000000000009);
    PROP_L("syncClock",        10, 6010000000000010);
    PROP_F("fuelTankCapacityL", 11, 6010000000000011);
    PROP_F("batteryCapacityKwh",12, 6010000000000012);
    PROP_I("wheelbaseMm",      13, 6010000000000013);
    PROP_I("curbWeightKg",     14, 6010000000000014);
    PROP_S("powertrainType",   15, 6010000000000015);
    PROP_S("drivetrainType",   16, 6010000000000016);
    LAST_PROP(16, 6010000000000016);

    // Entity 11: SignalDefinition
    DEF_ENTITY("SignalDefinition", 11, 6011000000000000);
    PROP_ID("id",              1, 6011000000000001);
    PROP_S("vssPath",          2, 6011000000000002);
    PROP_S("component",        3, 6011000000000003);
    PROP_S("signalKind",       4, 6011000000000004);
    PROP_S("valueType",        5, 6011000000000005);
    PROP_S("unit",             6, 6011000000000006);
    PROP_B("writable",         7, 6011000000000007);
    PROP_S("latestGroup",      8, 6011000000000008);
    PROP_S("historyGroup",     9, 6011000000000009);
    PROP_S("historyMode",     10, 6011000000000010);
    PROP_I("samplePeriodMs",  11, 6011000000000011);
    PROP_I("retainHours",     12, 6011000000000012);
    PROP_B("enabled",         13, 6011000000000013);
    PROP_L("syncClock",       14, 6011000000000014);
    LAST_PROP(14, 6011000000000014);

    // Entity 19: PowertrainSample
    DEF_ENTITY("PowertrainSample", 19, 6019000000000000);
    PROP_ID("id",              1, 6019000000000001);
    PROP_S("vehicleId",        2, 6019000000000002); PROP_IDX(13, 6019000000000100);
    PROP_L("ts",               3, 6019000000000003); PROP_IDX(14, 6019000000000200);
    PROP_S("tripId",           4, 6019000000000004);
    PROP_F("speedKph",         5, 6019000000000005);
    PROP_F("engineRpm",        6, 6019000000000006);
    PROP_F("fuelLevelPct",     7, 6019000000000007);
    PROP_F("fuelRateLph",      8, 6019000000000008);
    PROP_F("coolantTempC",     9, 6019000000000009);
    PROP_F("throttlePct",     10, 6019000000000010);
    PROP_I("gear",            11, 6019000000000011);
    PROP_L("syncClock",       12, 6019000000000012);
    LAST_PROP(12, 6019000000000012);

    // Entity 20: BatterySample
    DEF_ENTITY("BatterySample", 20, 6020000000000000);
    PROP_ID("id",              1, 6020000000000001);
    PROP_S("vehicleId",        2, 6020000000000002); PROP_IDX(15, 6020000000000100);
    PROP_L("ts",               3, 6020000000000003); PROP_IDX(16, 6020000000000200);
    PROP_S("tripId",           4, 6020000000000004);
    PROP_F("socPct",           5, 6020000000000005);
    PROP_F("sohPct",           6, 6020000000000006);
    PROP_F("batteryTempC",     7, 6020000000000007);
    PROP_F("chargingPowerKw",  8, 6020000000000008);
    PROP_F("estimatedRangeKm", 9, 6020000000000009);
    PROP_F("voltageV",        10, 6020000000000010);
    PROP_F("currentA",        11, 6020000000000011);
    PROP_L("syncClock",       12, 6020000000000012);
    LAST_PROP(12, 6020000000000012);

    // Entity 21: LocationSample
    DEF_ENTITY("LocationSample", 21, 6021000000000000);
    PROP_ID("id",              1, 6021000000000001);
    PROP_S("vehicleId",        2, 6021000000000002); PROP_IDX(17, 6021000000000100);
    PROP_L("ts",               3, 6021000000000003); PROP_IDX(18, 6021000000000200);
    PROP_S("tripId",           4, 6021000000000004);
    PROP_F("altitudeM",        7, 6021000000000007);
    PROP_F("headingDeg",       8, 6021000000000008);
    PROP_F("speedKph",         9, 6021000000000009);
    PROP_F("accuracyM",       10, 6021000000000010);
    PROP_L("syncClock",       11, 6021000000000011);
    PROP_S("locationGeoJson", 12, 6021000000000012);
    LAST_PROP(12, 6021000000000012);

    // Entity 22: CabinSample
    DEF_ENTITY("CabinSample", 22, 6022000000000000);
    PROP_ID("id",              1, 6022000000000001);
    PROP_S("vehicleId",        2, 6022000000000002); PROP_IDX(19, 6022000000000100);
    PROP_L("ts",               3, 6022000000000003); PROP_IDX(20, 6022000000000200);
    PROP_S("tripId",           4, 6022000000000004);
    PROP_F("insideTempC",      5, 6022000000000005);
    PROP_F("outsideTempC",     6, 6022000000000006);
    PROP_S("hvacMode",         7, 6022000000000007);
    PROP_I("fanSpeed",         8, 6022000000000008);
    PROP_L("syncClock",        9, 6022000000000009);
    LAST_PROP(9, 6022000000000009);

    // Entity 23: AdasSample
    DEF_ENTITY("AdasSample", 23, 6023000000000000);
    PROP_ID("id",              1, 6023000000000001);
    PROP_S("vehicleId",        2, 6023000000000002); PROP_IDX(21, 6023000000000100);
    PROP_L("ts",               3, 6023000000000003); PROP_IDX(22, 6023000000000200);
    PROP_S("tripId",           4, 6023000000000004);
    PROP_B("cruiseEnabled",    5, 6023000000000005);
    PROP_F("cruiseSetSpeedKph",6, 6023000000000006);
    PROP_B("laneKeepAssistOn", 7, 6023000000000007);
    PROP_B("collisionWarningActive",8,6023000000000008);
    PROP_L("syncClock",        9, 6023000000000009);
    LAST_PROP(9, 6023000000000009);

    // Entity 24: ChassisSample
    DEF_ENTITY("ChassisSample", 24, 6026000000000000);
    PROP_ID("id",                    1, 6026000000000001);
    PROP_S("vehicleId",              2, 6026000000000002); PROP_IDX(23, 6026000000000100);
    PROP_L("ts",                     3, 6026000000000003); PROP_IDX(24, 6026000000000200);
    PROP_S("tripId",                 4, 6026000000000004);
    PROP_F("steeringAngleDeg",       5, 6026000000000005);
    PROP_F("brakePedalPct",          6, 6026000000000006);
    PROP_F("tirePressureFlKpa",      7, 6026000000000007);
    PROP_F("tirePressureFrKpa",      8, 6026000000000008);
    PROP_F("tirePressureRlKpa",      9, 6026000000000009);
    PROP_F("tirePressureRrKpa",     10, 6026000000000010);
    PROP_B("absActive",             11, 6026000000000011);
    PROP_B("tractionControlActive", 12, 6026000000000012);
    PROP_L("syncClock",             13, 6026000000000013);
    LAST_PROP(13, 6026000000000013);

    obx_model_last_entity_id(m, 24, 6026000000000000);
    obx_model_last_index_id(m,  24, 6026000000000200);

#undef DEF_ENTITY
#undef PROP_L
#undef PROP_S
#undef PROP_I
#undef PROP_F
#undef PROP_D
#undef PROP_B
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
static float jf(const json& j, const char* k, float def = 0.f) {
    return j.contains(k) && !j[k].is_null() ? j[k].get<float>() : def;
}
static double jd(const json& j, const char* k, double def = 0.0) {
    return j.contains(k) && !j[k].is_null() ? j[k].get<double>() : def;
}
static int32_t ji(const json& j, const char* k, int32_t def = 0) {
    return j.contains(k) && !j[k].is_null() ? j[k].get<int32_t>() : def;
}
static bool jb(const json& j, const char* k, bool def = false) {
    return j.contains(k) && !j[k].is_null() ? j[k].get<bool>() : def;
}
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

    // Boxes
    auto vm_box    = store->box<VehicleMeta>();
    auto sig_box   = store->box<SignalDefinition>();
    auto pts_box   = store->box<PowertrainSample>();
    auto bats_box  = store->box<BatterySample>();
    auto locs_box  = store->box<LocationSample>();
    auto cabs_box  = store->box<CabinSample>();
    auto adas_s_box= store->box<AdasSample>();
    auto chs_s_box = store->box<ChassisSample>();

    // Background pruning thread — runs every hour
    std::thread prune_thread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::hours(1));
            int64_t cutoff = now_ms() - (int64_t)cfg.retain_hours * 3600 * 1000;
            try {
                pts_box.query(PowertrainSample_::ts.lessThan(cutoff)).build().remove();
                bats_box.query(BatterySample_::ts.lessThan(cutoff)).build().remove();
                locs_box.query(LocationSample_::ts.lessThan(cutoff)).build().remove();
                cabs_box.query(CabinSample_::ts.lessThan(cutoff)).build().remove();
                adas_s_box.query(AdasSample_::ts.lessThan(cutoff)).build().remove();
                chs_s_box.query(ChassisSample_::ts.lessThan(cutoff)).build().remove();
                std::cout << "🗑️  Pruned samples older than " << cfg.retain_hours << "h\n";
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
    svr.Post("/vss/snapshot", [&](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string vid  = js(body, "vehicle_id", VEHICLE_ID);
            int64_t     ts   = body.value("ts", now_ms());
            std::string trip = js(body, "trip_id", "");

            // ── PowertrainSample ──────────────────────────────────────────
            if (body.contains("powertrain")) {
                const auto& p = body["powertrain"];
                PowertrainSample sample;
                sample.vehicleId    = vid; sample.ts = ts; sample.tripId = trip;
                sample.speedKph     = jf(p,"speedKph");
                sample.engineRpm    = jf(p,"engineRpm");
                sample.fuelLevelPct = jf(p,"fuelLevelPct");
                sample.fuelRateLph  = jf(p,"fuelRateLph");
                sample.coolantTempC = jf(p,"coolantTempC");
                sample.throttlePct  = jf(p,"throttlePct");
                sample.gear         = ji(p,"gear");
                pts_box.put(sample);
            }

            // ── BatterySample ─────────────────────────────────────────────
            if (body.contains("battery")) {
                const auto& p = body["battery"];
                BatterySample sample;
                sample.vehicleId        = vid; sample.ts = ts; sample.tripId = trip;
                sample.socPct           = jf(p,"socPct");
                sample.sohPct           = jf(p,"sohPct",100.f);
                sample.batteryTempC     = jf(p,"batteryTempC");
                sample.chargingPowerKw  = jf(p,"chargingPowerKw");
                sample.estimatedRangeKm = jf(p,"estimatedRangeKm");
                sample.voltageV         = jf(p,"voltageV");
                sample.currentA         = jf(p,"currentA");
                bats_box.put(sample);
            }

            // ── ChassisSample ─────────────────────────────────────────────
            if (body.contains("chassis")) {
                const auto& p = body["chassis"];
                ChassisSample sample;
                sample.vehicleId             = vid; sample.ts = ts; sample.tripId = trip;
                sample.steeringAngleDeg      = jf(p,"steeringAngleDeg");
                sample.brakePedalPct         = jf(p,"brakePedalPct");
                sample.tirePressureFlKpa     = jf(p,"tirePressureFlKpa");
                sample.tirePressureFrKpa     = jf(p,"tirePressureFrKpa");
                sample.tirePressureRlKpa     = jf(p,"tirePressureRlKpa");
                sample.tirePressureRrKpa     = jf(p,"tirePressureRrKpa");
                sample.absActive             = jb(p,"absActive");
                sample.tractionControlActive = jb(p,"tractionControlActive");
                chs_s_box.put(sample);
            }

            // ── CabinSample ───────────────────────────────────────────────
            if (body.contains("cabin")) {
                const auto& p = body["cabin"];
                CabinSample sample;
                sample.vehicleId   = vid; sample.ts = ts; sample.tripId = trip;
                sample.insideTempC = jf(p,"insideTempC");
                sample.outsideTempC= jf(p,"outsideTempC");
                sample.hvacMode    = js(p,"hvacMode","auto");
                sample.fanSpeed    = ji(p,"fanSpeed");
                cabs_box.put(sample);
            }

            // ── LocationSample ────────────────────────────────────────────
            if (body.contains("location")) {
                const auto& p = body["location"];
                LocationSample sample;
                sample.vehicleId = vid; sample.ts = ts; sample.tripId = trip;
                double lat = jd(p,"latitude");
                double lon = jd(p,"longitude");
                sample.altitudeM = jf(p,"altitudeM");
                sample.headingDeg= jf(p,"headingDeg");
                sample.speedKph  = jf(p,"speedKph");
                sample.accuracyM = jf(p,"accuracyM");
                { json geo = json::object(); geo["type"] = "Point"; geo["coordinates"] = json::array({lon, lat}); sample.locationGeoJson = geo.dump(); }
                locs_box.put(sample);
            }

            // ── AdasSample ────────────────────────────────────────────────
            if (body.contains("adas")) {
                const auto& p = body["adas"];
                AdasSample sample;
                sample.vehicleId             = vid; sample.ts = ts; sample.tripId = trip;
                sample.cruiseEnabled         = jb(p,"cruiseEnabled");
                sample.cruiseSetSpeedKph     = jf(p,"cruiseSetSpeedKph");
                sample.laneKeepAssistOn      = jb(p,"laneKeepAssistOn");
                sample.collisionWarningActive= jb(p,"collisionWarningActive");
                adas_s_box.put(sample);
            }

            json resp = {{"success", true}, {"ts", ts}, {"vehicle_id", vid}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── POST /vss/meta ────────────────────────────────────────────────────────
    svr.Post("/vss/meta", [&](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string vid = js(body, "vehicle_id", VEHICLE_ID);

            VehicleMeta vm;
            auto existing = vm_box.query(VehicleMeta_::vehicleId.equals(vid)).build().find();
            if (!existing.empty()) {
                vm = existing[0];
            } else {
                vm.createdAt = now_ms();
            }
            vm.vehicleId          = vid;
            vm.vin                = js(body, "vin", vid);
            vm.oem                = js(body, "oem", "");
            vm.modelName          = js(body, "model", "");
            vm.platform           = js(body, "platform", "VSS-v4");
            vm.softwareVersion    = js(body, "softwareVersion", "1.0.0");
            vm.fuelTankCapacityL  = body.value("fuelTankCapacityL", 0.f);
            vm.batteryCapacityKwh = body.value("batteryCapacityKwh", 0.f);
            vm.wheelbaseMm        = body.value("wheelbaseMm", 0);
            vm.curbWeightKg       = body.value("curbWeightKg", 0);
            vm.powertrainType     = js(body, "powertrainType", "");
            vm.drivetrainType     = js(body, "drivetrainType", "");
            vm.updatedAt          = now_ms();
            obx_id id = vm_box.put(vm);

            std::cout << "✅ VehicleMeta upserted (id=" << id << ")\n";
            res.set_content(json{{"success", true}, {"id", id}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/powertrain/history ───────────────────────────────────────────
    svr.Get("/vss/powertrain/history", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = pts_box.query(
                PowertrainSample_::vehicleId.equals(VEHICLE_ID) &&
                PowertrainSample_::ts.greaterOrEq(cutoff))
                .order(PowertrainSample_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o; o["ts"]=r.ts; o["speedKph"]=r.speedKph; o["engineRpm"]=r.engineRpm;
                o["fuelLevelPct"]=r.fuelLevelPct; o["coolantTempC"]=r.coolantTempC;
                o["throttlePct"]=r.throttlePct; o["gear"]=r.gear; arr.push_back(o);
            }
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/battery/history ──────────────────────────────────────────────
    svr.Get("/vss/battery/history", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = bats_box.query(
                BatterySample_::vehicleId.equals(VEHICLE_ID) &&
                BatterySample_::ts.greaterOrEq(cutoff))
                .order(BatterySample_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o; o["ts"]=r.ts; o["socPct"]=r.socPct; o["sohPct"]=r.sohPct;
                o["voltageV"]=r.voltageV; o["currentA"]=r.currentA;
                o["batteryTempC"]=r.batteryTempC; o["estimatedRangeKm"]=r.estimatedRangeKm; arr.push_back(o);
            }
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/location/history ─────────────────────────────────────────────
    svr.Get("/vss/location/history", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = locs_box.query(
                LocationSample_::vehicleId.equals(VEHICLE_ID) &&
                LocationSample_::ts.greaterOrEq(cutoff))
                .order(LocationSample_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o; o["ts"]=r.ts; o["locationGeoJson"]=r.locationGeoJson;
                o["headingDeg"]=r.headingDeg; o["speedKph"]=r.speedKph; o["altitudeM"]=r.altitudeM; arr.push_back(o);
            }
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/cabin/history ────────────────────────────────────────────────
    svr.Get("/vss/cabin/history", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = cabs_box.query(
                CabinSample_::vehicleId.equals(VEHICLE_ID) &&
                CabinSample_::ts.greaterOrEq(cutoff))
                .order(CabinSample_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o; o["ts"]=r.ts; o["insideTempC"]=r.insideTempC;
                o["outsideTempC"]=r.outsideTempC; o["hvacMode"]=r.hvacMode; o["fanSpeed"]=r.fanSpeed; arr.push_back(o);
            }
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/adas/history ─────────────────────────────────────────────────
    svr.Get("/vss/adas/history", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = adas_s_box.query(
                AdasSample_::vehicleId.equals(VEHICLE_ID) &&
                AdasSample_::ts.greaterOrEq(cutoff))
                .order(AdasSample_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o; o["ts"]=r.ts; o["cruiseEnabled"]=r.cruiseEnabled;
                o["laneKeepAssistOn"]=r.laneKeepAssistOn; o["collisionWarningActive"]=r.collisionWarningActive; arr.push_back(o);
            }
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/chassis/history ──────────────────────────────────────────────
    svr.Get("/vss/chassis/history", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "10" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = chs_s_box.query(
                ChassisSample_::vehicleId.equals(VEHICLE_ID) &&
                ChassisSample_::ts.greaterOrEq(cutoff))
                .order(ChassisSample_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows) {
                json o; o["ts"]=r.ts;
                o["tirePressureFlKpa"]=r.tirePressureFlKpa; o["tirePressureFrKpa"]=r.tirePressureFrKpa;
                o["tirePressureRlKpa"]=r.tirePressureRlKpa; o["tirePressureRrKpa"]=r.tirePressureRrKpa;
                o["steeringAngleDeg"]=r.steeringAngleDeg; o["brakePedalPct"]=r.brakePedalPct;
                o["absActive"]=r.absActive; o["tractionControlActive"]=r.tractionControlActive;
                arr.push_back(o);
            }
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/latest ───────────────────────────────────────────────────────
    // Unified latest snapshot assembled from the newest sample of each domain +
    // VehicleMeta. Served straight from local ObjectBox, so it works offline
    // (unlike the Atlas-derived telemetry-status). Flat shape consumed by the UI.
    svr.Get("/vss/latest", [&](const Request&, Response& res) {
        try {
            json out;
            out["vehicle_id"] = VEHICLE_ID;

            if (auto r = pts_box.query(PowertrainSample_::vehicleId.equals(VEHICLE_ID))
                    .order(PowertrainSample_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                out["powertrain"] = {
                    {"ts", r->ts}, {"speedKph", r->speedKph}, {"engineRpm", r->engineRpm},
                    {"fuelLevelPct", r->fuelLevelPct}, {"fuelRateLph", r->fuelRateLph},
                    {"coolantTempC", r->coolantTempC}, {"throttlePct", r->throttlePct}, {"gear", r->gear}
                };
            }
            if (auto r = bats_box.query(BatterySample_::vehicleId.equals(VEHICLE_ID))
                    .order(BatterySample_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                out["battery"] = {
                    {"ts", r->ts}, {"socPct", r->socPct}, {"sohPct", r->sohPct},
                    {"batteryTempC", r->batteryTempC}, {"chargingPowerKw", r->chargingPowerKw},
                    {"estimatedRangeKm", r->estimatedRangeKm}, {"voltageV", r->voltageV}, {"currentA", r->currentA}
                };
            }
            if (auto r = chs_s_box.query(ChassisSample_::vehicleId.equals(VEHICLE_ID))
                    .order(ChassisSample_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                out["chassis"] = {
                    {"ts", r->ts}, {"steeringAngleDeg", r->steeringAngleDeg}, {"brakePedalPct", r->brakePedalPct},
                    {"tirePressureFlKpa", r->tirePressureFlKpa}, {"tirePressureFrKpa", r->tirePressureFrKpa},
                    {"tirePressureRlKpa", r->tirePressureRlKpa}, {"tirePressureRrKpa", r->tirePressureRrKpa},
                    {"absActive", r->absActive}, {"tractionControlActive", r->tractionControlActive}
                };
            }
            if (auto r = cabs_box.query(CabinSample_::vehicleId.equals(VEHICLE_ID))
                    .order(CabinSample_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                out["cabin"] = {
                    {"ts", r->ts}, {"insideTempC", r->insideTempC}, {"outsideTempC", r->outsideTempC},
                    {"hvacMode", r->hvacMode}, {"fanSpeed", r->fanSpeed}
                };
            }
            if (auto r = locs_box.query(LocationSample_::vehicleId.equals(VEHICLE_ID))
                    .order(LocationSample_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                json loc = {
                    {"ts", r->ts}, {"altitudeM", r->altitudeM}, {"headingDeg", r->headingDeg},
                    {"speedKph", r->speedKph}, {"accuracyM", r->accuracyM}
                };
                if (!r->locationGeoJson.empty()) {
                    try { loc["locationGeoJson"] = json::parse(r->locationGeoJson); }
                    catch (...) { loc["locationGeoJson"] = r->locationGeoJson; }
                }
                out["location"] = loc;
            }
            if (auto r = adas_s_box.query(AdasSample_::vehicleId.equals(VEHICLE_ID))
                    .order(AdasSample_::ts, OBXOrderFlags_DESCENDING).build().findFirst()) {
                out["adas"] = {
                    {"ts", r->ts}, {"cruiseEnabled", r->cruiseEnabled}, {"cruiseSetSpeedKph", r->cruiseSetSpeedKph},
                    {"laneKeepAssistOn", r->laneKeepAssistOn}, {"collisionWarningActive", r->collisionWarningActive}
                };
            }
            {
                auto metas = vm_box.query(VehicleMeta_::vehicleId.equals(VEHICLE_ID)).build().find();
                if (!metas.empty()) {
                    auto& m = metas[0];
                    out["meta"] = {
                        {"vehicleId", m.vehicleId}, {"vin", m.vin}, {"oem", m.oem}, {"model", m.modelName},
                        {"powertrainType", m.powertrainType}, {"drivetrainType", m.drivetrainType},
                        {"fuelTankCapacityL", m.fuelTankCapacityL}, {"batteryCapacityKwh", m.batteryCapacityKwh},
                        {"wheelbaseMm", m.wheelbaseMm}, {"curbWeightKg", m.curbWeightKg}
                    };
                }
            }
            res.set_content(out.dump(), "application/json");
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
            pts_box.query(PowertrainSample_::ts.lessThan(cutoff)).build().remove();
            bats_box.query(BatterySample_::ts.lessThan(cutoff)).build().remove();
            locs_box.query(LocationSample_::ts.lessThan(cutoff)).build().remove();
            cabs_box.query(CabinSample_::ts.lessThan(cutoff)).build().remove();
            adas_s_box.query(AdasSample_::ts.lessThan(cutoff)).build().remove();
            chs_s_box.query(ChassisSample_::ts.lessThan(cutoff)).build().remove();
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
            {"powertrain_samples", (int64_t)pts_box.count()},
            {"battery_samples",    (int64_t)bats_box.count()},
            {"location_samples",   (int64_t)locs_box.count()},
            {"chassis_samples",    (int64_t)chs_s_box.count()}
        }.dump(), "application/json");
    });

    std::cout << "🚀 Listening on port " << cfg.port << "\n\n";
    svr.listen("0.0.0.0", cfg.port);
    return 0;
}
