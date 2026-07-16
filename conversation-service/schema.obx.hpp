// Manually generated ObjectBox schema for Conversation entity

#pragma once

#include <cstdbool>
#include <cstdint>
#include <string>
#include <vector>

#include "flatbuffers/flatbuffers.h"
#include "objectbox.h"
#include "objectbox.hpp"

// Forward declaration
struct Conversation_;

// Conversation entity (maps to "conversations" id: 3, from objectbox-model.json)
struct Conversation {
    int64_t id;
    std::string conversation_id;
    std::string user_id;
    int64_t timestamp;
    std::string role;
    std::string message;
    std::string sources;
    int64_t syncClock;
    std::string tools_used;

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 3; }
    
        static void setObjectId(Conversation& object, obx_id newId) { object.id = newId; }
    
        /// Write given object to the FlatBufferBuilder
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const Conversation& object);
    
        /// Read an object from a valid FlatBuffer
        static Conversation fromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static std::unique_ptr<Conversation> newFromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static void fromFlatBuffer(const void* data, size_t size, Conversation& outObject);
    };
};

struct Conversation_ {
    static const obx::Property<Conversation, OBXPropertyType_Long> id;
    static const obx::Property<Conversation, OBXPropertyType_String> conversation_id;
    static const obx::Property<Conversation, OBXPropertyType_String> user_id;
    static const obx::Property<Conversation, OBXPropertyType_Long> timestamp;
    static const obx::Property<Conversation, OBXPropertyType_String> role;
    static const obx::Property<Conversation, OBXPropertyType_String> message;
    static const obx::Property<Conversation, OBXPropertyType_String> sources;
    static const obx::Property<Conversation, OBXPropertyType_Long> syncClock;
    static const obx::Property<Conversation, OBXPropertyType_String> tools_used;
};
