// Manually generated ObjectBox VSS schema — entities 10-25
#pragma once

#include <cstdbool>
#include <cstdint>
#include <string>
#include <vector>

#include "flatbuffers/flatbuffers.h"
#include "objectbox.h"
#include "objectbox.hpp"

// ── Forward declarations ──────────────────────────────────────────────────────
struct VehicleMeta_;
struct SignalDefinition_;
struct VehicleAttributeState_;
struct PowertrainState_;
struct BatteryState_;
struct ChassisState_;
struct CabinState_;
struct LocationState_;
struct AdasState_;
struct PowertrainSample_;
struct BatterySample_;
struct LocationSample_;
struct CabinSample_;
struct AdasSample_;
struct VehicleEvent_;
struct ExtensionPayload_;

// ── Entity 10: VehicleMeta ────────────────────────────────────────────────────
struct VehicleMeta {
    int64_t     id = 0;               // P1
    std::string vehicleId;            // P2
    std::string vin;                  // P3
    std::string oem;                  // P4
    std::string modelName;            // P5  (property name "model" in schema)
    std::string platform;             // P6
    std::string softwareVersion;      // P7
    int64_t     createdAt = 0;        // P8
    int64_t     updatedAt = 0;        // P9
    int64_t     syncClock = 0;        // P10

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 10; }
        static void setObjectId(VehicleMeta& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const VehicleMeta& o);
        static VehicleMeta fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<VehicleMeta> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, VehicleMeta& out);
    };
};
struct VehicleMeta_ {
    static const obx::Property<VehicleMeta, OBXPropertyType_Long>   id;
    static const obx::Property<VehicleMeta, OBXPropertyType_String> vehicleId;
};

// ── Entity 11: SignalDefinition ───────────────────────────────────────────────
struct SignalDefinition {
    int64_t     id = 0;               // P1
    std::string vssPath;              // P2
    std::string component;            // P3
    std::string signalKind;           // P4
    std::string valueType;            // P5
    std::string unit;                 // P6
    bool        writable = false;     // P7
    std::string latestGroup;          // P8
    std::string historyGroup;         // P9
    std::string historyMode;          // P10
    int32_t     samplePeriodMs = 0;   // P11
    int32_t     retainHours = 0;      // P12
    bool        enabled = true;       // P13
    int64_t     syncClock = 0;        // P14

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 11; }
        static void setObjectId(SignalDefinition& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const SignalDefinition& o);
        static SignalDefinition fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<SignalDefinition> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, SignalDefinition& out);
    };
};
struct SignalDefinition_ {
    static const obx::Property<SignalDefinition, OBXPropertyType_Long> id;
};

// ── Entity 12: VehicleAttributeState ─────────────────────────────────────────
struct VehicleAttributeState {
    int64_t     id = 0;                   // P1
    std::string vehicleId;                // P2  indexed
    int64_t     updatedAt = 0;            // P3
    float       fuelTankCapacityL = 0.f;  // P4
    float       batteryCapacityKwh = 0.f; // P5
    int32_t     wheelbaseMm = 0;          // P6
    int32_t     curbWeightKg = 0;         // P7
    std::string powertrainType;           // P8
    std::string drivetrainType;           // P9
    int64_t     syncClock = 0;            // P10

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 12; }
        static void setObjectId(VehicleAttributeState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const VehicleAttributeState& o);
        static VehicleAttributeState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<VehicleAttributeState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, VehicleAttributeState& out);
    };
};
struct VehicleAttributeState_ {
    static const obx::Property<VehicleAttributeState, OBXPropertyType_Long>   id;
    static const obx::Property<VehicleAttributeState, OBXPropertyType_String> vehicleId;
};

// ── Entity 13: PowertrainState ────────────────────────────────────────────────
struct PowertrainState {
    int64_t     id = 0;              // P1
    std::string vehicleId;           // P2  indexed
    int64_t     updatedAt = 0;       // P3
    float       speedKph = 0.f;      // P4
    float       engineRpm = 0.f;     // P5
    float       odometerKm = 0.f;    // P6
    float       fuelLevelPct = 0.f;  // P7
    float       fuelRateLph = 0.f;   // P8
    float       coolantTempC = 0.f;  // P9
    float       throttlePct = 0.f;   // P10
    int32_t     gear = 0;            // P11
    bool        ignitionOn = false;  // P12
    std::string tripId;              // P13
    int64_t     syncClock = 0;       // P14

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 13; }
        static void setObjectId(PowertrainState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const PowertrainState& o);
        static PowertrainState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<PowertrainState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, PowertrainState& out);
    };
};
struct PowertrainState_ {
    static const obx::Property<PowertrainState, OBXPropertyType_Long>   id;
    static const obx::Property<PowertrainState, OBXPropertyType_String> vehicleId;
};

