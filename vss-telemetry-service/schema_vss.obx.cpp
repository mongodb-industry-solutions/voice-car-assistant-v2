// FlatBuffer serialization for ObxTelemetry
#include "schema_vss.obx.hpp"

// slot = propertyId * 2 + 2
static void read_str(const flatbuffers::Table* t, int slot, std::string& out) {
    auto* p = t->GetPointer<const flatbuffers::String*>(slot);
    if (p) out.assign(p->c_str(), p->size()); else out.clear();
}

// ── Property definitions ──────────────────────────────────────────────────────
const obx::Property<ObxTelemetry, OBXPropertyType_Long>   ObxTelemetry_::id(1);
const obx::Property<ObxTelemetry, OBXPropertyType_String> ObxTelemetry_::vehicleId(2);
const obx::Property<ObxTelemetry, OBXPropertyType_Long>   ObxTelemetry_::ts(3);

// ── ObxTelemetry (entity 26, 6 props) ─────────────────────────────────────────
void ObxTelemetry::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ObxTelemetry& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_data      = fbb.CreateString(o.data);
    auto s_meta      = fbb.CreateString(o.meta);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_data);
    fbb.AddElement<int64_t>(12, o.syncClock);
    fbb.AddOffset(14, s_meta);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void ObxTelemetry::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, ObxTelemetry& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id        = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts        = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.data);
    o.syncClock = t->GetField<int64_t>(12, 0);
    read_str(t, 14, o.meta);
}
ObxTelemetry ObxTelemetry::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { ObxTelemetry o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<ObxTelemetry> ObxTelemetry::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<ObxTelemetry>(); fromFlatBuffer(d,s,*o); return o; }
