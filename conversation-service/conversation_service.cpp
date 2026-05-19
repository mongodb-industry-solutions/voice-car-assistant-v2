/**
 * Conversation Service - ObjectBox-based conversation storage with sync
 * 
 * Provides HTTP API for storing and retrieving conversation messages
 * Automatically syncs to MongoDB Atlas via ObjectBox Sync Server
 *
 * Endpoints:
 *   POST /conversations/message - Save a message
 *   GET  /conversations/:id     - Get conversation by ID
 *   GET  /conversations/user/:user_id - Get user's conversations
 *   GET  /health                - Health check
 */

#define OBX_CPP_FILE
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include "objectbox.hpp"
#include "objectbox-sync.hpp"
#include "schema.obx.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace httplib;

// Configuration
struct Config {
    std::string db_path = "/app/conversation-db";
    std::string sync_server_url = "ws://sync-server:9999";
    bool enable_sync = true;
    int port = 8081;
};

// Create ObjectBox model programmatically (must match sync server!)
OBX_model* create_obx_model() {
    OBX_model* model = obx_model();
    if (!model) return nullptr;

    // Entity 1: manual_chunks (required for sync compatibility, not used)
    obx_model_entity(model, "manual_chunks", 1, 2807783899453578393);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 871349036716677797);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "text", OBXPropertyType_String, 2, 6563616578029045320);
    obx_model_property(model, "source_file", OBXPropertyType_String, 3, 8818095693993590927);
    obx_model_property(model, "chunk_index", OBXPropertyType_Int, 4, 6846133054869205678);
    obx_model_property(model, "embedding", OBXPropertyType_FloatVector, 5, 6898708364220688226);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_hnsw_dimensions(model, 1024);
    obx_model_property_index_hnsw_distance_type(model, OBXVectorDistanceType_Cosine);
    obx_model_property_index_id(model, 1, 4357812374228481003);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 6, 1234567890123456789);
    obx_model_entity_last_property_id(model, 6, 1234567890123456789);
    
    // Entity 2: manuals (required for sync compatibility, not used)
    obx_model_entity(model, "manuals", 2, 3456789012345678901);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 2345678901234567890);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "filename", OBXPropertyType_String, 2, 3456789012345678902);
    obx_model_property(model, "make", OBXPropertyType_String, 3, 4567890123456789013);
    obx_model_property(model, "model", OBXPropertyType_String, 4, 5678901234567890124);
    obx_model_property(model, "total_chunks", OBXPropertyType_Int, 5, 6789012345678901235);
    obx_model_property(model, "status", OBXPropertyType_String, 6, 7890123456789012346);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 7, 9876543210987654321);
    obx_model_entity_last_property_id(model, 7, 9876543210987654321);

    // Entity 3: conversations (this is what we actually use)
    obx_model_entity(model, "conversations", 3, 1111222233334444555);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);  // Enable sync for conversations!
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 1111222233334444556);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "conversation_id", OBXPropertyType_String, 2, 2222333344445555666);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 2, 2222222222222222222);
    obx_model_property(model, "user_id", OBXPropertyType_String, 3, 3333444455556666777);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 3, 3333333333333333333);
    obx_model_property(model, "timestamp", OBXPropertyType_Long, 4, 4444555566667777888);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 4, 4444444444444444444);
    obx_model_property(model, "role", OBXPropertyType_String, 5, 5555666677778888999);
    obx_model_property(model, "message", OBXPropertyType_String, 6, 6666777788889999111);
    obx_model_property(model, "sources", OBXPropertyType_String, 7, 7777888899991111222);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 8, 8888999911112222333);
    obx_model_entity_last_property_id(model, 8, 8888999911112222333);
    
    // Entity 4: telemetry_snapshots (required for sync compatibility, not used)
    obx_model_entity(model, "telemetry_snapshots", 4, 2222333344445555777);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 2222333344445555778);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "timestamp", OBXPropertyType_Long, 2, 3333444455556666888);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 5, 5555555555555555555);
    obx_model_property(model, "vehicle_id", OBXPropertyType_String, 3, 4444555566667777999);
    obx_model_property(model, "driving_mode", OBXPropertyType_String, 4, 5555666677778888000);
    obx_model_property(model, "anomaly_count", OBXPropertyType_Int, 5, 6666777788889999222);
    obx_model_property(model, "engine_data", OBXPropertyType_String, 6, 7777888899990000333);
    obx_model_property(model, "tire_data", OBXPropertyType_String, 7, 8888999900001111444);
    obx_model_property(model, "battery_data", OBXPropertyType_String, 8, 9999000011112222555);
    obx_model_property(model, "fuel_data", OBXPropertyType_String, 9, 1111222233334444666);
    obx_model_property(model, "transmission_data", OBXPropertyType_String, 10, 2222333344445555888);
    obx_model_property(model, "brake_data", OBXPropertyType_String, 11, 3333444455556666999);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 12, 4444555566668888111);
    obx_model_entity_last_property_id(model, 12, 4444555566668888111);
    
    // ── VSS Telemetry Entities (10-25) ────────────────────────────────────────
    obx_model_entity(model, "VehicleMeta", 10, 6010000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",              OBXPropertyType_Long,   1,  6010000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",       OBXPropertyType_String, 2,  6010000000000002);
    obx_model_property(model, "vin",             OBXPropertyType_String, 3,  6010000000000003);
    obx_model_property(model, "oem",             OBXPropertyType_String, 4,  6010000000000004);
    obx_model_property(model, "model",           OBXPropertyType_String, 5,  6010000000000005);
    obx_model_property(model, "platform",        OBXPropertyType_String, 6,  6010000000000006);
    obx_model_property(model, "softwareVersion", OBXPropertyType_String, 7,  6010000000000007);
    obx_model_property(model, "createdAt",       OBXPropertyType_Long,   8,  6010000000000008);
    obx_model_property(model, "updatedAt",       OBXPropertyType_Long,   9,  6010000000000009);
    obx_model_property(model, "syncClock",       OBXPropertyType_Long,   10, 6010000000000010);
    obx_model_entity_last_property_id(model, 10, 6010000000000010);

    obx_model_entity(model, "SignalDefinition", 11, 6011000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",             OBXPropertyType_Long,   1,  6011000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vssPath",        OBXPropertyType_String, 2,  6011000000000002);
    obx_model_property(model, "component",      OBXPropertyType_String, 3,  6011000000000003);
    obx_model_property(model, "signalKind",     OBXPropertyType_String, 4,  6011000000000004);
    obx_model_property(model, "valueType",      OBXPropertyType_String, 5,  6011000000000005);
    obx_model_property(model, "unit",           OBXPropertyType_String, 6,  6011000000000006);
    obx_model_property(model, "writable",       OBXPropertyType_Bool,   7,  6011000000000007);
    obx_model_property(model, "latestGroup",    OBXPropertyType_String, 8,  6011000000000008);
    obx_model_property(model, "historyGroup",   OBXPropertyType_String, 9,  6011000000000009);
    obx_model_property(model, "historyMode",    OBXPropertyType_String, 10, 6011000000000010);
    obx_model_property(model, "samplePeriodMs", OBXPropertyType_Int,    11, 6011000000000011);
    obx_model_property(model, "retainHours",    OBXPropertyType_Int,    12, 6011000000000012);
    obx_model_property(model, "enabled",        OBXPropertyType_Bool,   13, 6011000000000013);
    obx_model_property(model, "syncClock",      OBXPropertyType_Long,   14, 6011000000000014);
    obx_model_entity_last_property_id(model, 14, 6011000000000014);

    obx_model_entity(model, "VehicleAttributeState", 12, 6012000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",                 OBXPropertyType_Long,   1,  6012000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",          OBXPropertyType_String, 2,  6012000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 6, 6012000000000100);
    obx_model_property(model, "updatedAt",          OBXPropertyType_Long,   3,  6012000000000003);
    obx_model_property(model, "fuelTankCapacityL",  OBXPropertyType_Float,  4,  6012000000000004);
    obx_model_property(model, "batteryCapacityKwh", OBXPropertyType_Float,  5,  6012000000000005);
    obx_model_property(model, "wheelbaseMm",        OBXPropertyType_Int,    6,  6012000000000006);
    obx_model_property(model, "curbWeightKg",       OBXPropertyType_Int,    7,  6012000000000007);
    obx_model_property(model, "powertrainType",     OBXPropertyType_String, 8,  6012000000000008);
    obx_model_property(model, "drivetrainType",     OBXPropertyType_String, 9,  6012000000000009);
    obx_model_property(model, "syncClock",          OBXPropertyType_Long,   10, 6012000000000010);
    obx_model_entity_last_property_id(model, 10, 6012000000000010);

    obx_model_entity(model, "PowertrainState", 13, 6013000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",           OBXPropertyType_Long,   1,  6013000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",    OBXPropertyType_String, 2,  6013000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 7, 6013000000000100);
    obx_model_property(model, "updatedAt",    OBXPropertyType_Long,   3,  6013000000000003);
    obx_model_property(model, "speedKph",     OBXPropertyType_Float,  4,  6013000000000004);
    obx_model_property(model, "engineRpm",    OBXPropertyType_Float,  5,  6013000000000005);
    obx_model_property(model, "odometerKm",   OBXPropertyType_Float,  6,  6013000000000006);
    obx_model_property(model, "fuelLevelPct", OBXPropertyType_Float,  7,  6013000000000007);
    obx_model_property(model, "fuelRateLph",  OBXPropertyType_Float,  8,  6013000000000008);
    obx_model_property(model, "coolantTempC", OBXPropertyType_Float,  9,  6013000000000009);
    obx_model_property(model, "throttlePct",  OBXPropertyType_Float,  10, 6013000000000010);
    obx_model_property(model, "gear",         OBXPropertyType_Int,    11, 6013000000000011);
    obx_model_property(model, "ignitionOn",   OBXPropertyType_Bool,   12, 6013000000000012);
    obx_model_property(model, "tripId",       OBXPropertyType_String, 13, 6013000000000013);
    obx_model_property(model, "syncClock",    OBXPropertyType_Long,   14, 6013000000000014);
    obx_model_entity_last_property_id(model, 14, 6013000000000014);

    obx_model_entity(model, "BatteryState", 14, 6014000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",               OBXPropertyType_Long,   1,  6014000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",        OBXPropertyType_String, 2,  6014000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 8, 6014000000000100);
    obx_model_property(model, "updatedAt",        OBXPropertyType_Long,   3,  6014000000000003);
    obx_model_property(model, "socPct",           OBXPropertyType_Float,  4,  6014000000000004);
    obx_model_property(model, "sohPct",           OBXPropertyType_Float,  5,  6014000000000005);
    obx_model_property(model, "batteryTempC",     OBXPropertyType_Float,  6,  6014000000000006);
    obx_model_property(model, "chargingState",    OBXPropertyType_String, 7,  6014000000000007);
    obx_model_property(model, "chargingPowerKw",  OBXPropertyType_Float,  8,  6014000000000008);
    obx_model_property(model, "estimatedRangeKm", OBXPropertyType_Float,  9,  6014000000000009);
    obx_model_property(model, "voltageV",         OBXPropertyType_Float,  10, 6014000000000010);
    obx_model_property(model, "currentA",         OBXPropertyType_Float,  11, 6014000000000011);
    obx_model_property(model, "syncClock",        OBXPropertyType_Long,   12, 6014000000000012);
    obx_model_entity_last_property_id(model, 12, 6014000000000012);

    obx_model_entity(model, "ChassisState", 15, 6015000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",                    OBXPropertyType_Long,   1,  6015000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",             OBXPropertyType_String, 2,  6015000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 9, 6015000000000100);
    obx_model_property(model, "updatedAt",             OBXPropertyType_Long,   3,  6015000000000003);
    obx_model_property(model, "steeringAngleDeg",      OBXPropertyType_Float,  4,  6015000000000004);
    obx_model_property(model, "brakePedalPct",         OBXPropertyType_Float,  5,  6015000000000005);
    obx_model_property(model, "tirePressureFlKpa",     OBXPropertyType_Float,  6,  6015000000000006);
    obx_model_property(model, "tirePressureFrKpa",     OBXPropertyType_Float,  7,  6015000000000007);
    obx_model_property(model, "tirePressureRlKpa",     OBXPropertyType_Float,  8,  6015000000000008);
    obx_model_property(model, "tirePressureRrKpa",     OBXPropertyType_Float,  9,  6015000000000009);
    obx_model_property(model, "absActive",             OBXPropertyType_Bool,   10, 6015000000000010);
    obx_model_property(model, "tractionControlActive", OBXPropertyType_Bool,   11, 6015000000000011);
    obx_model_property(model, "syncClock",             OBXPropertyType_Long,   12, 6015000000000012);
    obx_model_entity_last_property_id(model, 12, 6015000000000012);

    obx_model_entity(model, "CabinState", 16, 6016000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",                     OBXPropertyType_Long,   1,  6016000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",              OBXPropertyType_String, 2,  6016000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 10, 6016000000000100);
    obx_model_property(model, "updatedAt",              OBXPropertyType_Long,   3,  6016000000000003);
    obx_model_property(model, "insideTempC",            OBXPropertyType_Float,  4,  6016000000000004);
    obx_model_property(model, "outsideTempC",           OBXPropertyType_Float,  5,  6016000000000005);
    obx_model_property(model, "hvacMode",               OBXPropertyType_String, 6,  6016000000000006);
    obx_model_property(model, "fanSpeed",               OBXPropertyType_Int,    7,  6016000000000007);
    obx_model_property(model, "driverDoorOpen",         OBXPropertyType_Bool,   8,  6016000000000008);
    obx_model_property(model, "passengerDoorOpen",      OBXPropertyType_Bool,   9,  6016000000000009);
    obx_model_property(model, "rearLeftDoorOpen",       OBXPropertyType_Bool,   10, 6016000000000010);
    obx_model_property(model, "rearRightDoorOpen",      OBXPropertyType_Bool,   11, 6016000000000011);
    obx_model_property(model, "doorsLocked",            OBXPropertyType_Bool,   12, 6016000000000012);
    obx_model_property(model, "seatbeltDriverFastened", OBXPropertyType_Bool,   13, 6016000000000013);
    obx_model_property(model, "syncClock",              OBXPropertyType_Long,   14, 6016000000000014);
    obx_model_entity_last_property_id(model, 14, 6016000000000014);

    obx_model_entity(model, "LocationState", 17, 6017000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",         OBXPropertyType_Long,   1,  6017000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",  OBXPropertyType_String, 2,  6017000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 11, 6017000000000100);
    obx_model_property(model, "updatedAt",  OBXPropertyType_Long,   3,  6017000000000003);
    obx_model_property(model, "latitude",   OBXPropertyType_Double, 4,  6017000000000004);
    obx_model_property(model, "longitude",  OBXPropertyType_Double, 5,  6017000000000005);
    obx_model_property(model, "altitudeM",  OBXPropertyType_Float,  6,  6017000000000006);
    obx_model_property(model, "headingDeg", OBXPropertyType_Float,  7,  6017000000000007);
    obx_model_property(model, "speedKph",   OBXPropertyType_Float,  8,  6017000000000008);
    obx_model_property(model, "accuracyM",  OBXPropertyType_Float,  9,  6017000000000009);
    obx_model_property(model, "geohash",    OBXPropertyType_String, 10, 6017000000000010);
    obx_model_property(model, "syncClock",  OBXPropertyType_Long,   11, 6017000000000011);
    obx_model_entity_last_property_id(model, 11, 6017000000000011);

    obx_model_entity(model, "AdasState", 18, 6018000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",                     OBXPropertyType_Long,   1,  6018000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",              OBXPropertyType_String, 2,  6018000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 12, 6018000000000100);
    obx_model_property(model, "updatedAt",              OBXPropertyType_Long,   3,  6018000000000003);
    obx_model_property(model, "cruiseEnabled",          OBXPropertyType_Bool,   4,  6018000000000004);
    obx_model_property(model, "cruiseSetSpeedKph",      OBXPropertyType_Float,  5,  6018000000000005);
    obx_model_property(model, "laneKeepAssistOn",       OBXPropertyType_Bool,   6,  6018000000000006);
    obx_model_property(model, "parkingAssistOn",        OBXPropertyType_Bool,   7,  6018000000000007);
    obx_model_property(model, "collisionWarningActive", OBXPropertyType_Bool,   8,  6018000000000008);
    obx_model_property(model, "autopilotMode",          OBXPropertyType_String, 9,  6018000000000009);
    obx_model_property(model, "syncClock",              OBXPropertyType_Long,   10, 6018000000000010);
    obx_model_entity_last_property_id(model, 10, 6018000000000010);

    obx_model_entity(model, "PowertrainSample", 19, 6019000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",           OBXPropertyType_Long,   1,  6019000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",    OBXPropertyType_String, 2,  6019000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 13, 6019000000000100);
    obx_model_property(model, "ts",           OBXPropertyType_Long,   3,  6019000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 14, 6019000000000200);
    obx_model_property(model, "tripId",       OBXPropertyType_String, 4,  6019000000000004);
    obx_model_property(model, "speedKph",     OBXPropertyType_Float,  5,  6019000000000005);
    obx_model_property(model, "engineRpm",    OBXPropertyType_Float,  6,  6019000000000006);
    obx_model_property(model, "fuelLevelPct", OBXPropertyType_Float,  7,  6019000000000007);
    obx_model_property(model, "fuelRateLph",  OBXPropertyType_Float,  8,  6019000000000008);
    obx_model_property(model, "coolantTempC", OBXPropertyType_Float,  9,  6019000000000009);
    obx_model_property(model, "throttlePct",  OBXPropertyType_Float,  10, 6019000000000010);
    obx_model_property(model, "gear",         OBXPropertyType_Int,    11, 6019000000000011);
    obx_model_property(model, "syncClock",    OBXPropertyType_Long,   12, 6019000000000012);
    obx_model_entity_last_property_id(model, 12, 6019000000000012);

    obx_model_entity(model, "BatterySample", 20, 6020000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",               OBXPropertyType_Long,   1,  6020000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",        OBXPropertyType_String, 2,  6020000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 15, 6020000000000100);
    obx_model_property(model, "ts",               OBXPropertyType_Long,   3,  6020000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 16, 6020000000000200);
    obx_model_property(model, "tripId",           OBXPropertyType_String, 4,  6020000000000004);
    obx_model_property(model, "socPct",           OBXPropertyType_Float,  5,  6020000000000005);
    obx_model_property(model, "sohPct",           OBXPropertyType_Float,  6,  6020000000000006);
    obx_model_property(model, "batteryTempC",     OBXPropertyType_Float,  7,  6020000000000007);
    obx_model_property(model, "chargingPowerKw",  OBXPropertyType_Float,  8,  6020000000000008);
    obx_model_property(model, "estimatedRangeKm", OBXPropertyType_Float,  9,  6020000000000009);
    obx_model_property(model, "voltageV",         OBXPropertyType_Float,  10, 6020000000000010);
    obx_model_property(model, "currentA",         OBXPropertyType_Float,  11, 6020000000000011);
    obx_model_property(model, "syncClock",        OBXPropertyType_Long,   12, 6020000000000012);
    obx_model_entity_last_property_id(model, 12, 6020000000000012);

    obx_model_entity(model, "LocationSample", 21, 6021000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",         OBXPropertyType_Long,   1,  6021000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",  OBXPropertyType_String, 2,  6021000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 17, 6021000000000100);
    obx_model_property(model, "ts",         OBXPropertyType_Long,   3,  6021000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 18, 6021000000000200);
    obx_model_property(model, "tripId",     OBXPropertyType_String, 4,  6021000000000004);
    obx_model_property(model, "latitude",   OBXPropertyType_Double, 5,  6021000000000005);
    obx_model_property(model, "longitude",  OBXPropertyType_Double, 6,  6021000000000006);
    obx_model_property(model, "altitudeM",  OBXPropertyType_Float,  7,  6021000000000007);
    obx_model_property(model, "headingDeg", OBXPropertyType_Float,  8,  6021000000000008);
    obx_model_property(model, "speedKph",   OBXPropertyType_Float,  9,  6021000000000009);
    obx_model_property(model, "accuracyM",  OBXPropertyType_Float,  10, 6021000000000010);
    obx_model_property(model, "syncClock",  OBXPropertyType_Long,   11, 6021000000000011);
    obx_model_entity_last_property_id(model, 11, 6021000000000011);

    obx_model_entity(model, "CabinSample", 22, 6022000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",           OBXPropertyType_Long,   1, 6022000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",    OBXPropertyType_String, 2, 6022000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 19, 6022000000000100);
    obx_model_property(model, "ts",           OBXPropertyType_Long,   3, 6022000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 20, 6022000000000200);
    obx_model_property(model, "tripId",       OBXPropertyType_String, 4, 6022000000000004);
    obx_model_property(model, "insideTempC",  OBXPropertyType_Float,  5, 6022000000000005);
    obx_model_property(model, "outsideTempC", OBXPropertyType_Float,  6, 6022000000000006);
    obx_model_property(model, "hvacMode",     OBXPropertyType_String, 7, 6022000000000007);
    obx_model_property(model, "fanSpeed",     OBXPropertyType_Int,    8, 6022000000000008);
    obx_model_property(model, "syncClock",    OBXPropertyType_Long,   9, 6022000000000009);
    obx_model_entity_last_property_id(model, 9, 6022000000000009);

    obx_model_entity(model, "AdasSample", 23, 6023000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",                     OBXPropertyType_Long,   1, 6023000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",              OBXPropertyType_String, 2, 6023000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 21, 6023000000000100);
    obx_model_property(model, "ts",                     OBXPropertyType_Long,   3, 6023000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 22, 6023000000000200);
    obx_model_property(model, "tripId",                 OBXPropertyType_String, 4, 6023000000000004);
    obx_model_property(model, "cruiseEnabled",          OBXPropertyType_Bool,   5, 6023000000000005);
    obx_model_property(model, "cruiseSetSpeedKph",      OBXPropertyType_Float,  6, 6023000000000006);
    obx_model_property(model, "laneKeepAssistOn",       OBXPropertyType_Bool,   7, 6023000000000007);
    obx_model_property(model, "collisionWarningActive", OBXPropertyType_Bool,   8, 6023000000000008);
    obx_model_property(model, "syncClock",              OBXPropertyType_Long,   9, 6023000000000009);
    obx_model_entity_last_property_id(model, 9, 6023000000000009);

    obx_model_entity(model, "VehicleEvent", 24, 6024000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",          OBXPropertyType_Long,   1,  6024000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",   OBXPropertyType_String, 2,  6024000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 23, 6024000000000100);
    obx_model_property(model, "ts",          OBXPropertyType_Long,   3,  6024000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 24, 6024000000000200);
    obx_model_property(model, "tripId",      OBXPropertyType_String, 4,  6024000000000004);
    obx_model_property(model, "eventType",   OBXPropertyType_String, 5,  6024000000000005);
    obx_model_property(model, "severity",    OBXPropertyType_String, 6,  6024000000000006);
    obx_model_property(model, "vssPath",     OBXPropertyType_String, 7,  6024000000000007);
    obx_model_property(model, "code",        OBXPropertyType_String, 8,  6024000000000008);
    obx_model_property(model, "description", OBXPropertyType_String, 9,  6024000000000009);
    obx_model_property(model, "payloadJson", OBXPropertyType_String, 10, 6024000000000010);
    obx_model_property(model, "syncClock",   OBXPropertyType_Long,   11, 6024000000000011);
    obx_model_entity_last_property_id(model, 11, 6024000000000011);

    obx_model_entity(model, "ExtensionPayload", 25, 6025000000000000);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id",            OBXPropertyType_Long,   1, 6025000000000001);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId",     OBXPropertyType_String, 2, 6025000000000002);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 25, 6025000000000100);
    obx_model_property(model, "ts",            OBXPropertyType_Long,   3, 6025000000000003);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 26, 6025000000000200);
    obx_model_property(model, "component",     OBXPropertyType_String, 4, 6025000000000004);
    obx_model_property(model, "schemaVersion", OBXPropertyType_String, 5, 6025000000000005);
    obx_model_property(model, "payloadJson",   OBXPropertyType_String, 6, 6025000000000006);
    obx_model_property(model, "syncClock",     OBXPropertyType_Long,   7, 6025000000000007);
    obx_model_entity_last_property_id(model, 7, 6025000000000007);

    obx_model_last_entity_id(model, 25, 6025000000000000);
    obx_model_last_index_id(model, 26, 6025000000000200);
    
    return model;
}

