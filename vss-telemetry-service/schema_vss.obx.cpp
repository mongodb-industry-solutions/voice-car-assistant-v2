// FlatBuffer serialization for all 16 VSS entities
#include "schema_vss.obx.hpp"

// ── Helpers ───────────────────────────────────────────────────────────────────
// slot = propertyId * 2 + 2
// Strings: CreateString before StartTable, AddOffset inside table
// Numerics: AddElement inside table (type deduced)
// Bool: AddElement<uint8_t> / GetField<uint8_t>

static void read_str(const flatbuffers::Table* t, int slot, std::string& out) {
    auto* p = t->GetPointer<const flatbuffers::String*>(slot);
    if (p) out.assign(p->c_str(), p->size()); else out.clear();
}

// ── Property definitions ──────────────────────────────────────────────────────
const obx::Property<VehicleMeta, OBXPropertyType_Long>   VehicleMeta_::id(1);
const obx::Property<VehicleMeta, OBXPropertyType_String> VehicleMeta_::vehicleId(2);

const obx::Property<SignalDefinition, OBXPropertyType_Long> SignalDefinition_::id(1);


const obx::Property<PowertrainState, OBXPropertyType_Long>   PowertrainState_::id(1);
const obx::Property<PowertrainState, OBXPropertyType_String> PowertrainState_::vehicleId(2);

const obx::Property<BatteryState, OBXPropertyType_Long>   BatteryState_::id(1);
const obx::Property<BatteryState, OBXPropertyType_String> BatteryState_::vehicleId(2);

const obx::Property<ChassisState, OBXPropertyType_Long>   ChassisState_::id(1);
const obx::Property<ChassisState, OBXPropertyType_String> ChassisState_::vehicleId(2);

const obx::Property<CabinState, OBXPropertyType_Long>   CabinState_::id(1);
const obx::Property<CabinState, OBXPropertyType_String> CabinState_::vehicleId(2);

const obx::Property<LocationState, OBXPropertyType_Long>   LocationState_::id(1);
const obx::Property<LocationState, OBXPropertyType_String> LocationState_::vehicleId(2);

const obx::Property<AdasState, OBXPropertyType_Long>   AdasState_::id(1);
const obx::Property<AdasState, OBXPropertyType_String> AdasState_::vehicleId(2);

const obx::Property<PowertrainSample, OBXPropertyType_Long>   PowertrainSample_::id(1);
const obx::Property<PowertrainSample, OBXPropertyType_String> PowertrainSample_::vehicleId(2);
const obx::Property<PowertrainSample, OBXPropertyType_Long>   PowertrainSample_::ts(3);

const obx::Property<BatterySample, OBXPropertyType_Long>   BatterySample_::id(1);
const obx::Property<BatterySample, OBXPropertyType_String> BatterySample_::vehicleId(2);
const obx::Property<BatterySample, OBXPropertyType_Long>   BatterySample_::ts(3);

const obx::Property<LocationSample, OBXPropertyType_Long>   LocationSample_::id(1);
const obx::Property<LocationSample, OBXPropertyType_String> LocationSample_::vehicleId(2);
const obx::Property<LocationSample, OBXPropertyType_Long>   LocationSample_::ts(3);

const obx::Property<CabinSample, OBXPropertyType_Long>   CabinSample_::id(1);
const obx::Property<CabinSample, OBXPropertyType_String> CabinSample_::vehicleId(2);
const obx::Property<CabinSample, OBXPropertyType_Long>   CabinSample_::ts(3);

const obx::Property<AdasSample, OBXPropertyType_Long>   AdasSample_::id(1);
const obx::Property<AdasSample, OBXPropertyType_String> AdasSample_::vehicleId(2);
const obx::Property<AdasSample, OBXPropertyType_Long>   AdasSample_::ts(3);

