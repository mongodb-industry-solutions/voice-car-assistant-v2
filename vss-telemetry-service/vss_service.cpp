/**
 * VSS Telemetry Service — ObjectBox + Sync
 *
 * Endpoints:
 *   POST /vss/snapshot           — upsert all State entities + append Samples
 *   POST /vss/event              — append a VehicleEvent
 *   GET  /vss/latest             — full current state (all domains)
 *   GET  /vss/powertrain/history?minutes=N
 *   GET  /vss/battery/history?minutes=N
 *   GET  /vss/location/history?minutes=N
 *   GET  /vss/cabin/history?minutes=N
 *   GET  /vss/adas/history?minutes=N
 *   GET  /vss/events?minutes=N
 *   DELETE /vss/prune?older_than_hours=N
 *   GET  /health
 */

#define OBX_CPP_FILE
#include <iostream>
#include <memory>
#include <string>
#include <mutex>
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

// ── State ID cache (one row per domain per vehicle) ───────────────────────────
struct StateIds {
    obx_id vehicle_meta  = 0;
    obx_id powertrain    = 0;
    obx_id battery       = 0;
    obx_id chassis       = 0;
    obx_id cabin         = 0;
    obx_id location      = 0;
    obx_id adas          = 0;
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

    // ── Shared entities — registered so the sync client can apply incoming
    //    transactions from other services; no Box<T> needed here.

    // Entity 1: manual_chunks (search-service)
    DEF_ENTITY("manual_chunks", 1, 2807783899453578393ULL);
    PROP_ID("id",          1, 871349036716677797ULL);
    PROP_S("text",         2, 6563616578029045320ULL);
    PROP_S("source_file",  3, 8818095693993590927ULL);
    PROP_I("chunk_index",  4, 6846133054869205678ULL);
    PROP_FV("embedding",   5, 6898708364220688226ULL); PROP_IDX(1, 4357812374228481003ULL);
    PROP_L("syncClock",    6, 1234567890123456789ULL);
    LAST_PROP(6, 1234567890123456789ULL);

    // Entity 2: manuals (search-service)
    DEF_ENTITY("manuals", 2, 3456789012345678901ULL);
    PROP_ID("id",          1, 2345678901234567890ULL);
    PROP_S("filename",     2, 3456789012345678902ULL);
    PROP_S("make",         3, 4567890123456789013ULL);
    PROP_S("model",        4, 5678901234567890124ULL);
    PROP_I("total_chunks", 5, 6789012345678901235ULL);
    PROP_S("status",       6, 7890123456789012346ULL);
    PROP_L("syncClock",    7, 9876543210987654321ULL);
    LAST_PROP(7, 9876543210987654321ULL);

    // Entity 3: conversations (conversation-service)
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

    // Entity 4: telemetry_snapshots (telemetry-service)
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

    // Entity 13: PowertrainState
    DEF_ENTITY("PowertrainState", 13, 6013000000000000);
    PROP_ID("id",              1, 6013000000000001);
    PROP_S("vehicleId",        2, 6013000000000002); PROP_IDX(7, 6013000000000100);
    PROP_L("updatedAt",        3, 6013000000000003);
    PROP_F("speedKph",         4, 6013000000000004);
    PROP_F("engineRpm",        5, 6013000000000005);
    PROP_F("odometerKm",       6, 6013000000000006);
    PROP_F("fuelLevelPct",     7, 6013000000000007);
    PROP_F("fuelRateLph",      8, 6013000000000008);
    PROP_F("coolantTempC",     9, 6013000000000009);
    PROP_F("throttlePct",     10, 6013000000000010);
    PROP_I("gear",            11, 6013000000000011);
    PROP_B("ignitionOn",      12, 6013000000000012);
    PROP_S("tripId",          13, 6013000000000013);
    PROP_L("syncClock",       14, 6013000000000014);
    LAST_PROP(14, 6013000000000014);

