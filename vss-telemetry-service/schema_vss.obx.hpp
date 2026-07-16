// Manually generated ObjectBox VSS schema — single ObxTelemetry snapshot entity
#pragma once

#include <cstdbool>
#include <cstdint>
#include <string>
#include <vector>

#include "flatbuffers/flatbuffers.h"
#include "objectbox.h"
#include "objectbox.hpp"

// ── Forward declarations ──────────────────────────────────────────────────────
struct ObxTelemetry_;

// ── Entity 26: objectbox_telemetry ────────────────────────────────────────────
// One row per snapshot. `data` (domains) and `meta` (vehicle metadata) are each
// stored as a JSON string flagged JsonToNative, so the MongoDB connector expands
// them into native nested documents (siblings) in the objectbox_telemetry collection.
struct ObxTelemetry {
    int64_t     id = 0;         // P1
    std::string vehicleId;      // P2  indexed
    int64_t     ts = 0;         // P3  indexed
    std::string data;           // P4  domain payload JSON (external type: JsonToNative)
    int64_t     syncClock = 0;  // P5
    std::string meta;           // P6  vehicle metadata JSON (external type: JsonToNative)

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 26; }
        static void setObjectId(ObxTelemetry& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ObxTelemetry& o);
        static ObxTelemetry fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<ObxTelemetry> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, ObxTelemetry& out);
    };
};
struct ObxTelemetry_ {
    static const obx::Property<ObxTelemetry, OBXPropertyType_Long>   id;
    static const obx::Property<ObxTelemetry, OBXPropertyType_String> vehicleId;
    static const obx::Property<ObxTelemetry, OBXPropertyType_Long>   ts;
};