const obx::Property<VehicleEvent, OBXPropertyType_Long>   VehicleEvent_::id(1);
const obx::Property<VehicleEvent, OBXPropertyType_String> VehicleEvent_::vehicleId(2);
const obx::Property<VehicleEvent, OBXPropertyType_Long>   VehicleEvent_::ts(3);
const obx::Property<VehicleEvent, OBXPropertyType_String> VehicleEvent_::severity(6);

const obx::Property<ExtensionPayload, OBXPropertyType_Long>   ExtensionPayload_::id(1);
const obx::Property<ExtensionPayload, OBXPropertyType_String> ExtensionPayload_::vehicleId(2);
const obx::Property<ExtensionPayload, OBXPropertyType_Long>   ExtensionPayload_::ts(3);

// ── VehicleMeta (entity 10, 16 props) ────────────────────────────────────────
void VehicleMeta::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const VehicleMeta& o) {
    fbb.Clear();
    auto s_vehicleId       = fbb.CreateString(o.vehicleId);
    auto s_vin             = fbb.CreateString(o.vin);
    auto s_oem             = fbb.CreateString(o.oem);
    auto s_modelName       = fbb.CreateString(o.modelName);
    auto s_platform        = fbb.CreateString(o.platform);
    auto s_softwareVersion = fbb.CreateString(o.softwareVersion);
    auto s_powertrainType  = fbb.CreateString(o.powertrainType);
    auto s_drivetrainType  = fbb.CreateString(o.drivetrainType);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddOffset(8,  s_vin);
    fbb.AddOffset(10, s_oem);
    fbb.AddOffset(12, s_modelName);
    fbb.AddOffset(14, s_platform);
    fbb.AddOffset(16, s_softwareVersion);
    fbb.AddElement<int64_t>(18, o.createdAt);
    fbb.AddElement<int64_t>(20, o.updatedAt);
    fbb.AddElement<int64_t>(22, o.syncClock);
    fbb.AddElement<float>(24,   o.fuelTankCapacityL);
    fbb.AddElement<float>(26,   o.batteryCapacityKwh);
    fbb.AddElement<int32_t>(28, o.wheelbaseMm);
    fbb.AddElement<int32_t>(30, o.curbWeightKg);
    fbb.AddOffset(32, s_powertrainType);
    fbb.AddOffset(34, s_drivetrainType);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void VehicleMeta::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, VehicleMeta& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id                  = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    read_str(t, 8,  o.vin);
    read_str(t, 10, o.oem);
    read_str(t, 12, o.modelName);
    read_str(t, 14, o.platform);
    read_str(t, 16, o.softwareVersion);
    o.createdAt           = t->GetField<int64_t>(18, 0);
    o.updatedAt           = t->GetField<int64_t>(20, 0);
    o.syncClock           = t->GetField<int64_t>(22, 0);
    o.fuelTankCapacityL   = t->GetField<float>(24, 0.f);
    o.batteryCapacityKwh  = t->GetField<float>(26, 0.f);
    o.wheelbaseMm         = t->GetField<int32_t>(28, 0);
    o.curbWeightKg        = t->GetField<int32_t>(30, 0);
    read_str(t, 32, o.powertrainType);
    read_str(t, 34, o.drivetrainType);
}
VehicleMeta VehicleMeta::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { VehicleMeta o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<VehicleMeta> VehicleMeta::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<VehicleMeta>(); fromFlatBuffer(d,s,*o); return o; }