    // Entity 14: BatteryState
    DEF_ENTITY("BatteryState", 14, 6014000000000000);
    PROP_ID("id",              1, 6014000000000001);
    PROP_S("vehicleId",        2, 6014000000000002); PROP_IDX(8, 6014000000000100);
    PROP_L("updatedAt",        3, 6014000000000003);
    PROP_F("socPct",           4, 6014000000000004);
    PROP_F("sohPct",           5, 6014000000000005);
    PROP_F("batteryTempC",     6, 6014000000000006);
    PROP_S("chargingState",    7, 6014000000000007);
    PROP_F("chargingPowerKw",  8, 6014000000000008);
    PROP_F("estimatedRangeKm", 9, 6014000000000009);
    PROP_F("voltageV",        10, 6014000000000010);
    PROP_F("currentA",        11, 6014000000000011);
    PROP_L("syncClock",       12, 6014000000000012);
    LAST_PROP(12, 6014000000000012);

    // Entity 15: ChassisState
    DEF_ENTITY("ChassisState", 15, 6015000000000000);
    PROP_ID("id",              1, 6015000000000001);
    PROP_S("vehicleId",        2, 6015000000000002); PROP_IDX(9, 6015000000000100);
    PROP_L("updatedAt",        3, 6015000000000003);
    PROP_F("steeringAngleDeg", 4, 6015000000000004);
    PROP_F("brakePedalPct",    5, 6015000000000005);
    PROP_F("tirePressureFlKpa",6, 6015000000000006);
    PROP_F("tirePressureFrKpa",7, 6015000000000007);
    PROP_F("tirePressureRlKpa",8, 6015000000000008);
    PROP_F("tirePressureRrKpa",9, 6015000000000009);
    PROP_B("absActive",       10, 6015000000000010);
    PROP_B("tractionControlActive",11,6015000000000011);
    PROP_L("syncClock",       12, 6015000000000012);
    LAST_PROP(12, 6015000000000012);

    // Entity 16: CabinState
    DEF_ENTITY("CabinState", 16, 6016000000000000);
    PROP_ID("id",              1, 6016000000000001);
    PROP_S("vehicleId",        2, 6016000000000002); PROP_IDX(10, 6016000000000100);
    PROP_L("updatedAt",        3, 6016000000000003);
    PROP_F("insideTempC",      4, 6016000000000004);
    PROP_F("outsideTempC",     5, 6016000000000005);
    PROP_S("hvacMode",         6, 6016000000000006);
    PROP_I("fanSpeed",         7, 6016000000000007);
    PROP_B("driverDoorOpen",   8, 6016000000000008);
    PROP_B("passengerDoorOpen",9, 6016000000000009);
    PROP_B("rearLeftDoorOpen", 10,6016000000000010);
    PROP_B("rearRightDoorOpen",11,6016000000000011);
    PROP_B("doorsLocked",      12,6016000000000012);
    PROP_B("seatbeltDriverFastened",13,6016000000000013);
    PROP_L("syncClock",        14,6016000000000014);
    LAST_PROP(14, 6016000000000014);

    // Entity 17: LocationState
    DEF_ENTITY("LocationState", 17, 6017000000000000);
    PROP_ID("id",              1, 6017000000000001);
    PROP_S("vehicleId",        2, 6017000000000002); PROP_IDX(11, 6017000000000100);
    PROP_L("updatedAt",        3, 6017000000000003);
    PROP_D("latitude",         4, 6017000000000004);
    PROP_D("longitude",        5, 6017000000000005);
    PROP_F("altitudeM",        6, 6017000000000006);
    PROP_F("headingDeg",       7, 6017000000000007);
    PROP_F("speedKph",         8, 6017000000000008);
    PROP_F("accuracyM",        9, 6017000000000009);
    PROP_S("geohash",         10, 6017000000000010);
    PROP_L("syncClock",       11, 6017000000000011);
    LAST_PROP(11, 6017000000000011);

