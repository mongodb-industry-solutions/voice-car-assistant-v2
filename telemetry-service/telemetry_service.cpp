/**
 * Telemetry Service - ObjectBox-based telemetry storage with sync
 * 
 * Receives telemetry snapshots from the simulator, stores them locally,
 * and automatically syncs to MongoDB Atlas via ObjectBox Sync Server.
 *
 * Endpoints:
 *   POST /telemetry           - Save telemetry snapshot
 *   GET  /telemetry/latest    - Get latest snapshot
 *   GET  /telemetry/range     - Get time range
 *   GET  /health              - Health check
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
    std::string db_path = "/app/telemetry-db";
    std::string sync_server_url = "ws://sync-server:9999";
    bool enable_sync = true;
    int port = 8084;
};

// Create ObjectBox model programmatically
OBX_model* create_obx_model() {
    OBX_model* model = obx_model();
    if (!model) return nullptr;

    // Entity 1: manual_chunks (for sync compatibility)
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
    
    // Entity 2: manuals (for sync compatibility)
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

    // Entity 3: conversations (for sync compatibility)
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

    // Entity 4: telemetry_snapshots (Option B structure)
    obx_model_entity(model, "telemetry_snapshots", 4, 2222333344445555777);
    obx_model_entity_flags(model, OBXEntityFlags_SYNC_ENABLED);  // ✅ Enable sync
    obx_model_property(model, "id", OBXPropertyType_Long, 1, 2222333344445555778);
    obx_model_property_flags(model, OBXPropertyFlags_ID);
    obx_model_property(model, "timestamp", OBXPropertyType_Long, 2, 3333444455556666888);
    obx_model_property_flags(model, OBXPropertyFlags_INDEXED);
    obx_model_property_index_id(model, 5, 5555555555555555555);
    obx_model_property(model, "vehicle_id", OBXPropertyType_String, 3, 9988776655443322114);
    obx_model_property(model, "driving_mode", OBXPropertyType_String, 4, 9988776655443322115);
    obx_model_property(model, "anomaly_count", OBXPropertyType_Int, 5, 9988776655443322116);
    obx_model_property(model, "engine_data", OBXPropertyType_String, 6, 9988776655443322117);
    obx_model_property(model, "tire_data", OBXPropertyType_String, 7, 9988776655443322118);
    obx_model_property(model, "battery_data", OBXPropertyType_String, 8, 9988776655443322119);
    obx_model_property(model, "fuel_data", OBXPropertyType_String, 9, 9988776655443322120);
    obx_model_property(model, "transmission_data", OBXPropertyType_String, 10, 9988776655443322121);
    obx_model_property(model, "brake_data", OBXPropertyType_String, 11, 9988776655443322122);
    obx_model_property(model, "syncClock", OBXPropertyType_Long, 12, 9988776655443322123);
    obx_model_entity_last_property_id(model, 12, 9988776655443322123);
    
    obx_model_last_entity_id(model, 5, 9988776655443322111);
    obx_model_last_index_id(model, 6, 6666666666666666666);
    
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
    std::cout << "📊 Telemetry Service - ObjectBox + Sync\n";
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

    std::cout << "✅ ObjectBox store initialized\n";
    
    // Check sync availability
    bool isSyncAvailable = obx::Sync::isAvailable();
    std::cout << "🔍 ObjectBox Sync is " << (isSyncAvailable ? "available" : "unavailable") << "\n\n";

    // Initialize sync FIRST - MUST KEEP ALIVE for entire program lifetime!
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

    // NOW we can get the box after sync is initialized
    auto box = store->box<TelemetrySnapshot>();
    std::cout << "📊 Current snapshots: " << box.count() << "\n\n";

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

    // POST /telemetry - Save telemetry snapshot (Option B: per-system JSON fields)
    svr.Post("/telemetry", [&box](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            
            TelemetrySnapshot snapshot;
            snapshot.id = 0;  // Auto-generated
            snapshot.timestamp = body["timestamp"].get<int64_t>();
            snapshot.vehicle_id = body["vehicle_id"].get<std::string>();
            snapshot.driving_mode = body.value("driving_mode", "");
            snapshot.anomaly_count = body["anomaly_count"].get<int32_t>();
            
            // Extract each system from telemetry_batch and store as separate JSON
            if (body.contains("telemetry_batch")) {
                auto telemetry_batch = body["telemetry_batch"];
                
                snapshot.engine_data = telemetry_batch.value("engine", json::object()).dump();
                snapshot.tire_data = telemetry_batch.value("tires", json::object()).dump();
                snapshot.battery_data = telemetry_batch.value("battery", json::object()).dump();
                snapshot.fuel_data = telemetry_batch.value("fuel", json::object()).dump();
                snapshot.transmission_data = telemetry_batch.value("transmission", json::object()).dump();
                snapshot.brake_data = telemetry_batch.value("brakes", json::object()).dump();
            }
            
            snapshot.syncClock = 0;  // Managed by sync

            obx_id id = box.put(snapshot);
            
            json response = {
                {"success", true},
                {"id", id},
                {"timestamp", snapshot.timestamp},
                {"anomaly_count", snapshot.anomaly_count}
            };
            
            res.set_content(response.dump(), "application/json");
            
            // Log with anomaly indication
            if (snapshot.anomaly_count > 0) {
                std::cout << "⚠️ Saved telemetry ID " << id << " with " 
                          << snapshot.anomaly_count << " anomalies\n";
            } else {
                std::cout << "✅ Saved telemetry ID " << id << " (normal)\n";
            }
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /telemetry/latest - Get latest snapshot
    svr.Get("/telemetry/latest", [&box](const Request&, Response& res) {
        try {
            auto query = box.query().order(TelemetrySnapshot_::timestamp).build();
            auto results = query.find();
            
            if (results.empty()) {
                json response = {"error", "No telemetry data available"};
                res.status = 404;
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            // Reconstruct response from per-system fields
            auto& snapshot = results.back();
            json response = {
                {"timestamp", snapshot.timestamp},
                {"vehicle_id", snapshot.vehicle_id},
                {"driving_mode", snapshot.driving_mode},
                {"anomaly_count", snapshot.anomaly_count},
                {"telemetry_batch", {
                    {"engine", json::parse(snapshot.engine_data.empty() ? "{}" : snapshot.engine_data)},
                    {"tires", json::parse(snapshot.tire_data.empty() ? "{}" : snapshot.tire_data)},
                    {"battery", json::parse(snapshot.battery_data.empty() ? "{}" : snapshot.battery_data)},
                    {"fuel", json::parse(snapshot.fuel_data.empty() ? "{}" : snapshot.fuel_data)},
                    {"transmission", json::parse(snapshot.transmission_data.empty() ? "{}" : snapshot.transmission_data)},
                    {"brakes", json::parse(snapshot.brake_data.empty() ? "{}" : snapshot.brake_data)}
                }}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /telemetry/range?start=<ts>&end=<ts> - Get time range
    svr.Get("/telemetry/range", [&box](const Request& req, Response& res) {
        try {
            auto start_param = req.get_param_value("start");
            auto end_param = req.get_param_value("end");
            
            if (start_param.empty() || end_param.empty()) {
                json error = {{"error", "Missing start or end parameter"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            int64_t start = std::stoll(start_param);
            int64_t end = std::stoll(end_param);
            
            // Simple approach: get all and filter manually (for small result sets)
            auto query = box.query().order(TelemetrySnapshot_::timestamp).build();
            auto all_results = query.find();
            
            json snapshots = json::array();
            for (const auto& snapshot : all_results) {
                if (snapshot.timestamp >= start && snapshot.timestamp <= end) {
                    // Reconstruct telemetry from per-system fields
                    json telemetry_entry = {
                        {"timestamp", snapshot.timestamp},
                        {"vehicle_id", snapshot.vehicle_id},
                        {"driving_mode", snapshot.driving_mode},
                        {"anomaly_count", snapshot.anomaly_count},
                        {"telemetry_batch", {
                            {"engine", json::parse(snapshot.engine_data.empty() ? "{}" : snapshot.engine_data)},
                            {"tires", json::parse(snapshot.tire_data.empty() ? "{}" : snapshot.tire_data)},
                            {"battery", json::parse(snapshot.battery_data.empty() ? "{}" : snapshot.battery_data)},
                            {"fuel", json::parse(snapshot.fuel_data.empty() ? "{}" : snapshot.fuel_data)},
                            {"transmission", json::parse(snapshot.transmission_data.empty() ? "{}" : snapshot.transmission_data)},
                            {"brakes", json::parse(snapshot.brake_data.empty() ? "{}" : snapshot.brake_data)}
                        }}
                    };
                    snapshots.push_back(telemetry_entry);
                }
            }
            
            json response = {
                {"count", snapshots.size()},
                {"snapshots", snapshots}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /telemetry/stats - Get statistics
    svr.Get("/telemetry/stats", [&box](const Request&, Response& res) {
        try {
            int64_t count = box.count();
            
            // Get latest snapshot
            auto query = box.query().order(TelemetrySnapshot_::timestamp).build();
            auto results = query.find();
            
            json stats = {
                {"total_snapshots", count},
                {"has_data", count > 0}
            };
            
            if (!results.empty()) {
                stats["latest_timestamp"] = results.back().timestamp;
                stats["latest_anomaly_count"] = results.back().anomaly_count;
            }
            
            res.set_content(stats.dump(), "application/json");
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
            {"snapshot_count", box.count()},
            {"service", "telemetry-service"}
        };
        res.set_content(health.dump(), "application/json");
    });

    // Start server
    std::cout << "🚀 Starting HTTP server on port " << config.port << "...\n\n";
    svr.listen("0.0.0.0", config.port);

    return 0;
}