// ── Entity 14: BatteryState ───────────────────────────────────────────────────
struct BatteryState {
    int64_t     id = 0;                    // P1
    std::string vehicleId;                 // P2  indexed
    int64_t     updatedAt = 0;             // P3
    float       socPct = 0.f;             // P4
    float       sohPct = 0.f;             // P5
    float       batteryTempC = 0.f;       // P6
    std::string chargingState;            // P7
    float       chargingPowerKw = 0.f;    // P8
    float       estimatedRangeKm = 0.f;   // P9
    float       voltageV = 0.f;           // P10
    float       currentA = 0.f;           // P11
    int64_t     syncClock = 0;            // P12

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 14; }
        static void setObjectId(BatteryState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const BatteryState& o);
        static BatteryState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<BatteryState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, BatteryState& out);
    };
};
struct BatteryState_ {
    static const obx::Property<BatteryState, OBXPropertyType_Long>   id;
    static const obx::Property<BatteryState, OBXPropertyType_String> vehicleId;
};

// ── Entity 15: ChassisState ───────────────────────────────────────────────────
struct ChassisState {
    int64_t id = 0;                         // P1
    std::string vehicleId;                  // P2  indexed
    int64_t     updatedAt = 0;              // P3
    float       steeringAngleDeg = 0.f;    // P4
    float       brakePedalPct = 0.f;       // P5
    float       tirePressureFlKpa = 0.f;   // P6
    float       tirePressureFrKpa = 0.f;   // P7
    float       tirePressureRlKpa = 0.f;   // P8
    float       tirePressureRrKpa = 0.f;   // P9
    bool        absActive = false;          // P10
    bool        tractionControlActive = false; // P11
    int64_t     syncClock = 0;             // P12

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 15; }
        static void setObjectId(ChassisState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ChassisState& o);
        static ChassisState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<ChassisState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, ChassisState& out);
    };
};
struct ChassisState_ {
    static const obx::Property<ChassisState, OBXPropertyType_Long>   id;
    static const obx::Property<ChassisState, OBXPropertyType_String> vehicleId;
};

// ── Entity 16: CabinState ─────────────────────────────────────────────────────
struct CabinState {
    int64_t     id = 0;                       // P1
    std::string vehicleId;                    // P2  indexed
    int64_t     updatedAt = 0;                // P3
    float       insideTempC = 0.f;           // P4
    float       outsideTempC = 0.f;          // P5
    std::string hvacMode;                    // P6
    int32_t     fanSpeed = 0;                // P7
    bool        driverDoorOpen = false;      // P8
    bool        passengerDoorOpen = false;   // P9
    bool        rearLeftDoorOpen = false;    // P10
    bool        rearRightDoorOpen = false;   // P11
    bool        doorsLocked = true;          // P12
    bool        seatbeltDriverFastened = false; // P13
    int64_t     syncClock = 0;               // P14

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 16; }
        static void setObjectId(CabinState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const CabinState& o);
        static CabinState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<CabinState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, CabinState& out);
    };
};
struct CabinState_ {
    static const obx::Property<CabinState, OBXPropertyType_Long>   id;
    static const obx::Property<CabinState, OBXPropertyType_String> vehicleId;
};

// ── Entity 17: LocationState ──────────────────────────────────────────────────
struct LocationState {
    int64_t     id = 0;             // P1
    std::string vehicleId;          // P2  indexed
    int64_t     updatedAt = 0;      // P3
    double      latitude = 0.0;     // P4
    double      longitude = 0.0;    // P5
    float       altitudeM = 0.f;   // P6
    float       headingDeg = 0.f;  // P7
    float       speedKph = 0.f;    // P8
    float       accuracyM = 0.f;   // P9
    std::string geohash;            // P10
    int64_t     syncClock = 0;      // P11

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 17; }
        static void setObjectId(LocationState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const LocationState& o);
        static LocationState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<LocationState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, LocationState& out);
    };
};
struct LocationState_ {
    static const obx::Property<LocationState, OBXPropertyType_Long>   id;
    static const obx::Property<LocationState, OBXPropertyType_String> vehicleId;
};

