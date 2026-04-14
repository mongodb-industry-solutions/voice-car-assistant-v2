// Manually generated ObjectBox schema for TelemetrySnapshot entity

#pragma once

#include <cstdbool>
#include <cstdint>
#include <string>
#include <vector>

#include "flatbuffers/flatbuffers.h"
#include "objectbox.h"
#include "objectbox.hpp"

// Forward declaration
struct TelemetrySnapshot_;

// TelemetrySnapshot entity (maps to "telemetry_snapshots" id: 4, from objectbox-model.json)
struct TelemetrySnapshot {
    int64_t id;
    int64_t timestamp;
    std::string vehicle_id;
    std::string driving_mode;
    int32_t anomaly_count;
    std::string engine_data;
    std::string tire_data;
    std::string battery_data;
    std::string fuel_data;
    std::string transmission_data;
    std::string brake_data;
    int64_t syncClock;

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 4; }  // Entity ID 4 for telemetry_snapshots
    
        static void setObjectId(TelemetrySnapshot& object, obx_id newId) { object.id = newId; }
    
        /// Write given object to the FlatBufferBuilder
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const TelemetrySnapshot& object);
    
        /// Read an object from a valid FlatBuffer
        static TelemetrySnapshot fromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static std::unique_ptr<TelemetrySnapshot> newFromFlatBuffer(const void* data, size_t size);
    
        /// Read an object from a valid FlatBuffer
        static void fromFlatBuffer(const void* data, size_t size, TelemetrySnapshot& outObject);
    };
};

struct TelemetrySnapshot_ {
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_Long> id;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_Long> timestamp;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> vehicle_id;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> driving_mode;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_Int> anomaly_count;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> engine_data;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> tire_data;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> battery_data;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> fuel_data;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> transmission_data;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_String> brake_data;
    static const obx::Property<TelemetrySnapshot, OBXPropertyType_Long> syncClock;
};
