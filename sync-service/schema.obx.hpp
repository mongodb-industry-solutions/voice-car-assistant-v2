// Manually generated ObjectBox schema for ManualChunk and Manual entities

#pragma once

#include <cstdbool>
#include <cstdint>
#include <string>
#include <vector>

#include "flatbuffers/flatbuffers.h"
#include "objectbox.h"
#include "objectbox.hpp"

// Forward declarations
struct ManualChunk_;
struct Manual_;

// ManualChunk entity (maps to "manual_chunks" id: 1, from objectbox-model.json)
struct ManualChunk {
    int64_t id;
    std::string text;
    std::string source_file;
    int32_t chunk_index;
    std::vector<float> embedding;  // 1024 dimensions with HNSW index
    int64_t syncClock;  // Note: property name is syncClock not sync_clock

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 1; }  // Changed from 3 to 1
    
        static void setObjectId(ManualChunk& object, obx_id newId) { object.id = newId; }
    
        /// Write given object to the FlatBufferBuilder
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ManualChunk& object);
    
        /// Read an object from a valid FlatBuffer
        static ManualChunk fromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static std::unique_ptr<ManualChunk> newFromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static void fromFlatBuffer(const void* data, size_t size, ManualChunk& outObject);
    };
};

struct ManualChunk_ {
    static const obx::Property<ManualChunk, OBXPropertyType_Long> id;
    static const obx::Property<ManualChunk, OBXPropertyType_String> text;
    static const obx::Property<ManualChunk, OBXPropertyType_String> source_file;
    static const obx::Property<ManualChunk, OBXPropertyType_Int> chunk_index;
    static const obx::Property<ManualChunk, OBXPropertyType_FloatVector> embedding;
    static const obx::Property<ManualChunk, OBXPropertyType_Long> syncClock;
};

// Manual entity (maps to "manuals" id: 2, from objectbox-model.json)
struct Manual {
    int64_t id;
    std::string filename;
    std::string make;
    std::string model;
    int32_t total_chunks;
    std::string status;
    int64_t syncClock;  // Note: property name is syncClock not sync_clock

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 2; }  // Changed from 4 to 2
    
        static void setObjectId(Manual& object, obx_id newId) { object.id = newId; }
    
        /// Write given object to the FlatBufferBuilder
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const Manual& object);
    
        /// Read an object from a valid FlatBuffer
        static Manual fromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static std::unique_ptr<Manual> newFromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static void fromFlatBuffer(const void* data, size_t size, Manual& outObject);
    };
};

struct Manual_ {
    static const obx::Property<Manual, OBXPropertyType_Long> id;
    static const obx::Property<Manual, OBXPropertyType_String> filename;
    static const obx::Property<Manual, OBXPropertyType_String> make;
    static const obx::Property<Manual, OBXPropertyType_String> model;
    static const obx::Property<Manual, OBXPropertyType_Int> total_chunks;
    static const obx::Property<Manual, OBXPropertyType_String> status;
    static const obx::Property<Manual, OBXPropertyType_Long> syncClock;
};