// ── Entity 18: AdasState ─────────────────────────────────────────────────────
struct AdasState {
    int64_t     id = 0;                         // P1
    std::string vehicleId;                      // P2  indexed
    int64_t     updatedAt = 0;                  // P3
    bool        cruiseEnabled = false;          // P4
    float       cruiseSetSpeedKph = 0.f;       // P5
    bool        laneKeepAssistOn = false;       // P6
    bool        parkingAssistOn = false;        // P7
    bool        collisionWarningActive = false; // P8
    std::string autopilotMode;                  // P9
    int64_t     syncClock = 0;                  // P10

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 18; }
        static void setObjectId(AdasState& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const AdasState& o);
        static AdasState fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<AdasState> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, AdasState& out);
    };
};
struct AdasState_ {
    static const obx::Property<AdasState, OBXPropertyType_Long>   id;
    static const obx::Property<AdasState, OBXPropertyType_String> vehicleId;
};

// ── Entity 19: PowertrainSample ───────────────────────────────────────────────
struct PowertrainSample {
    int64_t     id = 0;              // P1
    std::string vehicleId;           // P2  indexed
    int64_t     ts = 0;              // P3  indexed
    std::string tripId;              // P4
    float       speedKph = 0.f;      // P5
    float       engineRpm = 0.f;     // P6
    float       fuelLevelPct = 0.f;  // P7
    float       fuelRateLph = 0.f;   // P8
    float       coolantTempC = 0.f;  // P9
    float       throttlePct = 0.f;   // P10
    int32_t     gear = 0;            // P11
    int64_t     syncClock = 0;       // P12

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 19; }
        static void setObjectId(PowertrainSample& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const PowertrainSample& o);
        static PowertrainSample fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<PowertrainSample> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, PowertrainSample& out);
    };
};
struct PowertrainSample_ {
    static const obx::Property<PowertrainSample, OBXPropertyType_Long>   id;
    static const obx::Property<PowertrainSample, OBXPropertyType_String> vehicleId;
    static const obx::Property<PowertrainSample, OBXPropertyType_Long>   ts;
};

// ── Entity 20: BatterySample ──────────────────────────────────────────────────
struct BatterySample {
    int64_t     id = 0;                   // P1
    std::string vehicleId;                // P2  indexed
    int64_t     ts = 0;                   // P3  indexed
    std::string tripId;                   // P4
    float       socPct = 0.f;            // P5
    float       sohPct = 0.f;            // P6
    float       batteryTempC = 0.f;      // P7
    float       chargingPowerKw = 0.f;   // P8
    float       estimatedRangeKm = 0.f;  // P9
    float       voltageV = 0.f;          // P10
    float       currentA = 0.f;          // P11
    int64_t     syncClock = 0;           // P12

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 20; }
        static void setObjectId(BatterySample& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const BatterySample& o);
        static BatterySample fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<BatterySample> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, BatterySample& out);
    };
};
struct BatterySample_ {
    static const obx::Property<BatterySample, OBXPropertyType_Long>   id;
    static const obx::Property<BatterySample, OBXPropertyType_String> vehicleId;
    static const obx::Property<BatterySample, OBXPropertyType_Long>   ts;
};

// ── Entity 21: LocationSample ─────────────────────────────────────────────────
struct LocationSample {
    int64_t     id = 0;           // P1
    std::string vehicleId;        // P2  indexed
    int64_t     ts = 0;           // P3  indexed
    std::string tripId;           // P4
    double      latitude = 0.0;   // P5
    double      longitude = 0.0;  // P6
    float       altitudeM = 0.f; // P7
    float       headingDeg = 0.f;// P8
    float       speedKph = 0.f;  // P9
    float       accuracyM = 0.f; // P10
    int64_t     syncClock = 0;    // P11

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 21; }
        static void setObjectId(LocationSample& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const LocationSample& o);
        static LocationSample fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<LocationSample> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, LocationSample& out);
    };
};
struct LocationSample_ {
    static const obx::Property<LocationSample, OBXPropertyType_Long>   id;
    static const obx::Property<LocationSample, OBXPropertyType_String> vehicleId;
    static const obx::Property<LocationSample, OBXPropertyType_Long>   ts;
};