// ── SignalDefinition (entity 11, 14 props) ────────────────────────────────────
void SignalDefinition::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const SignalDefinition& o) {
    fbb.Clear();
    auto s_vssPath      = fbb.CreateString(o.vssPath);
    auto s_component    = fbb.CreateString(o.component);
    auto s_signalKind   = fbb.CreateString(o.signalKind);
    auto s_valueType    = fbb.CreateString(o.valueType);
    auto s_unit         = fbb.CreateString(o.unit);
    auto s_latestGroup  = fbb.CreateString(o.latestGroup);
    auto s_historyGroup = fbb.CreateString(o.historyGroup);
    auto s_historyMode  = fbb.CreateString(o.historyMode);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vssPath);
    fbb.AddOffset(8,  s_component);
    fbb.AddOffset(10, s_signalKind);
    fbb.AddOffset(12, s_valueType);
    fbb.AddOffset(14, s_unit);
    fbb.AddElement<uint8_t>(16, o.writable ? 1u : 0u);
    fbb.AddOffset(18, s_latestGroup);
    fbb.AddOffset(20, s_historyGroup);
    fbb.AddOffset(22, s_historyMode);
    fbb.AddElement<int32_t>(24, o.samplePeriodMs);
    fbb.AddElement<int32_t>(26, o.retainHours);
    fbb.AddElement<uint8_t>(28, o.enabled ? 1u : 0u);
    fbb.AddElement<int64_t>(30, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void SignalDefinition::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, SignalDefinition& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id             = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vssPath);
    read_str(t, 8,  o.component);
    read_str(t, 10, o.signalKind);
    read_str(t, 12, o.valueType);
    read_str(t, 14, o.unit);
    o.writable       = t->GetField<uint8_t>(16, 0) != 0;
    read_str(t, 18, o.latestGroup);
    read_str(t, 20, o.historyGroup);
    read_str(t, 22, o.historyMode);
    o.samplePeriodMs = t->GetField<int32_t>(24, 0);
    o.retainHours    = t->GetField<int32_t>(26, 0);
    o.enabled        = t->GetField<uint8_t>(28, 0) != 0;
    o.syncClock      = t->GetField<int64_t>(30, 0);
}
SignalDefinition SignalDefinition::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { SignalDefinition o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<SignalDefinition> SignalDefinition::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<SignalDefinition>(); fromFlatBuffer(d,s,*o); return o; }

// ── PowertrainState (entity 13, 14 props) ────────────────────────────────────
void PowertrainState::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const PowertrainState& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_tripId    = fbb.CreateString(o.tripId);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.updatedAt);
    fbb.AddElement<float>(10,   o.speedKph);
    fbb.AddElement<float>(12,   o.engineRpm);
    fbb.AddElement<float>(14,   o.odometerKm);
    fbb.AddElement<float>(16,   o.fuelLevelPct);
    fbb.AddElement<float>(18,   o.fuelRateLph);
    fbb.AddElement<float>(20,   o.coolantTempC);
    fbb.AddElement<float>(22,   o.throttlePct);
    fbb.AddElement<int32_t>(24, o.gear);
    fbb.AddElement<uint8_t>(26, o.ignitionOn ? 1u : 0u);
    fbb.AddOffset(28, s_tripId);
    fbb.AddElement<int64_t>(30, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void PowertrainState::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, PowertrainState& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id          = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.updatedAt   = t->GetField<int64_t>(8, 0);
    o.speedKph    = t->GetField<float>(10, 0.f);
    o.engineRpm   = t->GetField<float>(12, 0.f);
    o.odometerKm  = t->GetField<float>(14, 0.f);
    o.fuelLevelPct= t->GetField<float>(16, 0.f);
    o.fuelRateLph = t->GetField<float>(18, 0.f);
    o.coolantTempC= t->GetField<float>(20, 0.f);
    o.throttlePct = t->GetField<float>(22, 0.f);
    o.gear        = t->GetField<int32_t>(24, 0);
    o.ignitionOn  = t->GetField<uint8_t>(26, 0) != 0;
    read_str(t, 28, o.tripId);
    o.syncClock   = t->GetField<int64_t>(30, 0);
}
PowertrainState PowertrainState::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { PowertrainState o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<PowertrainState> PowertrainState::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<PowertrainState>(); fromFlatBuffer(d,s,*o); return o; }

