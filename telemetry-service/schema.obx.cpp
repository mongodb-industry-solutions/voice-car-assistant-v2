// Manually generated ObjectBox schema implementation for TelemetrySnapshot entity

#include "schema.obx.hpp"

// Property definitions for TelemetrySnapshot (Entity ID: 4, maps to "telemetry_snapshots")
const obx::Property<TelemetrySnapshot, OBXPropertyType_Long> TelemetrySnapshot_::id(1);
const obx::Property<TelemetrySnapshot, OBXPropertyType_Long> TelemetrySnapshot_::timestamp(2);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::vehicle_id(3);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::driving_mode(4);
const obx::Property<TelemetrySnapshot, OBXPropertyType_Int> TelemetrySnapshot_::anomaly_count(5);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::engine_data(6);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::tire_data(7);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::battery_data(8);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::fuel_data(9);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::transmission_data(10);
const obx::Property<TelemetrySnapshot, OBXPropertyType_String> TelemetrySnapshot_::brake_data(11);
const obx::Property<TelemetrySnapshot, OBXPropertyType_Long> TelemetrySnapshot_::syncClock(12);

// TelemetrySnapshot serialization
void TelemetrySnapshot::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const TelemetrySnapshot& object) {
    fbb.Clear();
    auto offset_vehicle_id = fbb.CreateString(object.vehicle_id);
    auto offset_driving_mode = fbb.CreateString(object.driving_mode);
    auto offset_engine_data = fbb.CreateString(object.engine_data);
    auto offset_tire_data = fbb.CreateString(object.tire_data);
    auto offset_battery_data = fbb.CreateString(object.battery_data);
    auto offset_fuel_data = fbb.CreateString(object.fuel_data);
    auto offset_transmission_data = fbb.CreateString(object.transmission_data);
    auto offset_brake_data = fbb.CreateString(object.brake_data);
    
    flatbuffers::uoffset_t fbStart = fbb.StartTable();
    fbb.AddElement(4, object.id);  // Property 1: id
    fbb.AddElement(6, object.timestamp);  // Property 2: timestamp
    fbb.AddOffset(8, offset_vehicle_id);  // Property 3: vehicle_id
    fbb.AddOffset(10, offset_driving_mode);  // Property 4: driving_mode
    fbb.AddElement(12, object.anomaly_count);  // Property 5: anomaly_count
    fbb.AddOffset(14, offset_engine_data);  // Property 6: engine_data
    fbb.AddOffset(16, offset_tire_data);  // Property 7: tire_data
    fbb.AddOffset(18, offset_battery_data);  // Property 8: battery_data
    fbb.AddOffset(20, offset_fuel_data);  // Property 9: fuel_data
    fbb.AddOffset(22, offset_transmission_data);  // Property 10: transmission_data
    fbb.AddOffset(24, offset_brake_data);  // Property 11: brake_data
    fbb.AddElement(26, object.syncClock);  // Property 12: syncClock
    
    flatbuffers::Offset<flatbuffers::Table> offset;
    offset.o = fbb.EndTable(fbStart);
    fbb.Finish(offset);
}

TelemetrySnapshot TelemetrySnapshot::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t size) {
    TelemetrySnapshot object;
    fromFlatBuffer(data, size, object);
    return object;
}

std::unique_ptr<TelemetrySnapshot> TelemetrySnapshot::_OBX_MetaInfo::newFromFlatBuffer(const void* data, size_t size) {
    auto object = std::make_unique<TelemetrySnapshot>();
    fromFlatBuffer(data, size, *object);
    return object;
}

void TelemetrySnapshot::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, TelemetrySnapshot& outObject) {
    const auto* table = flatbuffers::GetRoot<flatbuffers::Table>(data);
    assert(table);
    
    // Property 1: id
    outObject.id = table->GetField<int64_t>(4, 0);
    
    // Property 2: timestamp
    outObject.timestamp = table->GetField<int64_t>(6, 0);
    
    // Property 3: vehicle_id
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(8);
        if (ptr) {
            outObject.vehicle_id.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.vehicle_id.clear();
        }
    }
    
    // Property 4: driving_mode
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(10);
        if (ptr) {
            outObject.driving_mode.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.driving_mode.clear();
        }
    }
    
    // Property 5: anomaly_count
    outObject.anomaly_count = table->GetField<int32_t>(12, 0);
    
    // Property 6: engine_data
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(14);
        if (ptr) {
            outObject.engine_data.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.engine_data.clear();
        }
    }
    
    // Property 7: tire_data
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(16);
        if (ptr) {
            outObject.tire_data.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.tire_data.clear();
        }
    }
    
    // Property 8: battery_data
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(18);
        if (ptr) {
            outObject.battery_data.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.battery_data.clear();
        }
    }
    
    // Property 9: fuel_data
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(20);
        if (ptr) {
            outObject.fuel_data.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.fuel_data.clear();
        }
    }
    
    // Property 10: transmission_data
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(22);
        if (ptr) {
            outObject.transmission_data.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.transmission_data.clear();
        }
    }
    
    // Property 11: brake_data
    {
        auto* ptr = table->GetPointer<const flatbuffers::String*>(24);
        if (ptr) {
            outObject.brake_data.assign(ptr->c_str(), ptr->size());
        } else {
            outObject.brake_data.clear();
        }
    }
    
    // Property 12: syncClock
    outObject.syncClock = table->GetField<int64_t>(26, 0);
}