// ── Entity 22: CabinSample ────────────────────────────────────────────────────
struct CabinSample {
    int64_t     id = 0;               // P1
    std::string vehicleId;            // P2  indexed
    int64_t     ts = 0;               // P3  indexed
    std::string tripId;               // P4
    float       insideTempC = 0.f;   // P5
    float       outsideTempC = 0.f;  // P6
    std::string hvacMode;             // P7
    int32_t     fanSpeed = 0;         // P8
    int64_t     syncClock = 0;        // P9

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 22; }
        static void setObjectId(CabinSample& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const CabinSample& o);
        static CabinSample fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<CabinSample> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, CabinSample& out);
    };
};
struct CabinSample_ {
    static const obx::Property<CabinSample, OBXPropertyType_Long>   id;
    static const obx::Property<CabinSample, OBXPropertyType_String> vehicleId;
    static const obx::Property<CabinSample, OBXPropertyType_Long>   ts;
};

// ── Entity 23: AdasSample ─────────────────────────────────────────────────────
struct AdasSample {
    int64_t     id = 0;                         // P1
    std::string vehicleId;                      // P2  indexed
    int64_t     ts = 0;                         // P3  indexed
    std::string tripId;                         // P4
    bool        cruiseEnabled = false;          // P5
    float       cruiseSetSpeedKph = 0.f;       // P6
    bool        laneKeepAssistOn = false;       // P7
    bool        collisionWarningActive = false; // P8
    int64_t     syncClock = 0;                  // P9

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 23; }
        static void setObjectId(AdasSample& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const AdasSample& o);
        static AdasSample fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<AdasSample> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, AdasSample& out);
    };
};
struct AdasSample_ {
    static const obx::Property<AdasSample, OBXPropertyType_Long>   id;
    static const obx::Property<AdasSample, OBXPropertyType_String> vehicleId;
    static const obx::Property<AdasSample, OBXPropertyType_Long>   ts;
};

// ── Entity 24: VehicleEvent ───────────────────────────────────────────────────
struct VehicleEvent {
    int64_t     id = 0;           // P1
    std::string vehicleId;        // P2  indexed
    int64_t     ts = 0;           // P3  indexed
    std::string tripId;           // P4
    std::string eventType;        // P5
    std::string severity;         // P6
    std::string vssPath;          // P7
    std::string code;             // P8
    std::string description;      // P9
    std::string payloadJson;      // P10
    int64_t     syncClock = 0;    // P11

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 24; }
        static void setObjectId(VehicleEvent& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const VehicleEvent& o);
        static VehicleEvent fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<VehicleEvent> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, VehicleEvent& out);
    };
};
struct VehicleEvent_ {
    static const obx::Property<VehicleEvent, OBXPropertyType_Long>   id;
    static const obx::Property<VehicleEvent, OBXPropertyType_String> vehicleId;
    static const obx::Property<VehicleEvent, OBXPropertyType_Long>   ts;
    static const obx::Property<VehicleEvent, OBXPropertyType_String> severity;
};

// ── Entity 25: ExtensionPayload ───────────────────────────────────────────────
struct ExtensionPayload {
    int64_t     id = 0;               // P1
    std::string vehicleId;            // P2  indexed
    int64_t     ts = 0;               // P3  indexed
    std::string component;            // P4
    std::string schemaVersion;        // P5
    std::string payloadJson;          // P6
    int64_t     syncClock = 0;        // P7

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 25; }
        static void setObjectId(ExtensionPayload& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ExtensionPayload& o);
        static ExtensionPayload fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<ExtensionPayload> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, ExtensionPayload& out);
    };
};
struct ExtensionPayload_ {
    static const obx::Property<ExtensionPayload, OBXPropertyType_Long>   id;
    static const obx::Property<ExtensionPayload, OBXPropertyType_String> vehicleId;
    static const obx::Property<ExtensionPayload, OBXPropertyType_Long>   ts;
};