// ── BatteryState (entity 14, 12 props) ───────────────────────────────────────
void BatteryState::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const BatteryState& o) {
    fbb.Clear();
    auto s_vehicleId     = fbb.CreateString(o.vehicleId);
    auto s_chargingState = fbb.CreateString(o.chargingState);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.updatedAt);
    fbb.AddElement<float>(10,   o.socPct);
    fbb.AddElement<float>(12,   o.sohPct);
    fbb.AddElement<float>(14,   o.batteryTempC);
    fbb.AddOffset(16, s_chargingState);
    fbb.AddElement<float>(18,   o.chargingPowerKw);
    fbb.AddElement<float>(20,   o.estimatedRangeKm);
    fbb.AddElement<float>(22,   o.voltageV);
    fbb.AddElement<float>(24,   o.currentA);
    fbb.AddElement<int64_t>(26, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void BatteryState::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, BatteryState& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id               = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.updatedAt        = t->GetField<int64_t>(8, 0);
    o.socPct           = t->GetField<float>(10, 0.f);
    o.sohPct           = t->GetField<float>(12, 0.f);
    o.batteryTempC     = t->GetField<float>(14, 0.f);
    read_str(t, 16, o.chargingState);
    o.chargingPowerKw  = t->GetField<float>(18, 0.f);
    o.estimatedRangeKm = t->GetField<float>(20, 0.f);
    o.voltageV         = t->GetField<float>(22, 0.f);
    o.currentA         = t->GetField<float>(24, 0.f);
    o.syncClock        = t->GetField<int64_t>(26, 0);
}
BatteryState BatteryState::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { BatteryState o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<BatteryState> BatteryState::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<BatteryState>(); fromFlatBuffer(d,s,*o); return o; }

// ── ChassisState (entity 15, 12 props) ───────────────────────────────────────
void ChassisState::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ChassisState& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.updatedAt);
    fbb.AddElement<float>(10,   o.steeringAngleDeg);
    fbb.AddElement<float>(12,   o.brakePedalPct);
    fbb.AddElement<float>(14,   o.tirePressureFlKpa);
    fbb.AddElement<float>(16,   o.tirePressureFrKpa);
    fbb.AddElement<float>(18,   o.tirePressureRlKpa);
    fbb.AddElement<float>(20,   o.tirePressureRrKpa);
    fbb.AddElement<uint8_t>(22, o.absActive ? 1u : 0u);
    fbb.AddElement<uint8_t>(24, o.tractionControlActive ? 1u : 0u);
    fbb.AddElement<int64_t>(26, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void ChassisState::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, ChassisState& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id                    = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.updatedAt             = t->GetField<int64_t>(8, 0);
    o.steeringAngleDeg      = t->GetField<float>(10, 0.f);
    o.brakePedalPct         = t->GetField<float>(12, 0.f);
    o.tirePressureFlKpa     = t->GetField<float>(14, 0.f);
    o.tirePressureFrKpa     = t->GetField<float>(16, 0.f);
    o.tirePressureRlKpa     = t->GetField<float>(18, 0.f);
    o.tirePressureRrKpa     = t->GetField<float>(20, 0.f);
    o.absActive             = t->GetField<uint8_t>(22, 0) != 0;
    o.tractionControlActive = t->GetField<uint8_t>(24, 0) != 0;
    o.syncClock             = t->GetField<int64_t>(26, 0);
}
ChassisState ChassisState::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { ChassisState o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<ChassisState> ChassisState::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<ChassisState>(); fromFlatBuffer(d,s,*o); return o; }

