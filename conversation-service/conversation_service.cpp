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
#include <cstdlib>
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
        options.maxDbSizeInKByte(1572864);  // 1.5 GiB (default is 1 GiB)
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
    // Env overrides for the single-pod deploy (unset locally → keep args/defaults).
    if (const char* s = std::getenv("SYNC_SERVER_URL")) config.sync_server_url = s;
    if (const char* p = std::getenv("PORT")) {
        try { config.port = std::stoi(p); }
        catch (const std::exception&) {
            std::cerr << "Invalid PORT='" << p << "'; using default " << config.port << std::endl;
        }
    }

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
            // sources is JsonToNative → must be valid JSON; default/empty becomes an empty array.
            conv.sources = body.value("sources", "");
            if (conv.sources.empty()) conv.sources = "[]";
            // tools_used is JsonToNative → must be valid JSON; default/empty becomes an empty array.
            conv.tools_used = body.value("tools_used", "");  // JSON array of tool names (assistant turns)
            if (conv.tools_used.empty()) conv.tools_used = "[]";
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
                    {"tools_used", conv.tools_used},
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
                    {"tools_used", conv.tools_used},
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
