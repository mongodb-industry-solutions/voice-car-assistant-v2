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
    
    // Entity 1: manual_chunks (from sync-server schema)
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
    
    // Entity 2: manuals (from sync-server schema)
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
    
    // Entity 3: conversations (not used, but required for sync compatibility)
    obx_model_entity(model, "conversations", 3, 1111222233334444555);
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
    
    // Entity 4: telemetry_snapshots (not used, but required for sync compatibility)
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
    
    obx_model_last_entity_id(model, 4, 2222333344445555777);
    obx_model_last_index_id(model, 5, 5555555555555555555);
    
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
    
    std::cout << "Starting HTTP server on 0.0.0.0:" << config.port << "..." << std::endl;
    svr.listen("0.0.0.0", config.port);
    
    return 0;
}
