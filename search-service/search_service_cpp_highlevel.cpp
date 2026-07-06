/**
 * ObjectBox Search Service with Sync - C++ High-Level API
 * Uses manually generated schema files for ManualChunk and Manual entities
 * Builds ObjectBox from source to access sync functionality
 */

// CRITICAL: Define this BEFORE including objectbox headers to materialize C++ implementations
#define OBX_CPP_FILE

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <chrono>

// ObjectBox C++ High-Level API (built from source)
#include "objectbox.hpp"
#include "objectbox-sync.hpp"
#include "schema.obx.hpp"

// HTTP server (no SSL needed for localhost)
#include "httplib.h"

// JSON
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Global ObjectBox store and sync client
std::shared_ptr<obx::Store> store;
std::unique_ptr<obx::SyncClient> syncClient;

struct Config {
    std::string db_path;
    std::string sync_url;
    bool enable_sync;
    int port;
};

// Create ObjectBox model programmatically for all 4 entities (for sync compatibility)
OBX_model* create_obx_model() {
    OBX_model* model = obx_model();

    // Model generated from sync-server-setup/objectbox-model.json (authoritative).
    // ALL sync clients must present this identical model or the Sync Server rejects
    // them with UNSUPPORTED_DATA_MODEL (57). Entities this service does not use are
    // still declared for schema compatibility. Regenerate if the shared model changes.

    // Entity 1: manual_chunks
    obx_model_entity(model, "manual_chunks", 1, 2807783899453578393ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 871349036716677797ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "text", OBXPropertyType_String, 2, 6563616578029045320ULL);
    obx_model_property(model, "source_file", OBXPropertyType_String, 3, 8818095693993590927ULL);
    obx_model_property(model, "chunk_index", OBXPropertyType_Int, 4, 6846133054869205678ULL);
    obx_model_property(model, "embedding", OBXPropertyType_FloatVector, 5, 6898708364220688226ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_hnsw_dimensions(model, 1024);
    obx_model_property_index_hnsw_distance_type(model, OBXVectorDistanceType_Cosine);
    obx_model_property_index_id(model, 1, 4357812374228481003ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 6, 1234567890123456789ULL);
    obx_model_entity_last_property_id(model, 6, 1234567890123456789ULL);

    // Entity 2: manuals
    obx_model_entity(model, "manuals", 2, 3456789012345678901ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 2345678901234567890ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "filename", OBXPropertyType_String, 2, 3456789012345678902ULL);
    obx_model_property(model, "make", OBXPropertyType_String, 3, 4567890123456789013ULL);
    obx_model_property(model, "model", OBXPropertyType_String, 4, 5678901234567890124ULL);
    obx_model_property(model, "total_chunks", OBXPropertyType_Int, 5, 6789012345678901235ULL);
    obx_model_property(model, "status", OBXPropertyType_String, 6, 7890123456789012346ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 7, 9876543210987654321ULL);
    obx_model_entity_last_property_id(model, 7, 9876543210987654321ULL);

    // Entity 3: conversations
    obx_model_entity(model, "conversations", 3, 1111222233334444555ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 1111222233334444556ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "conversation_id", OBXPropertyType_String, 2, 2222333344445555666ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 2, 2222222222222222222ULL);
    obx_model_property(model, "user_id", OBXPropertyType_String, 3, 3333444455556666777ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 3, 3333333333333333333ULL);
    obx_model_property(model, "timestamp", OBXPropertyType_Long, 4, 4444555566667777888ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 4, 4444444444444444444ULL);
    obx_model_property(model, "role", OBXPropertyType_String, 5, 5555666677778888999ULL);
    obx_model_property(model, "message", OBXPropertyType_String, 6, 6666777788889999111ULL);
    obx_model_property(model, "sources", OBXPropertyType_String, 7, 7777888899991111222ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 8, 8888999911112222333ULL);
    obx_model_entity_last_property_id(model, 8, 8888999911112222333ULL);

    // Entity 4: telemetry_snapshots
    obx_model_entity(model, "telemetry_snapshots", 4, 2222333344445555777ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 2222333344445555778ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "timestamp", OBXPropertyType_Long, 2, 3333444455556666888ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 5, 5555555555555555555ULL);
    obx_model_property(model, "vehicle_id", OBXPropertyType_String, 3, 4444555566667777999ULL);
    obx_model_property(model, "driving_mode", OBXPropertyType_String, 4, 5555666677778888000ULL);
    obx_model_property(model, "anomaly_count", OBXPropertyType_Int, 5, 6666777788889999222ULL);
    obx_model_property(model, "engine_data", OBXPropertyType_String, 6, 7777888899990000333ULL);
    obx_model_property(model, "tire_data", OBXPropertyType_String, 7, 8888999900001111444ULL);
    obx_model_property(model, "battery_data", OBXPropertyType_String, 8, 9999000011112222555ULL);
    obx_model_property(model, "fuel_data", OBXPropertyType_String, 9, 1111222233334444666ULL);
    obx_model_property(model, "transmission_data", OBXPropertyType_String, 10, 2222333344445555888ULL);
    obx_model_property(model, "brake_data", OBXPropertyType_String, 11, 3333444455556666999ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 12, 4444555566668888111ULL);
    obx_model_entity_last_property_id(model, 12, 4444555566668888111ULL);

    // Entity 10: VehicleMeta
    obx_model_entity(model, "VehicleMeta", 10, 6010000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6010000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6010000000000002ULL);
    obx_model_property(model, "vin", OBXPropertyType_String, 3, 6010000000000003ULL);
    obx_model_property(model, "oem", OBXPropertyType_String, 4, 6010000000000004ULL);
    obx_model_property(model, "model", OBXPropertyType_String, 5, 6010000000000005ULL);
    obx_model_property(model, "platform", OBXPropertyType_String, 6, 6010000000000006ULL);
    obx_model_property(model, "softwareVersion", OBXPropertyType_String, 7, 6010000000000007ULL);
    obx_model_property(model, "createdAt", OBXPropertyType_Long, 8, 6010000000000008ULL);
    obx_model_property(model, "updatedAt", OBXPropertyType_Long, 9, 6010000000000009ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 10, 6010000000000010ULL);
    obx_model_property(model, "fuelTankCapacityL", OBXPropertyType_Float, 11, 6010000000000011ULL);
    obx_model_property(model, "batteryCapacityKwh", OBXPropertyType_Float, 12, 6010000000000012ULL);
    obx_model_property(model, "wheelbaseMm", OBXPropertyType_Int, 13, 6010000000000013ULL);
    obx_model_property(model, "curbWeightKg", OBXPropertyType_Int, 14, 6010000000000014ULL);
    obx_model_property(model, "powertrainType", OBXPropertyType_String, 15, 6010000000000015ULL);
    obx_model_property(model, "drivetrainType", OBXPropertyType_String, 16, 6010000000000016ULL);
    obx_model_entity_last_property_id(model, 16, 6010000000000016ULL);

    // Entity 11: SignalDefinition
    obx_model_entity(model, "SignalDefinition", 11, 6011000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6011000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vssPath", OBXPropertyType_String, 2, 6011000000000002ULL);
    obx_model_property(model, "component", OBXPropertyType_String, 3, 6011000000000003ULL);
    obx_model_property(model, "signalKind", OBXPropertyType_String, 4, 6011000000000004ULL);
    obx_model_property(model, "valueType", OBXPropertyType_String, 5, 6011000000000005ULL);
    obx_model_property(model, "unit", OBXPropertyType_String, 6, 6011000000000006ULL);
    obx_model_property(model, "writable", OBXPropertyType_Bool, 7, 6011000000000007ULL);
    obx_model_property(model, "latestGroup", OBXPropertyType_String, 8, 6011000000000008ULL);
    obx_model_property(model, "historyGroup", OBXPropertyType_String, 9, 6011000000000009ULL);
    obx_model_property(model, "historyMode", OBXPropertyType_String, 10, 6011000000000010ULL);
    obx_model_property(model, "samplePeriodMs", OBXPropertyType_Int, 11, 6011000000000011ULL);
    obx_model_property(model, "retainHours", OBXPropertyType_Int, 12, 6011000000000012ULL);
    obx_model_property(model, "enabled", OBXPropertyType_Bool, 13, 6011000000000013ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 14, 6011000000000014ULL);
    obx_model_entity_last_property_id(model, 14, 6011000000000014ULL);

    // Entity 19: PowertrainSample
    obx_model_entity(model, "PowertrainSample", 19, 6019000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6019000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6019000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 13, 6019000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6019000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 14, 6019000000000200ULL);
    obx_model_property(model, "tripId", OBXPropertyType_String, 4, 6019000000000004ULL);
    obx_model_property(model, "speedKph", OBXPropertyType_Float, 5, 6019000000000005ULL);
    obx_model_property(model, "engineRpm", OBXPropertyType_Float, 6, 6019000000000006ULL);
    obx_model_property(model, "fuelLevelPct", OBXPropertyType_Float, 7, 6019000000000007ULL);
    obx_model_property(model, "fuelRateLph", OBXPropertyType_Float, 8, 6019000000000008ULL);
    obx_model_property(model, "coolantTempC", OBXPropertyType_Float, 9, 6019000000000009ULL);
    obx_model_property(model, "throttlePct", OBXPropertyType_Float, 10, 6019000000000010ULL);
    obx_model_property(model, "gear", OBXPropertyType_Int, 11, 6019000000000011ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 12, 6019000000000012ULL);
    obx_model_entity_last_property_id(model, 12, 6019000000000012ULL);

    // Entity 20: BatterySample
    obx_model_entity(model, "BatterySample", 20, 6020000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6020000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6020000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 15, 6020000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6020000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 16, 6020000000000200ULL);
    obx_model_property(model, "tripId", OBXPropertyType_String, 4, 6020000000000004ULL);
    obx_model_property(model, "socPct", OBXPropertyType_Float, 5, 6020000000000005ULL);
    obx_model_property(model, "sohPct", OBXPropertyType_Float, 6, 6020000000000006ULL);
    obx_model_property(model, "batteryTempC", OBXPropertyType_Float, 7, 6020000000000007ULL);
    obx_model_property(model, "chargingPowerKw", OBXPropertyType_Float, 8, 6020000000000008ULL);
    obx_model_property(model, "estimatedRangeKm", OBXPropertyType_Float, 9, 6020000000000009ULL);
    obx_model_property(model, "voltageV", OBXPropertyType_Float, 10, 6020000000000010ULL);
    obx_model_property(model, "currentA", OBXPropertyType_Float, 11, 6020000000000011ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 12, 6020000000000012ULL);
    obx_model_entity_last_property_id(model, 12, 6020000000000012ULL);

    // Entity 21: LocationSample
    obx_model_entity(model, "LocationSample", 21, 6021000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6021000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6021000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 17, 6021000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6021000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 18, 6021000000000200ULL);
    obx_model_property(model, "tripId", OBXPropertyType_String, 4, 6021000000000004ULL);
    obx_model_property(model, "altitudeM", OBXPropertyType_Float, 7, 6021000000000007ULL);
    obx_model_property(model, "headingDeg", OBXPropertyType_Float, 8, 6021000000000008ULL);
    obx_model_property(model, "speedKph", OBXPropertyType_Float, 9, 6021000000000009ULL);
    obx_model_property(model, "accuracyM", OBXPropertyType_Float, 10, 6021000000000010ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 11, 6021000000000011ULL);
    obx_model_property(model, "locationGeoJson", OBXPropertyType_String, 12, 6021000000000012ULL);
    obx_model_entity_last_property_id(model, 12, 6021000000000012ULL);

    // Entity 22: CabinSample
    obx_model_entity(model, "CabinSample", 22, 6022000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6022000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6022000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 19, 6022000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6022000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 20, 6022000000000200ULL);
    obx_model_property(model, "tripId", OBXPropertyType_String, 4, 6022000000000004ULL);
    obx_model_property(model, "insideTempC", OBXPropertyType_Float, 5, 6022000000000005ULL);
    obx_model_property(model, "outsideTempC", OBXPropertyType_Float, 6, 6022000000000006ULL);
    obx_model_property(model, "hvacMode", OBXPropertyType_String, 7, 6022000000000007ULL);
    obx_model_property(model, "fanSpeed", OBXPropertyType_Int, 8, 6022000000000008ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 9, 6022000000000009ULL);
    obx_model_entity_last_property_id(model, 9, 6022000000000009ULL);

    // Entity 23: AdasSample
    obx_model_entity(model, "AdasSample", 23, 6023000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6023000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6023000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 21, 6023000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6023000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 22, 6023000000000200ULL);
    obx_model_property(model, "tripId", OBXPropertyType_String, 4, 6023000000000004ULL);
    obx_model_property(model, "cruiseEnabled", OBXPropertyType_Bool, 5, 6023000000000005ULL);
    obx_model_property(model, "cruiseSetSpeedKph", OBXPropertyType_Float, 6, 6023000000000006ULL);
    obx_model_property(model, "laneKeepAssistOn", OBXPropertyType_Bool, 7, 6023000000000007ULL);
    obx_model_property(model, "collisionWarningActive", OBXPropertyType_Bool, 8, 6023000000000008ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 9, 6023000000000009ULL);
    obx_model_entity_last_property_id(model, 9, 6023000000000009ULL);

    // Entity 24: ChassisSample
    obx_model_entity(model, "ChassisSample", 24, 6026000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6026000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6026000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 23, 6026000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6026000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 24, 6026000000000200ULL);
    obx_model_property(model, "tripId", OBXPropertyType_String, 4, 6026000000000004ULL);
    obx_model_property(model, "steeringAngleDeg", OBXPropertyType_Float, 5, 6026000000000005ULL);
    obx_model_property(model, "brakePedalPct", OBXPropertyType_Float, 6, 6026000000000006ULL);
    obx_model_property(model, "tirePressureFlKpa", OBXPropertyType_Float, 7, 6026000000000007ULL);
    obx_model_property(model, "tirePressureFrKpa", OBXPropertyType_Float, 8, 6026000000000008ULL);
    obx_model_property(model, "tirePressureRlKpa", OBXPropertyType_Float, 9, 6026000000000009ULL);
    obx_model_property(model, "tirePressureRrKpa", OBXPropertyType_Float, 10, 6026000000000010ULL);
    obx_model_property(model, "absActive", OBXPropertyType_Bool, 11, 6026000000000011ULL);
    obx_model_property(model, "tractionControlActive", OBXPropertyType_Bool, 12, 6026000000000012ULL);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 13, 6026000000000013ULL);
    obx_model_entity_last_property_id(model, 13, 6026000000000013ULL);

    obx_model_last_entity_id(model, 24, 6026000000000000ULL);
    obx_model_last_index_id(model, 24, 6026000000000200ULL);

    return model;
}

bool init_objectbox(const Config& config) {
    std::cout << "=== ObjectBox Search Service (C++ High-Level API) ===" << std::endl;
    std::cout << "Database: " << config.db_path << std::endl;
    
    try {
        // Create model programmatically
        OBX_model* model = create_obx_model();
        
        // Create store with model
        obx::Options options(model);
        options.directory(config.db_path.c_str());
        store = std::make_shared<obx::Store>(options);
        std::cout << "✓ Store opened" << std::endl;
        
        // Get chunk count
        auto chunkBox = store->box<ManualChunk>();
        uint64_t count = chunkBox.count();
        std::cout << "✓ Chunk box opened (" << count << " chunks)" << std::endl;
        
        // Initialize sync if enabled
        if (config.enable_sync && !config.sync_url.empty()) {
            if (obx_has_feature(OBXFeature_Sync)) {
                std::cout << "\nConnecting to sync server: " << config.sync_url << std::endl;
                
                syncClient = std::make_unique<obx::SyncClient>(
                    *store, 
                    config.sync_url, 
                    obx::SyncCredentials::none()
                );
                syncClient->start();
                
                std::cout << "✓ Sync client started" << std::endl;
                
                // Wait for initial sync
                std::cout << "Waiting for initial sync (5 seconds)..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
                count = chunkBox.count();
                std::cout << "✓ Initial sync complete (" << count << " chunks)" << std::endl;
            } else {
                std::cerr << "Warning: ObjectBox Sync is not available in this build" << std::endl;
            }
        }
        
        std::cout << "\n=== Service Ready ===" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize ObjectBox: " << e.what() << std::endl;
        return false;
    }
}

json search_chunks(const std::vector<float>& embedding, int limit) {
    json results;
    results["count"] = 0;
    results["results"] = json::array();
    
    try {
        auto chunkBox = store->box<ManualChunk>();
        
        // Query with nearest neighbor search
        auto query = chunkBox.query(
            ManualChunk_::embedding.nearestNeighbors(embedding, limit)
        ).build();
        
        auto foundWithScores = query.findWithScores();
        results["count"] = foundWithScores.size();
        
        for (const auto& [chunk, score] : foundWithScores) {
            json item;
            item["id"] = chunk.id;
            item["text"] = chunk.text;
            item["source_file"] = chunk.source_file;
            item["chunk_index"] = chunk.chunk_index;
            item["score"] = score;
            results["results"].push_back(item);
        }
        
    } catch (const std::exception& e) {
        results["error"] = e.what();
    }
    
    return results;
}

int main(int argc, char* argv[]) {
    // Parse config
    Config config;
    config.db_path = argc > 1 ? argv[1] : "/app/search-service-db";
    config.sync_url = argc > 2 ? argv[2] : "ws://sync-server:9999";
    config.enable_sync = argc > 3 ? (std::string(argv[3]) == "true") : true;
    config.port = 8080;
    
    // Initialize ObjectBox
    if (!init_objectbox(config)) {
        return 1;
    }
    
    // Create HTTP server
    httplib::Server svr;

    // Health check endpoint
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto chunkBox = store->box<ManualChunk>();
            uint64_t count = chunkBox.count();
            
            json response;
            response["status"] = "healthy";
            response["chunk_count"] = count;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["status"] = "error";
            error["error"] = e.what();
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Search endpoint
    svr.Post("/search", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            std::vector<float> embedding = body["embedding"];
            int limit = body.value("limit", 3);
            
            if (embedding.size() != 1024) {
                json error;
                error["error"] = "Embedding must be 1024 dimensions";
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            std::cout << "Searching with " << embedding.size() << " dims, limit=" << limit << std::endl;
            
            json result = search_chunks(embedding, limit);
            res.set_content(result.dump(), "application/json");

        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Ingest endpoint — load_documents.py POSTs embedded chunks here. Because the
    // box is SYNC_ENABLED and the sync client is connected, these inserts flow to
    // the Sync Server and on to MongoDB (mirrors the telemetry POST /vss/snapshot).
    // Accepts a single chunk object, or a batch via {"chunks": [ ... ]}.
    svr.Post("/chunks", [](const httplib::Request& req, httplib::Response& res) {
        auto bad_request = [&res](const std::string& msg) {
            res.status = 400;
            res.set_content(json{{"error", msg}}.dump(), "application/json");
        };

        // Malformed JSON is a client error → 400, not 500.
        json body;
        try {
            body = json::parse(req.body);
        } catch (const std::exception& e) {
            bad_request(std::string("Invalid JSON body: ") + e.what());
            return;
        }

        // Normalise to an array of chunk objects.
        json items;
        if (body.is_array()) {
            items = body;
        } else if (body.is_object() && body.contains("chunks") && body["chunks"].is_array()) {
            items = body["chunks"];
        } else if (body.is_object()) {
            items = json::array({body});  // single chunk object
        } else {
            bad_request("Body must be a chunk object, an array of chunks, or {\"chunks\": [ ... ]}.");
            return;
        }

        try {
            auto chunkBox = store->box<ManualChunk>();
            int inserted = 0;

            for (size_t i = 0; i < items.size(); ++i) {
                const auto& item = items[i];

                // ── Per-item validation — all client errors → 400 ──────────────
                if (!item.is_object()) {
                    bad_request("Chunk " + std::to_string(i) + " must be a JSON object.");
                    return;
                }
                auto emb_it = item.find("embedding");
                if (emb_it == item.end() || !emb_it->is_array()) {
                    bad_request("Chunk " + std::to_string(i) + " is missing an 'embedding' array.");
                    return;
                }
                std::vector<float> embedding;
                try {
                    embedding = emb_it->get<std::vector<float>>();
                } catch (const std::exception&) {
                    bad_request("Chunk " + std::to_string(i) + " 'embedding' must be an array of numbers.");
                    return;
                }
                if (embedding.size() != 1024) {
                    bad_request("Chunk " + std::to_string(i) + " embedding must be 1024 dimensions (got "
                                + std::to_string(embedding.size()) + ").");
                    return;
                }

                ManualChunk chunk;
                chunk.id          = 0;  // new insert
                chunk.text        = item.value("text", "");
                chunk.source_file = item.value("source_file", "");
                chunk.chunk_index = item.value("chunk_index", 0);
                chunk.embedding   = std::move(embedding);
                chunk.syncClock   = 0;  // set by the Sync Server
                chunkBox.put(chunk);
                inserted++;
            }

            json response;
            response["inserted"]    = inserted;
            response["chunk_count"] = chunkBox.count();
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            // Genuine server-side failure (e.g. store write) → 500.
            json error;
            error["error"] = e.what();
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    std::cout << "Starting HTTP server on 0.0.0.0:" << config.port << "..." << std::endl;
    svr.listen("0.0.0.0", config.port);
    
    return 0;
}
