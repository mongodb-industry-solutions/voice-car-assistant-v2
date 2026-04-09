/**
 * ObjectBox Search Service with Sync (C++)
 * Uses C API directly with objectbox-model.json - no schema generation needed
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

// ObjectBox C API
#include "objectbox.h"

// Simple HTTP server using cpp-httplib (header-only)
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

// JSON parsing (nlohmann/json - header-only)
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Entity IDs from objectbox-model.json
const obx_schema_id ENTITY_MANUAL_CHUNK = 3;
const obx_schema_id ENTITY_MANUAL = 4;

// Property IDs for ManualChunk
const obx_schema_id PROP_CHUNK_ID = 1;
const obx_schema_id PROP_CHUNK_TEXT = 2;
const obx_schema_id PROP_CHUNK_SOURCE_FILE = 3;
const obx_schema_id PROP_CHUNK_INDEX = 4;
const obx_schema_id PROP_CHUNK_EMBEDDING = 5;
const obx_schema_id PROP_CHUNK_SYNC_CLOCK = 6;

// Global ObjectBox store
OBX_store* store = nullptr;
OBX_box* chunk_box = nullptr;
OBX_sync* sync_client = nullptr;

struct Config {
    std::string db_path;
    std::string sync_url;
    bool enable_sync;
    int port;
};

// Read model JSON file
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Initialize ObjectBox store with model JSON
bool init_objectbox(const Config& config) {
    std::cout << "=== Initializing ObjectBox ===" << std::endl;
    std::cout << "Database: " << config.db_path << std::endl;
    
    // Read model JSON
    std::string model_json_path = "/app/objectbox-model.json";
    std::cout << "Loading model: " << model_json_path << std::endl;
    
    std::string model_json = read_file(model_json_path);
    
    // Create store options
    OBX_store_options* opt = obx_opt();
    if (!opt) {
        std::cerr << "Failed to create store options" << std::endl;
        return false;
    }
    
    // Set directory
    obx_opt_directory(opt, config.db_path.c_str());
    
    // Set model from JSON string
    obx_err err = obx_opt_model_bytes_direct(opt, model_json.c_str(), model_json.size());
    if (err != OBX_SUCCESS) {
        std::cerr << "Failed to set model: " << obx_last_error_message() << std::endl;
        obx_opt_free(opt);
        return false;
    }
    
    // Create store
    store = obx_store_open(opt);
    if (!store) {
        std::cerr << "Failed to open store: " << obx_last_error_message() << std::endl;
        return false;
    }
    
    std::cout << "✓ Store opened" << std::endl;
    
    // Get box for ManualChunk
    chunk_box = obx_box(store, ENTITY_MANUAL_CHUNK);
    if (!chunk_box) {
        std::cerr << "Failed to get box: " << obx_last_error_message() << std::endl;
        return false;
    }
    
    uint64_t count = 0;
    obx_box_count(chunk_box, 0, &count);
    std::cout << "✓ Chunk box opened (" << count << " chunks)" << std::endl;
    
    // Initialize sync if enabled
    if (config.enable_sync && !config.sync_url.empty()) {
        std::cout << "\nConnecting to sync server: " << config.sync_url << std::endl;
        
        sync_client = obx_sync(store);
        if (!sync_client) {
            std::cerr << "Failed to create sync client: " << obx_last_error_message() << std::endl;
            return false;
        }
        
        // Set server URL
        obx_sync_url(sync_client, config.sync_url.c_str());
        
        // No credentials (same as SyncCredentials.none())
        obx_sync_credentials(sync_client, OBXSyncCredentialsType_NONE, nullptr, 0);
        
        // Start sync
        err = obx_sync_start(sync_client);
        if (err != OBX_SUCCESS) {
            std::cerr << "Failed to start sync: " << obx_last_error_message() << std::endl;
            return false;
        }
        
        std::cout << "✓ Sync client started" << std::endl;
        
        // Wait for initial sync
        std::cout << "Waiting for initial sync (5 seconds)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        obx_box_count(chunk_box, 0, &count);
        std::cout << "✓ Initial sync complete (" << count << " chunks)" << std::endl;
    }
    
    std::cout << "\n=== Service Ready ===" << std::endl;
    return true;
}

// Search using HNSW vector index
json search_chunks(const std::vector<float>& embedding, int limit) {
    json results;
    results["count"] = 0;
    results["results"] = json::array();
    
    if (!chunk_box) {
        results["error"] = "Chunk box not initialized";
        return results;
    }
    
    // Create query for nearest neighbors
    OBX_query_builder* qb = obx_query_builder(store, ENTITY_MANUAL_CHUNK);
    if (!qb) {
        results["error"] = obx_last_error_message();
        return results;
    }
    
    // Add nearest neighbor condition
    obx_qb_nearest_neighbors_f32(qb, PROP_CHUNK_EMBEDDING, 
                                  embedding.data(), embedding.size(), limit);
    
    OBX_query* query = obx_query(qb);
    if (!query) {
        results["error"] = obx_last_error_message();
        obx_qb_close(qb);
        return results;
    }
    
    // Find with scores
    OBX_bytes_score_array* found = obx_query_find_with_scores(query);
    if (!found) {
        results["error"] = obx_last_error_message();
        obx_query_close(query);
        return results;
    }
    
    // Parse results (simplified - you'd use FlatBuffers to properly parse)
    results["count"] = found->count;
    
    for (size_t i = 0; i < found->count; i++) {
        json item;
        item["score"] = found->scores[i];
        // Note: Proper implementation would deserialize FlatBuffers here
        // For now, just return score
        results["results"].push_back(item);
    }
    
    obx_bytes_score_array_free(found);
    obx_query_close(query);
    
    return results;
}

void cleanup() {
    // Sync is automatically stopped when store closes
    if (store) {
        obx_store_close(store);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== ObjectBox Search Service (C++) ===" << std::endl;
    
    // Parse config
    Config config;
    config.db_path = argc > 1 ? argv[1] : "/app/search-service-db";
    config.sync_url = argc > 2 ? argv[2] : "ws://sync-server:9999";
    config.enable_sync = argc > 3 ? (std::string(argv[3]) == "true") : true;
    config.port = 8080;
    
    // Initialize ObjectBox
    if (!init_objectbox(config)) {
        std::cerr << "Failed to initialize ObjectBox" << std::endl;
        return 1;
    }
    
    // Create HTTP server
    httplib::Server svr;
    
    // Health check endpoint
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        uint64_t count = 0;
        if (chunk_box) obx_box_count(chunk_box, 0, &count);
        json response;
        response["status"] = "healthy";
        response["chunk_count"] = count;
        res.set_content(response.dump(), "application/json");
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
    
    // Cleanup on exit
    std::atexit(cleanup);
    
    svr.listen("0.0.0.0", config.port);
    
    return 0;
}