// ── CabinState (entity 16, 14 props) ─────────────────────────────────────────
void CabinState::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const CabinState& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_hvacMode  = fbb.CreateString(o.hvacMode);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.updatedAt);
    fbb.AddElement<float>(10,   o.insideTempC);
    fbb.AddElement<float>(12,   o.outsideTempC);
    fbb.AddOffset(14, s_hvacMode);
    fbb.AddElement<int32_t>(16, o.fanSpeed);
    fbb.AddElement<uint8_t>(18, o.driverDoorOpen ? 1u : 0u);
    fbb.AddElement<uint8_t>(20, o.passengerDoorOpen ? 1u : 0u);
    fbb.AddElement<uint8_t>(22, o.rearLeftDoorOpen ? 1u : 0u);
    fbb.AddElement<uint8_t>(24, o.rearRightDoorOpen ? 1u : 0u);
    fbb.AddElement<uint8_t>(26, o.doorsLocked ? 1u : 0u);
    fbb.AddElement<uint8_t>(28, o.seatbeltDriverFastened ? 1u : 0u);
    fbb.AddElement<int64_t>(30, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void CabinState::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, CabinState& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id                    = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.updatedAt             = t->GetField<int64_t>(8, 0);
    o.insideTempC           = t->GetField<float>(10, 0.f);
    o.outsideTempC          = t->GetField<float>(12, 0.f);
    read_str(t, 14, o.hvacMode);
    o.fanSpeed              = t->GetField<int32_t>(16, 0);
    o.driverDoorOpen        = t->GetField<uint8_t>(18, 0) != 0;
    o.passengerDoorOpen     = t->GetField<uint8_t>(20, 0) != 0;
    o.rearLeftDoorOpen      = t->GetField<uint8_t>(22, 0) != 0;
    o.rearRightDoorOpen     = t->GetField<uint8_t>(24, 0) != 0;
    o.doorsLocked           = t->GetField<uint8_t>(26, 0) != 0;
    o.seatbeltDriverFastened= t->GetField<uint8_t>(28, 0) != 0;
    o.syncClock             = t->GetField<int64_t>(30, 0);
}
CabinState CabinState::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { CabinState o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<CabinState> CabinState::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<CabinState>(); fromFlatBuffer(d,s,*o); return o; }

// ── LocationState (entity 17, 11 props) ──────────────────────────────────────
void LocationState::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const LocationState& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_geohash   = fbb.CreateString(o.geohash);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.updatedAt);
    fbb.AddElement<double>(10,  o.latitude);
    fbb.AddElement<double>(12,  o.longitude);
    fbb.AddElement<float>(14,   o.altitudeM);
    fbb.AddElement<float>(16,   o.headingDeg);
    fbb.AddElement<float>(18,   o.speedKph);
    fbb.AddElement<float>(20,   o.accuracyM);
    fbb.AddOffset(22, s_geohash);
    fbb.AddElement<int64_t>(24, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void LocationState::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, LocationState& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id        = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.updatedAt = t->GetField<int64_t>(8, 0);
    o.latitude  = t->GetField<double>(10, 0.0);
    o.longitude = t->GetField<double>(12, 0.0);
    o.altitudeM = t->GetField<float>(14, 0.f);
    o.headingDeg= t->GetField<float>(16, 0.f);
    o.speedKph  = t->GetField<float>(18, 0.f);
    o.accuracyM = t->GetField<float>(20, 0.f);
    read_str(t, 22, o.geohash);
    o.syncClock = t->GetField<int64_t>(24, 0);
}
LocationState LocationState::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { LocationState o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<LocationState> LocationState::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<LocationState>(); fromFlatBuffer(d,s,*o); return o; }

