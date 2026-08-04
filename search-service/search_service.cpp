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
#include <cstdlib>

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
    // ALL sync clients must present this identical model or the Sync Server rejects them.
    // Regenerate whenever the shared model changes.

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
    obx_model_property_external_type(model, OBXExternalPropertyType_JsonToNative);  // JSON array → native array in Atlas
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 8, 8888999911112222333ULL);
    obx_model_property(model, "tools_used", OBXPropertyType_String, 9, 9099888877776666555ULL);
    obx_model_property_external_type(model, OBXExternalPropertyType_JsonToNative);  // JSON array → native array in Atlas
    obx_model_entity_last_property_id(model, 9, 9099888877776666555ULL);

    // Entity 26: objectbox_telemetry
    obx_model_entity(model, "objectbox_telemetry", 26, 6030000000000000ULL);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 6030000000000001ULL);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "vehicleId", OBXPropertyType_String, 2, 6030000000000002ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 25, 6030000000000100ULL);
    obx_model_property(model, "ts", OBXPropertyType_Long, 3, 6030000000000003ULL);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 26, 6030000000000200ULL);
    obx_model_property(model, "data", OBXPropertyType_String, 4, 6030000000000004ULL);
    obx_model_property_external_type(model, OBXExternalPropertyType_JsonToNative);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 5, 6030000000000005ULL);
    obx_model_property(model, "meta", OBXPropertyType_String, 6, 6030000000000006ULL);
    obx_model_property_external_type(model, OBXExternalPropertyType_JsonToNative);
    obx_model_entity_last_property_id(model, 6, 6030000000000006ULL);

    obx_model_last_entity_id(model, 26, 6030000000000000ULL);
    obx_model_last_index_id(model, 26, 6030000000000200ULL);

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
        options.maxDbSizeInKByte(1572864);  // 1.5 GiB (default is 1 GiB)
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
    // SYNC_SERVER_URL / PORT env override args+defaults (single-pod deploy sets these);
    // local/compose leaves them unset and keeps the argv defaults below.
    const char* sync_env = std::getenv("SYNC_SERVER_URL");
    config.sync_url = sync_env ? std::string(sync_env) : (argc > 2 ? argv[2] : "ws://sync-server:9999");
    config.enable_sync = argc > 3 ? (std::string(argv[3]) == "true") : true;
    config.port = 8080;
    if (const char* port_env = std::getenv("PORT")) {
        try { config.port = std::stoi(port_env); }
        catch (const std::exception&) {
            std::cerr << "Invalid PORT='" << port_env << "'; using default " << config.port << std::endl;
        }
    }
    
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