    // Entity 18: AdasState
    DEF_ENTITY("AdasState", 18, 6018000000000000);
    PROP_ID("id",              1, 6018000000000001);
    PROP_S("vehicleId",        2, 6018000000000002); PROP_IDX(12, 6018000000000100);
    PROP_L("updatedAt",        3, 6018000000000003);
    PROP_B("cruiseEnabled",    4, 6018000000000004);
    PROP_F("cruiseSetSpeedKph",5, 6018000000000005);
    PROP_B("laneKeepAssistOn", 6, 6018000000000006);
    PROP_B("parkingAssistOn",  7, 6018000000000007);
    PROP_B("collisionWarningActive",8,6018000000000008);
    PROP_S("autopilotMode",    9, 6018000000000009);
    PROP_L("syncClock",       10, 6018000000000010);
    LAST_PROP(10, 6018000000000010);

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
    PROP_D("latitude",         5, 6021000000000005);
    PROP_D("longitude",        6, 6021000000000006);
    PROP_F("altitudeM",        7, 6021000000000007);
    PROP_F("headingDeg",       8, 6021000000000008);
    PROP_F("speedKph",         9, 6021000000000009);
    PROP_F("accuracyM",       10, 6021000000000010);
    PROP_L("syncClock",       11, 6021000000000011);
    LAST_PROP(11, 6021000000000011);

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

    // Entity 24: VehicleEvent
    DEF_ENTITY("VehicleEvent", 24, 6024000000000000);
    PROP_ID("id",              1, 6024000000000001);
    PROP_S("vehicleId",        2, 6024000000000002); PROP_IDX(23, 6024000000000100);
    PROP_L("ts",               3, 6024000000000003); PROP_IDX(24, 6024000000000200);
    PROP_S("tripId",           4, 6024000000000004);
    PROP_S("eventType",        5, 6024000000000005);
    PROP_S("severity",         6, 6024000000000006);
    PROP_S("vssPath",          7, 6024000000000007);
    PROP_S("code",             8, 6024000000000008);
    PROP_S("description",      9, 6024000000000009);
    PROP_S("payloadJson",     10, 6024000000000010);
    PROP_L("syncClock",       11, 6024000000000011);
    LAST_PROP(11, 6024000000000011);

    // Entity 25: ExtensionPayload
    DEF_ENTITY("ExtensionPayload", 25, 6025000000000000);
    PROP_ID("id",              1, 6025000000000001);
    PROP_S("vehicleId",        2, 6025000000000002); PROP_IDX(25, 6025000000000100);
    PROP_L("ts",               3, 6025000000000003); PROP_IDX(26, 6025000000000200);
    PROP_S("component",        4, 6025000000000004);
    PROP_S("schemaVersion",    5, 6025000000000005);
    PROP_S("payloadJson",      6, 6025000000000006);
    PROP_L("syncClock",        7, 6025000000000007);
    LAST_PROP(7, 6025000000000007);