// ── AdasState (entity 18, 10 props) ──────────────────────────────────────────
void AdasState::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const AdasState& o) {
    fbb.Clear();
    auto s_vehicleId     = fbb.CreateString(o.vehicleId);
    auto s_autopilotMode = fbb.CreateString(o.autopilotMode);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.updatedAt);
    fbb.AddElement<uint8_t>(10, o.cruiseEnabled ? 1u : 0u);
    fbb.AddElement<float>(12,   o.cruiseSetSpeedKph);
    fbb.AddElement<uint8_t>(14, o.laneKeepAssistOn ? 1u : 0u);
    fbb.AddElement<uint8_t>(16, o.parkingAssistOn ? 1u : 0u);
    fbb.AddElement<uint8_t>(18, o.collisionWarningActive ? 1u : 0u);
    fbb.AddOffset(20, s_autopilotMode);
    fbb.AddElement<int64_t>(22, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void AdasState::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, AdasState& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id                    = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.updatedAt             = t->GetField<int64_t>(8, 0);
    o.cruiseEnabled         = t->GetField<uint8_t>(10, 0) != 0;
    o.cruiseSetSpeedKph     = t->GetField<float>(12, 0.f);
    o.laneKeepAssistOn      = t->GetField<uint8_t>(14, 0) != 0;
    o.parkingAssistOn       = t->GetField<uint8_t>(16, 0) != 0;
    o.collisionWarningActive= t->GetField<uint8_t>(18, 0) != 0;
    read_str(t, 20, o.autopilotMode);
    o.syncClock             = t->GetField<int64_t>(22, 0);
}
AdasState AdasState::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { AdasState o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<AdasState> AdasState::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<AdasState>(); fromFlatBuffer(d,s,*o); return o; }

// ── PowertrainSample (entity 19, 12 props) ────────────────────────────────────
void PowertrainSample::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const PowertrainSample& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_tripId    = fbb.CreateString(o.tripId);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_tripId);
    fbb.AddElement<float>(12,   o.speedKph);
    fbb.AddElement<float>(14,   o.engineRpm);
    fbb.AddElement<float>(16,   o.fuelLevelPct);
    fbb.AddElement<float>(18,   o.fuelRateLph);
    fbb.AddElement<float>(20,   o.coolantTempC);
    fbb.AddElement<float>(22,   o.throttlePct);
    fbb.AddElement<int32_t>(24, o.gear);
    fbb.AddElement<int64_t>(26, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void PowertrainSample::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, PowertrainSample& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id          = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts          = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.tripId);
    o.speedKph    = t->GetField<float>(12, 0.f);
    o.engineRpm   = t->GetField<float>(14, 0.f);
    o.fuelLevelPct= t->GetField<float>(16, 0.f);
    o.fuelRateLph = t->GetField<float>(18, 0.f);
    o.coolantTempC= t->GetField<float>(20, 0.f);
    o.throttlePct = t->GetField<float>(22, 0.f);
    o.gear        = t->GetField<int32_t>(24, 0);
    o.syncClock   = t->GetField<int64_t>(26, 0);
}
PowertrainSample PowertrainSample::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { PowertrainSample o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<PowertrainSample> PowertrainSample::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<PowertrainSample>(); fromFlatBuffer(d,s,*o); return o; }

// ── BatterySample (entity 20, 12 props) ───────────────────────────────────────
void BatterySample::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const BatterySample& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_tripId    = fbb.CreateString(o.tripId);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_tripId);
    fbb.AddElement<float>(12,   o.socPct);
    fbb.AddElement<float>(14,   o.sohPct);
    fbb.AddElement<float>(16,   o.batteryTempC);
    fbb.AddElement<float>(18,   o.chargingPowerKw);
    fbb.AddElement<float>(20,   o.estimatedRangeKm);
    fbb.AddElement<float>(22,   o.voltageV);
    fbb.AddElement<float>(24,   o.currentA);
    fbb.AddElement<int64_t>(26, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void BatterySample::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, BatterySample& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id               = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts               = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.tripId);
    o.socPct           = t->GetField<float>(12, 0.f);
    o.sohPct           = t->GetField<float>(14, 0.f);
    o.batteryTempC     = t->GetField<float>(16, 0.f);
    o.chargingPowerKw  = t->GetField<float>(18, 0.f);
    o.estimatedRangeKm = t->GetField<float>(20, 0.f);
    o.voltageV         = t->GetField<float>(22, 0.f);
    o.currentA         = t->GetField<float>(24, 0.f);
    o.syncClock        = t->GetField<int64_t>(26, 0);
}
BatterySample BatterySample::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { BatterySample o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<BatterySample> BatterySample::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<BatterySample>(); fromFlatBuffer(d,s,*o); return o; }