// Initialize ObjectBox store
std::shared_ptr<obx::Store> init_store(const Config& config) {
    OBX_model* model = create_obx_model();
    if (!model) {
        std::cerr << "Failed to create model" << std::endl;
        return nullptr;
    }

    try {
        obx::Options options(model);
        options.directory(config.db_path.c_str());
        return std::make_shared<obx::Store>(options);
    } catch (const std::exception& e) {
        std::cerr << "Failed to open store: " << e.what() << std::endl;
        return nullptr;
    }
}

// Get current timestamp in milliseconds
int64_t current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Main HTTP server
int main(int argc, char* argv[]) {
    // Disable stdout buffering for immediate log output
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    std::cout << "=============================================================\n";
    std::cout << "💬 Conversation Service - ObjectBox + Sync\n";
    std::cout << "=============================================================\n\n";
    std::cout << "🔧 Logs are unbuffered for immediate output\n\n";

    // Parse configuration
    Config config;
    if (argc > 1) config.db_path = argv[1];
    if (argc > 2) config.sync_server_url = argv[2];
    if (argc > 3) config.enable_sync = (std::string(argv[3]) == "true");

    std::cout << "📂 Database: " << config.db_path << "\n";
    std::cout << "🔄 Sync URL: " << config.sync_server_url << "\n";
    std::cout << "🌐 HTTP Port: " << config.port << "\n\n";

    // Initialize ObjectBox
    std::shared_ptr<obx::Store> store = init_store(config);
    if (!store) {
        std::cerr << "❌ Failed to initialize store\n";
        return 1;
    }

    auto box = store->box<Conversation>();
    std::cout << "✅ ObjectBox store initialized\n";
    std::cout << "📊 Current conversations: " << box.count() << "\n";
    
    // Check sync availability
    bool isSyncAvailable = obx::Sync::isAvailable();
    std::cout << "🔍 ObjectBox Sync is " << (isSyncAvailable ? "available" : "unavailable") << "\n\n";

    // Initialize sync - MUST KEEP ALIVE for entire program lifetime!
    std::shared_ptr<obx::SyncClient> syncClient;
    if (config.enable_sync && !config.sync_server_url.empty() && isSyncAvailable) {
        std::cout << "🔄 Connecting to sync server: " << config.sync_server_url << "\n";
        
        try {
            // Create sync client and KEEP IT ALIVE (critical for sync to work!)
            syncClient = obx::Sync::client(*store, config.sync_server_url, obx::SyncCredentials::none());
            syncClient->start();
            
            // Wait for connection to establish
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            std::cout << "✅ Sync client started and running\n\n";
        } catch (const std::exception& e) {
            std::cerr << "⚠️ Sync start failed: " << e.what() << "\n\n";
        }
    }

    // HTTP server
    Server svr;

    // CORS headers
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // OPTIONS handler for CORS
    svr.Options("/(.*)", [](const Request&, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.status = 204;
    });

    // POST /conversations/message - Save a message
    svr.Post("/conversations/message", [&box](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            
            Conversation conv;
            conv.id = 0;  // Auto-generated
            conv.conversation_id = body["conversation_id"].get<std::string>();
            conv.user_id = body["user_id"].get<std::string>();
            conv.timestamp = current_timestamp_ms();
            conv.role = body["role"].get<std::string>();
            conv.message = body["message"].get<std::string>();
            conv.sources = body.value("sources", "");
            conv.syncClock = 0;  // Managed by sync

            obx_id id = box.put(conv);
            
            json response = {
                {"success", true},
                {"id", id},
                {"timestamp", conv.timestamp}
            };
            
            res.set_content(response.dump(), "application/json");
            std::cout << "💾 Saved message: " << conv.role << " in " << conv.conversation_id << "\n";
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /conversations/:conversation_id - Get full conversation
    svr.Get(R"(/conversations/([^/]+))", [&box](const Request& req, Response& res) {
        try {
            std::string conversation_id = req.matches[1];
            
            auto query = box.query(Conversation_::conversation_id.equals(conversation_id, false))
                            .order(Conversation_::timestamp)
                            .build();
            auto results = query.find();
            
            json messages = json::array();
            for (const auto& conv : results) {
                messages.push_back({
                    {"id", conv.id},
                    {"role", conv.role},
                    {"message", conv.message},
                    {"sources", conv.sources},
                    {"timestamp", conv.timestamp}
                });
            }
            
            json response = {
                {"conversation_id", conversation_id},
                {"count", messages.size()},
                {"messages", messages}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /conversations/user/:user_id - Get user's conversations
    svr.Get(R"(/conversations/user/([^/]+))", [&box](const Request& req, Response& res) {
        try {
            std::string user_id = req.matches[1];
            
            auto query = box.query(Conversation_::user_id.equals(user_id, false))
                            .order(Conversation_::timestamp)
                            .build();
            auto results = query.find();
            
            json messages = json::array();
            for (const auto& conv : results) {
                messages.push_back({
                    {"id", conv.id},
                    {"conversation_id", conv.conversation_id},
                    {"role", conv.role},
                    {"message", conv.message},
                    {"timestamp", conv.timestamp}
                });
            }
            
            json response = {
                {"user_id", user_id},
                {"count", messages.size()},
                {"messages", messages}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /health - Health check
    svr.Get("/health", [&box](const Request&, Response& res) {
        json health = {
            {"status", "healthy"},
            {"message_count", box.count()},
            {"service", "conversation-service"}
        };
        res.set_content(health.dump(), "application/json");
    });

    // Start server
    std::cout << "🚀 Starting HTTP server on port " << config.port << "...\n\n";
    if (!svr.listen("0.0.0.0", config.port)) {
        std::cerr << "❌ Failed to start server\n";
        return 1;
    }

    return 0;
}