    obx_model_last_entity_id(m, 25, 6025000000000000);
    obx_model_last_index_id(m,  26, 6025000000000200);

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

// ── Helper: pick entity with highest updatedAt from a vector ─────────────────
template<typename T>
static obx_id latest_id(const std::vector<T>& v) {
    if (v.empty()) return 0;
    auto it = std::max_element(v.begin(), v.end(),
        [](const T& a, const T& b){ return a.updatedAt < b.updatedAt; });
    return it->id;
}

// Remove all entities in v except the one with the given id
template<typename T>
static void remove_stale(obx::Box<T>& box, const std::vector<T>& v, obx_id keep_id) {
    for (const auto& e : v) {
        if (e.id != keep_id) box.remove(e.id);
    }
}

// ── State IDs initialization ──────────────────────────────────────────────────
void init_state_ids(StateIds& ids, obx::Store& store, const std::string& vid) {
    {
        auto box = store.box<PowertrainState>();
        auto r = box.query(PowertrainState_::vehicleId.equals(vid)).build().find();
        ids.powertrain = latest_id(r);
        if (r.size() > 1) { remove_stale(box, r, ids.powertrain); std::cout << "🧹 Pruned stale PowertrainState duplicates\n"; }
    }
    {
        auto box = store.box<BatteryState>();
        auto r = box.query(BatteryState_::vehicleId.equals(vid)).build().find();
        ids.battery = latest_id(r);
        if (r.size() > 1) { remove_stale(box, r, ids.battery); std::cout << "🧹 Pruned stale BatteryState duplicates\n"; }
    }
    {
        auto box = store.box<ChassisState>();
        auto r = box.query(ChassisState_::vehicleId.equals(vid)).build().find();
        ids.chassis = latest_id(r);
        if (r.size() > 1) { remove_stale(box, r, ids.chassis); std::cout << "🧹 Pruned stale ChassisState duplicates\n"; }
    }
    {
        auto box = store.box<CabinState>();
        auto r = box.query(CabinState_::vehicleId.equals(vid)).build().find();
        ids.cabin = latest_id(r);
        if (r.size() > 1) { remove_stale(box, r, ids.cabin); std::cout << "🧹 Pruned stale CabinState duplicates\n"; }
    }
    {
        auto box = store.box<LocationState>();
        auto r = box.query(LocationState_::vehicleId.equals(vid)).build().find();
        ids.location = latest_id(r);
        if (r.size() > 1) { remove_stale(box, r, ids.location); std::cout << "🧹 Pruned stale LocationState duplicates\n"; }
    }
    {
        auto box = store.box<AdasState>();
        auto r = box.query(AdasState_::vehicleId.equals(vid)).build().find();
        ids.adas = latest_id(r);
        if (r.size() > 1) { remove_stale(box, r, ids.adas); std::cout << "🧹 Pruned stale AdasState duplicates\n"; }
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
    auto vm_box   = store->box<VehicleMeta>();
    auto sig_box  = store->box<SignalDefinition>();
    auto pt_box   = store->box<PowertrainState>();
    auto bat_box  = store->box<BatteryState>();
    auto ch_box   = store->box<ChassisState>();
    auto cab_box  = store->box<CabinState>();
    auto loc_box  = store->box<LocationState>();
    auto adas_box = store->box<AdasState>();
    auto pts_box  = store->box<PowertrainSample>();
    auto bats_box = store->box<BatterySample>();
    auto locs_box = store->box<LocationSample>();
    auto cabs_box = store->box<CabinSample>();
    auto adas_s_box=store->box<AdasSample>();
    auto ev_box   = store->box<VehicleEvent>();
    auto ext_box  = store->box<ExtensionPayload>();

    // State ID cache + mutex
    StateIds state_ids;
    std::mutex state_mutex;
    init_state_ids(state_ids, *store, VEHICLE_ID);

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
                // Events retained 7 days
                int64_t event_cutoff = now_ms() - 7LL * 24 * 3600 * 1000;
                ev_box.query(VehicleEvent_::ts.lessThan(event_cutoff)).build().remove();
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

            std::lock_guard<std::mutex> lock(state_mutex);

            // ── PowertrainState + PowertrainSample ────────────────────────
            if (body.contains("powertrain")) {
                const auto& p = body["powertrain"];
                PowertrainState s;
                if (state_ids.powertrain) { auto e = pt_box.get(state_ids.powertrain); if (e) s = *e; }
                s.id          = state_ids.powertrain;
                s.vehicleId   = vid;
                s.updatedAt   = ts;
                s.speedKph    = jf(p,"speedKph");
                s.engineRpm   = jf(p,"engineRpm");
                s.odometerKm  = jf(p,"odometerKm");
                s.fuelLevelPct= jf(p,"fuelLevelPct");
                s.fuelRateLph = jf(p,"fuelRateLph");
                s.coolantTempC= jf(p,"coolantTempC");
                s.throttlePct = jf(p,"throttlePct");
                s.gear        = ji(p,"gear");
                s.ignitionOn  = jb(p,"ignitionOn",true);
                s.tripId      = trip;
                state_ids.powertrain = pt_box.put(s);

                PowertrainSample sample;
                sample.vehicleId   = vid; sample.ts = ts; sample.tripId = trip;
                sample.speedKph    = s.speedKph;   sample.engineRpm   = s.engineRpm;
                sample.fuelLevelPct= s.fuelLevelPct;sample.fuelRateLph = s.fuelRateLph;
                sample.coolantTempC= s.coolantTempC;sample.throttlePct = s.throttlePct;
                sample.gear        = s.gear;
                pts_box.put(sample);
            }

            // ── BatteryState + BatterySample ──────────────────────────────
            if (body.contains("battery")) {
                const auto& p = body["battery"];
                BatteryState s;
                if (state_ids.battery) { auto e = bat_box.get(state_ids.battery); if (e) s = *e; }
                s.id               = state_ids.battery;
                s.vehicleId        = vid;  s.updatedAt = ts;
                s.socPct           = jf(p,"socPct");
                s.sohPct           = jf(p,"sohPct",100.f);
                s.batteryTempC     = jf(p,"batteryTempC");
                s.chargingState    = js(p,"chargingState","not_charging");
                s.chargingPowerKw  = jf(p,"chargingPowerKw");
                s.estimatedRangeKm = jf(p,"estimatedRangeKm");
                s.voltageV         = jf(p,"voltageV");
                s.currentA         = jf(p,"currentA");
                state_ids.battery = bat_box.put(s);

                BatterySample sample;
                sample.vehicleId      = vid; sample.ts = ts; sample.tripId = trip;
                sample.socPct         = s.socPct;   sample.sohPct      = s.sohPct;
                sample.batteryTempC   = s.batteryTempC;
                sample.chargingPowerKw= s.chargingPowerKw;
                sample.estimatedRangeKm=s.estimatedRangeKm;
                sample.voltageV       = s.voltageV; sample.currentA    = s.currentA;
                bats_box.put(sample);
            }

            // ── ChassisState ──────────────────────────────────────────────
            if (body.contains("chassis")) {
                const auto& p = body["chassis"];
                ChassisState s;
                if (state_ids.chassis) { auto e = ch_box.get(state_ids.chassis); if (e) s = *e; }
                s.id                    = state_ids.chassis;
                s.vehicleId             = vid;  s.updatedAt = ts;
                s.steeringAngleDeg      = jf(p,"steeringAngleDeg");
                s.brakePedalPct         = jf(p,"brakePedalPct");
                s.tirePressureFlKpa     = jf(p,"tirePressureFlKpa");
                s.tirePressureFrKpa     = jf(p,"tirePressureFrKpa");
                s.tirePressureRlKpa     = jf(p,"tirePressureRlKpa");
                s.tirePressureRrKpa     = jf(p,"tirePressureRrKpa");
                s.absActive             = jb(p,"absActive");
                s.tractionControlActive = jb(p,"tractionControlActive");
                state_ids.chassis = ch_box.put(s);
            }

            // ── CabinState + CabinSample ──────────────────────────────────
            if (body.contains("cabin")) {
                const auto& p = body["cabin"];
                CabinState s;
                if (state_ids.cabin) { auto e = cab_box.get(state_ids.cabin); if (e) s = *e; }
                s.id                    = state_ids.cabin;
                s.vehicleId             = vid;  s.updatedAt = ts;
                s.insideTempC           = jf(p,"insideTempC");
                s.outsideTempC          = jf(p,"outsideTempC");
                s.hvacMode              = js(p,"hvacMode","auto");
                s.fanSpeed              = ji(p,"fanSpeed");
                s.driverDoorOpen        = jb(p,"driverDoorOpen");
                s.passengerDoorOpen     = jb(p,"passengerDoorOpen");
                s.rearLeftDoorOpen      = jb(p,"rearLeftDoorOpen");
                s.rearRightDoorOpen     = jb(p,"rearRightDoorOpen");
                s.doorsLocked           = jb(p,"doorsLocked",true);
                s.seatbeltDriverFastened= jb(p,"seatbeltDriverFastened");
                state_ids.cabin = cab_box.put(s);

                CabinSample sample;
                sample.vehicleId  = vid; sample.ts = ts; sample.tripId = trip;
                sample.insideTempC= s.insideTempC; sample.outsideTempC = s.outsideTempC;
                sample.hvacMode   = s.hvacMode;    sample.fanSpeed     = s.fanSpeed;
                cabs_box.put(sample);
            }

            // ── LocationState + LocationSample ────────────────────────────
            if (body.contains("location")) {
                const auto& p = body["location"];
                LocationState s;
                if (state_ids.location) { auto e = loc_box.get(state_ids.location); if (e) s = *e; }
                s.id        = state_ids.location;
                s.vehicleId = vid;  s.updatedAt = ts;
                s.latitude  = jd(p,"latitude");
                s.longitude = jd(p,"longitude");
                s.altitudeM = jf(p,"altitudeM");
                s.headingDeg= jf(p,"headingDeg");
                s.speedKph  = jf(p,"speedKph");
                s.accuracyM = jf(p,"accuracyM");
                s.geohash   = js(p,"geohash","");
                state_ids.location = loc_box.put(s);

                LocationSample sample;
                sample.vehicleId = vid; sample.ts = ts; sample.tripId = trip;
                sample.latitude  = s.latitude;  sample.longitude = s.longitude;
                sample.altitudeM = s.altitudeM; sample.headingDeg= s.headingDeg;
                sample.speedKph  = s.speedKph;  sample.accuracyM = s.accuracyM;
                locs_box.put(sample);
            }

            // ── AdasState + AdasSample ────────────────────────────────────
            if (body.contains("adas")) {
                const auto& p = body["adas"];
                AdasState s;
                if (state_ids.adas) { auto e = adas_box.get(state_ids.adas); if (e) s = *e; }
                s.id                    = state_ids.adas;
                s.vehicleId             = vid;  s.updatedAt = ts;
                s.cruiseEnabled         = jb(p,"cruiseEnabled");
                s.cruiseSetSpeedKph     = jf(p,"cruiseSetSpeedKph");
                s.laneKeepAssistOn      = jb(p,"laneKeepAssistOn");
                s.parkingAssistOn       = jb(p,"parkingAssistOn");
                s.collisionWarningActive= jb(p,"collisionWarningActive");
                s.autopilotMode         = js(p,"autopilotMode","none");
                state_ids.adas = adas_box.put(s);

                AdasSample sample;
                sample.vehicleId             = vid; sample.ts = ts; sample.tripId = trip;
                sample.cruiseEnabled         = s.cruiseEnabled;
                sample.cruiseSetSpeedKph     = s.cruiseSetSpeedKph;
                sample.laneKeepAssistOn      = s.laneKeepAssistOn;
                sample.collisionWarningActive= s.collisionWarningActive;
                adas_s_box.put(sample);
            }

            // ── VehicleEvents from snapshot ───────────────────────────────
            if (body.contains("events") && body["events"].is_array()) {
                for (const auto& ev : body["events"]) {
                    VehicleEvent e;
                    e.vehicleId   = vid;  e.ts = ts;  e.tripId = trip;
                    e.eventType   = js(ev,"eventType","info");
                    e.severity    = js(ev,"severity","info");
                    e.vssPath     = js(ev,"vssPath","");
                    e.code        = js(ev,"code","");
                    e.description = js(ev,"description","");
                    e.payloadJson = js(ev,"payloadJson","{}");
                    ev_box.put(e);
                }
            }

            json resp = {{"success", true}, {"ts", ts}, {"vehicle_id", vid}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── POST /vss/event ───────────────────────────────────────────────────────
    svr.Post("/vss/event", [&](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            VehicleEvent e;
            e.vehicleId   = js(body,"vehicle_id", VEHICLE_ID);
            e.ts          = body.value("ts", now_ms());
            e.tripId      = js(body,"trip_id","");
            e.eventType   = js(body,"eventType","info");
            e.severity    = js(body,"severity","info");
            e.vssPath     = js(body,"vssPath","");
            e.code        = js(body,"code","");
            e.description = js(body,"description","");
            e.payloadJson = js(body,"payloadJson","{}");
            obx_id id = ev_box.put(e);
            res.set_content(json{{"success",true},{"id",id}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── POST /vss/meta ────────────────────────────────────────────────────────
    svr.Post("/vss/meta", [&](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string vid = js(body, "vehicle_id", VEHICLE_ID);

            std::lock_guard<std::mutex> lock(state_mutex);

            VehicleMeta vm;
            auto existing = vm_box.query(VehicleMeta_::vehicleId.equals(vid)).build().find();
            if (!existing.empty()) {
                vm = existing[0];
            } else {
                vm.createdAt = now_ms();
            }
            vm.vehicleId       = vid;
            vm.vin             = js(body, "vin", vid);
            vm.oem             = js(body, "oem", "");
            vm.modelName       = js(body, "model", "");
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
            state_ids.vehicle_meta = id;

            std::cout << "✅ VehicleMeta upserted (id=" << id << ")\n";
            res.set_content(json{{"success", true}, {"id", id}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/latest ───────────────────────────────────────────────────────
    svr.Get("/vss/latest", [&](const Request&, Response& res) {
        try {
            json out;
            out["vehicle_id"] = VEHICLE_ID;
            out["ts"]         = now_ms();

            {
                auto ps = pt_box.query(PowertrainState_::vehicleId.equals(VEHICLE_ID)).build().find();
                auto id = latest_id(ps);
                if (id) {
                    auto it = std::find_if(ps.begin(), ps.end(), [id](const auto& e){ return e.id == id; });
                    auto& p = *it;
                    out["powertrain"] = {
                        {"speedKph",p.speedKph},{"engineRpm",p.engineRpm},
                        {"fuelLevelPct",p.fuelLevelPct},{"coolantTempC",p.coolantTempC},
                        {"throttlePct",p.throttlePct},{"gear",p.gear},
                        {"ignitionOn",p.ignitionOn},{"odometerKm",p.odometerKm},
                        {"updatedAt",p.updatedAt}
                    };
                }
            }
            {
                auto bs = bat_box.query(BatteryState_::vehicleId.equals(VEHICLE_ID)).build().find();
                auto id = latest_id(bs);
                if (id) {
                    auto it = std::find_if(bs.begin(), bs.end(), [id](const auto& e){ return e.id == id; });
                    auto& b = *it;
                    out["battery"] = {
                        {"socPct",b.socPct},{"sohPct",b.sohPct},
                        {"voltageV",b.voltageV},{"currentA",b.currentA},
                        {"batteryTempC",b.batteryTempC},{"chargingState",b.chargingState},
                        {"estimatedRangeKm",b.estimatedRangeKm},{"updatedAt",b.updatedAt}
                    };
                }
            }
            {
                auto cs = ch_box.query(ChassisState_::vehicleId.equals(VEHICLE_ID)).build().find();
                auto id = latest_id(cs);
                if (id) {
                    auto it = std::find_if(cs.begin(), cs.end(), [id](const auto& e){ return e.id == id; });
                    auto& c = *it;
                    out["chassis"] = {
                        {"tirePressureFlKpa",c.tirePressureFlKpa},{"tirePressureFrKpa",c.tirePressureFrKpa},
                        {"tirePressureRlKpa",c.tirePressureRlKpa},{"tirePressureRrKpa",c.tirePressureRrKpa},
                        {"steeringAngleDeg",c.steeringAngleDeg},{"brakePedalPct",c.brakePedalPct},
                        {"absActive",c.absActive},{"tractionControlActive",c.tractionControlActive},
                        {"updatedAt",c.updatedAt}
                    };
                }
            }
            {
                auto cab = cab_box.query(CabinState_::vehicleId.equals(VEHICLE_ID)).build().find();
                auto id = latest_id(cab);
                if (id) {
                    auto it = std::find_if(cab.begin(), cab.end(), [id](const auto& e){ return e.id == id; });
                    auto& c = *it;
                    out["cabin"] = {
                        {"insideTempC",c.insideTempC},{"outsideTempC",c.outsideTempC},
                        {"hvacMode",c.hvacMode},{"fanSpeed",c.fanSpeed},
                        {"driverDoorOpen",c.driverDoorOpen},{"doorsLocked",c.doorsLocked},
                        {"seatbeltDriverFastened",c.seatbeltDriverFastened},{"updatedAt",c.updatedAt}
                    };
                }
            }
            {
                auto ls = loc_box.query(LocationState_::vehicleId.equals(VEHICLE_ID)).build().find();
                auto id = latest_id(ls);
                if (id) {
                    auto it = std::find_if(ls.begin(), ls.end(), [id](const auto& e){ return e.id == id; });
                    auto& l = *it;
                    out["location"] = {
                        {"latitude",l.latitude},{"longitude",l.longitude},
                        {"altitudeM",l.altitudeM},{"headingDeg",l.headingDeg},
                        {"speedKph",l.speedKph},{"geohash",l.geohash},{"updatedAt",l.updatedAt}
                    };
                }
            }
            {
                auto as = adas_box.query(AdasState_::vehicleId.equals(VEHICLE_ID)).build().find();
                auto id = latest_id(as);
                if (id) {
                    auto it = std::find_if(as.begin(), as.end(), [id](const auto& e){ return e.id == id; });
                    auto& a = *it;
                    out["adas"] = {
                        {"cruiseEnabled",a.cruiseEnabled},{"cruiseSetSpeedKph",a.cruiseSetSpeedKph},
                        {"laneKeepAssistOn",a.laneKeepAssistOn},{"parkingAssistOn",a.parkingAssistOn},
                        {"collisionWarningActive",a.collisionWarningActive},
                        {"autopilotMode",a.autopilotMode},{"updatedAt",a.updatedAt}
                    };
                }
            }

            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
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
            for (auto& r : rows)
                arr.push_back({{"ts",r.ts},{"speedKph",r.speedKph},{"engineRpm",r.engineRpm},
                    {"fuelLevelPct",r.fuelLevelPct},{"coolantTempC",r.coolantTempC},
                    {"throttlePct",r.throttlePct},{"gear",r.gear}});
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
            for (auto& r : rows)
                arr.push_back({{"ts",r.ts},{"socPct",r.socPct},{"sohPct",r.sohPct},
                    {"voltageV",r.voltageV},{"currentA",r.currentA},
                    {"batteryTempC",r.batteryTempC},{"estimatedRangeKm",r.estimatedRangeKm}});
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
            for (auto& r : rows)
                arr.push_back({{"ts",r.ts},{"latitude",r.latitude},{"longitude",r.longitude},
                    {"headingDeg",r.headingDeg},{"speedKph",r.speedKph},{"altitudeM",r.altitudeM}});
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
            for (auto& r : rows)
                arr.push_back({{"ts",r.ts},{"insideTempC",r.insideTempC},
                    {"outsideTempC",r.outsideTempC},{"hvacMode",r.hvacMode},{"fanSpeed",r.fanSpeed}});
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
            for (auto& r : rows)
                arr.push_back({{"ts",r.ts},{"cruiseEnabled",r.cruiseEnabled},
                    {"laneKeepAssistOn",r.laneKeepAssistOn},
                    {"collisionWarningActive",r.collisionWarningActive}});
            res.set_content(json{{"count",(int)arr.size()},{"samples",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
        }
    });

    // ── GET /vss/events ───────────────────────────────────────────────────────
    svr.Get("/vss/events", [&](const Request& req, Response& res) {
        try {
            int mins = std::stoi(req.get_param_value("minutes").empty() ? "60" : req.get_param_value("minutes"));
            int64_t cutoff = now_ms() - (int64_t)mins * 60 * 1000;
            auto rows = ev_box.query(
                VehicleEvent_::vehicleId.equals(VEHICLE_ID) &&
                VehicleEvent_::ts.greaterOrEq(cutoff))
                .order(VehicleEvent_::ts, OBXOrderFlags_DESCENDING).build().find();
            json arr = json::array();
            for (auto& r : rows)
                arr.push_back({{"ts",r.ts},{"eventType",r.eventType},{"severity",r.severity},
                    {"vssPath",r.vssPath},{"code",r.code},{"description",r.description}});
            res.set_content(json{{"count",(int)arr.size()},{"events",arr}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error",e.what()}}.dump(), "application/json");
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
            {"events",             (int64_t)ev_box.count()}
        }.dump(), "application/json");
    });

    std::cout << "🚀 Listening on port " << cfg.port << "\n\n";
    svr.listen("0.0.0.0", cfg.port);
    return 0;
}