// ── LocationSample (entity 21, 11 props) ──────────────────────────────────────
void LocationSample::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const LocationSample& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_tripId    = fbb.CreateString(o.tripId);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_tripId);
    fbb.AddElement<double>(12,  o.latitude);
    fbb.AddElement<double>(14,  o.longitude);
    fbb.AddElement<float>(16,   o.altitudeM);
    fbb.AddElement<float>(18,   o.headingDeg);
    fbb.AddElement<float>(20,   o.speedKph);
    fbb.AddElement<float>(22,   o.accuracyM);
    fbb.AddElement<int64_t>(24, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void LocationSample::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, LocationSample& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id        = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts        = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.tripId);
    o.latitude  = t->GetField<double>(12, 0.0);
    o.longitude = t->GetField<double>(14, 0.0);
    o.altitudeM = t->GetField<float>(16, 0.f);
    o.headingDeg= t->GetField<float>(18, 0.f);
    o.speedKph  = t->GetField<float>(20, 0.f);
    o.accuracyM = t->GetField<float>(22, 0.f);
    o.syncClock = t->GetField<int64_t>(24, 0);
}
LocationSample LocationSample::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { LocationSample o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<LocationSample> LocationSample::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<LocationSample>(); fromFlatBuffer(d,s,*o); return o; }

// ── CabinSample (entity 22, 9 props) ─────────────────────────────────────────
void CabinSample::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const CabinSample& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_tripId    = fbb.CreateString(o.tripId);
    auto s_hvacMode  = fbb.CreateString(o.hvacMode);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_tripId);
    fbb.AddElement<float>(12,   o.insideTempC);
    fbb.AddElement<float>(14,   o.outsideTempC);
    fbb.AddOffset(16, s_hvacMode);
    fbb.AddElement<int32_t>(18, o.fanSpeed);
    fbb.AddElement<int64_t>(20, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void CabinSample::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, CabinSample& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id         = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts         = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.tripId);
    o.insideTempC = t->GetField<float>(12, 0.f);
    o.outsideTempC= t->GetField<float>(14, 0.f);
    read_str(t, 16, o.hvacMode);
    o.fanSpeed   = t->GetField<int32_t>(18, 0);
    o.syncClock  = t->GetField<int64_t>(20, 0);
}
CabinSample CabinSample::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { CabinSample o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<CabinSample> CabinSample::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<CabinSample>(); fromFlatBuffer(d,s,*o); return o; }

// ── AdasSample (entity 23, 9 props) ──────────────────────────────────────────
void AdasSample::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const AdasSample& o) {
    fbb.Clear();
    auto s_vehicleId = fbb.CreateString(o.vehicleId);
    auto s_tripId    = fbb.CreateString(o.tripId);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_tripId);
    fbb.AddElement<uint8_t>(12, o.cruiseEnabled ? 1u : 0u);
    fbb.AddElement<float>(14,   o.cruiseSetSpeedKph);
    fbb.AddElement<uint8_t>(16, o.laneKeepAssistOn ? 1u : 0u);
    fbb.AddElement<uint8_t>(18, o.collisionWarningActive ? 1u : 0u);
    fbb.AddElement<int64_t>(20, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void AdasSample::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, AdasSample& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id                    = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts                    = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.tripId);
    o.cruiseEnabled         = t->GetField<uint8_t>(12, 0) != 0;
    o.cruiseSetSpeedKph     = t->GetField<float>(14, 0.f);
    o.laneKeepAssistOn      = t->GetField<uint8_t>(16, 0) != 0;
    o.collisionWarningActive= t->GetField<uint8_t>(18, 0) != 0;
    o.syncClock             = t->GetField<int64_t>(20, 0);
}
AdasSample AdasSample::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { AdasSample o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<AdasSample> AdasSample::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<AdasSample>(); fromFlatBuffer(d,s,*o); return o; }

