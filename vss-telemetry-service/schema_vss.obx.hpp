// Manually generated ObjectBox VSS schema — sample entities + VehicleMeta + SignalDefinition
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
struct PowertrainSample_;
struct BatterySample_;
struct LocationSample_;
struct CabinSample_;
struct AdasSample_;
struct ChassisSample_;

// ── Entity 10: VehicleMeta ────────────────────────────────────────────────────
struct VehicleMeta {
    int64_t     id = 0;                    // P1
    std::string vehicleId;                 // P2
    std::string vin;                       // P3
    std::string oem;                       // P4
    std::string modelName;                 // P5  (property name "model" in schema)
    std::string platform;                  // P6
    std::string softwareVersion;           // P7
    int64_t     createdAt = 0;             // P8
    int64_t     updatedAt = 0;             // P9
    int64_t     syncClock = 0;             // P10
    float       fuelTankCapacityL = 0.f;   // P11
    float       batteryCapacityKwh = 0.f;  // P12
    int32_t     wheelbaseMm = 0;           // P13
    int32_t     curbWeightKg = 0;          // P14
    std::string powertrainType;            // P15
    std::string drivetrainType;            // P16

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
    float       altitudeM = 0.f; // P7
    float       headingDeg = 0.f;// P8
    float       speedKph = 0.f;  // P9
    float       accuracyM = 0.f; // P10
    int64_t     syncClock = 0;    // P11
    std::string locationGeoJson;  // P12

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

// ── Entity 24: ChassisSample ──────────────────────────────────────────────────
struct ChassisSample {
    int64_t     id = 0;                          // P1
    std::string vehicleId;                       // P2  indexed
    int64_t     ts = 0;                          // P3  indexed
    std::string tripId;                          // P4
    float       steeringAngleDeg = 0.f;         // P5
    float       brakePedalPct = 0.f;            // P6
    float       tirePressureFlKpa = 0.f;        // P7
    float       tirePressureFrKpa = 0.f;        // P8
    float       tirePressureRlKpa = 0.f;        // P9
    float       tirePressureRrKpa = 0.f;        // P10
    bool        absActive = false;               // P11
    bool        tractionControlActive = false;   // P12
    int64_t     syncClock = 0;                   // P13

    struct _OBX_MetaInfo {
        static constexpr obx_schema_id entityId() { return 24; }
        static void setObjectId(ChassisSample& o, obx_id v) { o.id = v; }
        static void toFlatBuffer(flatbuffers::FlatBufferBuilder& fbb, const ChassisSample& o);
        static ChassisSample fromFlatBuffer(const void* data, size_t size);
        static std::unique_ptr<ChassisSample> newFromFlatBuffer(const void* data, size_t size);
        static void fromFlatBuffer(const void* data, size_t size, ChassisSample& out);
    };
};
struct ChassisSample_ {
    static const obx::Property<ChassisSample, OBXPropertyType_Long>   id;
    static const obx::Property<ChassisSample, OBXPropertyType_String> vehicleId;
    static const obx::Property<ChassisSample, OBXPropertyType_Long>   ts;
};