// ── VehicleEvent (entity 24, 11 props) ────────────────────────────────────────
void VehicleEvent::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const VehicleEvent& o) {
    fbb.Clear();
    auto s_vehicleId   = fbb.CreateString(o.vehicleId);
    auto s_tripId      = fbb.CreateString(o.tripId);
    auto s_eventType   = fbb.CreateString(o.eventType);
    auto s_severity    = fbb.CreateString(o.severity);
    auto s_vssPath     = fbb.CreateString(o.vssPath);
    auto s_code        = fbb.CreateString(o.code);
    auto s_description = fbb.CreateString(o.description);
    auto s_payloadJson = fbb.CreateString(o.payloadJson);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_tripId);
    fbb.AddOffset(12, s_eventType);
    fbb.AddOffset(14, s_severity);
    fbb.AddOffset(16, s_vssPath);
    fbb.AddOffset(18, s_code);
    fbb.AddOffset(20, s_description);
    fbb.AddOffset(22, s_payloadJson);
    fbb.AddElement<int64_t>(24, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void VehicleEvent::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, VehicleEvent& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id        = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts        = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.tripId);
    read_str(t, 12, o.eventType);
    read_str(t, 14, o.severity);
    read_str(t, 16, o.vssPath);
    read_str(t, 18, o.code);
    read_str(t, 20, o.description);
    read_str(t, 22, o.payloadJson);
    o.syncClock = t->GetField<int64_t>(24, 0);
}
VehicleEvent VehicleEvent::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { VehicleEvent o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<VehicleEvent> VehicleEvent::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<VehicleEvent>(); fromFlatBuffer(d,s,*o); return o; }

// ── ExtensionPayload (entity 25, 7 props) ────────────────────────────────────
void ExtensionPayload::_OBX_MetaInfo::toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ExtensionPayload& o) {
    fbb.Clear();
    auto s_vehicleId      = fbb.CreateString(o.vehicleId);
    auto s_component      = fbb.CreateString(o.component);
    auto s_schemaVersion  = fbb.CreateString(o.schemaVersion);
    auto s_payloadJson    = fbb.CreateString(o.payloadJson);
    auto start = fbb.StartTable();
    fbb.AddElement<int64_t>(4,  o.id);
    fbb.AddOffset(6,  s_vehicleId);
    fbb.AddElement<int64_t>(8,  o.ts);
    fbb.AddOffset(10, s_component);
    fbb.AddOffset(12, s_schemaVersion);
    fbb.AddOffset(14, s_payloadJson);
    fbb.AddElement<int64_t>(16, o.syncClock);
    flatbuffers::Offset<flatbuffers::Table> off; off.o = fbb.EndTable(start);
    fbb.Finish(off);
}
void ExtensionPayload::_OBX_MetaInfo::fromFlatBuffer(const void* data, size_t, ExtensionPayload& o) {
    const auto* t = flatbuffers::GetRoot<flatbuffers::Table>(data);
    o.id            = t->GetField<int64_t>(4, 0);
    read_str(t, 6,  o.vehicleId);
    o.ts            = t->GetField<int64_t>(8, 0);
    read_str(t, 10, o.component);
    read_str(t, 12, o.schemaVersion);
    read_str(t, 14, o.payloadJson);
    o.syncClock     = t->GetField<int64_t>(16, 0);
}
ExtensionPayload ExtensionPayload::_OBX_MetaInfo::fromFlatBuffer(const void* d, size_t s) { ExtensionPayload o; fromFlatBuffer(d,s,o); return o; }
std::unique_ptr<ExtensionPayload> ExtensionPayload::_OBX_MetaInfo::newFromFlatBuffer(const void* d, size_t s) { auto o = std::make_unique<ExtensionPayload>(); fromFlatBuffer(d,s,*o); return o; }
