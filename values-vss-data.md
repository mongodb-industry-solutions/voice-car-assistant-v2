$defs:
  boolean_domain: &boolean_domain "boolean {true, false}"
  uint8_domain: &uint8_domain "uint8 [0..255]"
  int8_domain: &int8_domain "int8 [-128..127]"
  uint16_domain: &uint16_domain "uint16 [0..65535]"
  int16_domain: &int16_domain "int16 [-32768..32767]"
  uint32_domain: &uint32_domain "uint32 [0..4294967295]"
  int32_domain: &int32_domain "int32 [-2147483648..2147483647]"
  float_domain: &float_domain "float (any floating-point value unless constrained below)"
  double_domain: &double_domain "double (any floating-point value unless constrained below)"
  string_domain: &string_domain "string (free-form unless constrained below)"
  string_array_domain: &string_array_domain "string[] (array of free-form strings unless constrained below)"

  MovableItem: &MovableItem
    IsOpen: "boolean {true,false}"
    Position: "uint8 [0..100]"
    Switch: "string {'INACTIVE','CLOSE','OPEN','ONE_SHOT_CLOSE','ONE_SHOT_OPEN'}"

  LockableMovableItem: &LockableMovableItem
    IsLocked: "boolean {true,false}"
    IsOpen: "boolean {true,false}"
    Position: "uint8 [0..100]"
    Switch: "string {'INACTIVE','CLOSE','OPEN','ONE_SHOT_CLOSE','ONE_SHOT_OPEN'}"

  DoorWindowShade: &DoorWindowShade
    IsChildLockActive: "boolean {true,false}"
    IsLocked: "boolean {true,false}"
    IsOpen: "boolean {true,false}"
    Position: "uint8 [0..100]"
    Shade:
      <<: *MovableItem
    Switch: "string {'INACTIVE','CLOSE','OPEN','ONE_SHOT_CLOSE','ONE_SHOT_OPEN'}"
    Window:
      <<: *MovableItem

  SeatOccupantPosition: &SeatOccupantPosition
    Airbag:
      IsDeployed: *boolean_domain
      IsEnabled: *boolean_domain
    Backrest:
      BottomLumbarSupport: *float_domain
      IsLessLumbarSupportSwitchEngaged: *boolean_domain
      IsLessSideBolsterSupportSwitchEngaged: *boolean_domain
      IsLumbarDownSwitchEngaged: *boolean_domain
      IsLumbarUpSwitchEngaged: *boolean_domain
      IsMoreLumbarSupportSwitchEngaged: *boolean_domain
      IsMoreSideBolsterSupportSwitchEngaged: *boolean_domain
      IsReclineBackwardSwitchEngaged: *boolean_domain
      IsReclineForwardSwitchEngaged: *boolean_domain
      LumbarHeight: *uint8_domain
      LumbarSupport: *float_domain
      MidLumbarSupport: *float_domain
      Recline: *float_domain
      SideBolsterSupport: *float_domain
      SideBolsterSupportLeft: *float_domain
      SideBolsterSupportRight: *float_domain
      TopLumbarSupport: *float_domain
      UpperShoulderSupport: *float_domain
    Headrest:
      Angle: *float_domain
      Height: *uint8_domain
      IsBackwardSwitchEngaged: *boolean_domain
      IsDownSwitchEngaged: *boolean_domain
      IsForwardSwitchEngaged: *boolean_domain
      IsUpSwitchEngaged: *boolean_domain
    Height: *uint16_domain
    IsBackwardSwitchEngaged: *boolean_domain
    IsBelted: *boolean_domain
    IsCoolerSwitchEngaged: *boolean_domain
    IsDecreaseMassageLevelSwitchEngaged: *boolean_domain
    IsDownSwitchEngaged: *boolean_domain
    IsForwardSwitchEngaged: *boolean_domain
    IsIncreaseMassageLevelSwitchEngaged: *boolean_domain
    IsTiltBackwardSwitchEngaged: *boolean_domain
    IsTiltForwardSwitchEngaged: *boolean_domain
    IsUpSwitchEngaged: *boolean_domain
    IsWarmerSwitchEngaged: *boolean_domain
    Massage:
      IsAvailable: *boolean_domain
      Level: *uint8_domain
      Status: *string_domain
      SupportedTypes: *string_array_domain
      TypeActive: *string_domain
    NeckScarf: "branch (subsignals not expanded in current fetch)"
    OccupancyStatus: *string_domain
    Position: *uint16_domain
    SeatBeltHeight: *uint16_domain
    Seating:
      IsBackwardSwitchEngaged: *boolean_domain
      IsForwardSwitchEngaged: *boolean_domain
      Length: *uint16_domain
      SideBolsterSupportLeft: *float_domain
      SideBolsterSupportRight: *float_domain
    Tilt: *float_domain

  WheelSide: &WheelSide
    AngularSpeed: *float_domain
    Brake:
      FluidLevel: *uint8_domain
      IsBrakesWorn: *boolean_domain
      IsFluidLevelLow: *boolean_domain
      PadWear: *uint8_domain
    Speed: *float_domain
    Tire:
      AirTemperature: *float_domain
      IsPressureLow: *boolean_domain
      Pressure: *uint16_domain
      RubberTemperature: *float_domain
      Temperature: *float_domain
      WinterStatus: *string_domain
    Torque: *int16_domain

  ControlUnitNode: &ControlUnitNode
    Health:
      Network:
        CAN:
          IsNetworkOK: *boolean_domain
        ETH:
          IsNetworkOK: *boolean_domain
      Resources:
        Power: *float_domain
        Temperature: *float_domain
        Utilization:
          CPU: *float_domain
          Memory: *float_domain
      SWSupervision:
        IsAliveTriggered: *boolean_domain
        IsDeadlineTriggered: *boolean_domain
        IsLogicalTriggered: *boolean_domain
        IsWatchdogTriggered: *boolean_domain
    ID: "uint8 [0..255] (default 0)"

  OccupantNode: &OccupantNode
    HeadPosition:
      Pitch: *float_domain
      Roll: *float_domain
      X: *int16_domain
      Y: *int16_domain
      Yaw: *float_domain
      Z: *int16_domain
    Identifier:
      Issuer: *string_domain
      Subject: *string_domain
    MidEyeGaze:
      Azimuth: *float_domain
      Elevation: *float_domain

  ElectricMotorNode: &ElectricMotorNode
    EngineCode: *string_domain
    EngineCoolant:
      Capacity: *float_domain
      Level: *string_domain
      LifeRemaining: *int32_domain
      Temperature: *float_domain
    MaxPower: "uint16 [0..65535] (default 0)"
    MaxRegenPower: "uint16 [0..65535] (default 0)"
    MaxRegenTorque: "uint16 [0..65535] (default 0)"
    MaxTorque: "uint16 [0..65535] (default 0)"
    Power: *int16_domain
    Speed: *float_domain
    Temperature: *float_domain
    TimeInUse: *float_domain
    Torque: *int16_domain

  ChargingPortNode: &ChargingPortNode
    IsChargingCableConnected: *boolean_domain
    IsChargingCableLocked: *boolean_domain
    IsFlapOpen: *boolean_domain
    SupportedInletTypes: "string[] each from {'IEC_TYPE_1_AC','IEC_TYPE_2_AC','IEC_TYPE_3_AC','IEC_TYPE_4_DC','IEC_TYPE_1_CCS_DC','IEC_TYPE_2_CCS_DC','TESLA_ROADSTER','TESLA_HPWC','TESLA_SUPERCHARGER','GBT_AC','GBT_DC','OTHER'}"

Vehicle:
  ADAS:
    ABS:
      IsEnabled: *boolean_domain
      IsEngaged: *boolean_domain
      IsError: *boolean_domain
    ActiveAutonomyLevel: *string_domain
    CruiseControl:
      AdaptiveDistanceSet: *float_domain
      AdaptiveIntervalSet: *uint8_domain
      IsActive: *boolean_domain
      IsAdaptive: *boolean_domain
      IsEnabled: *boolean_domain
      IsError: *boolean_domain
      SpeedSet: *float_domain
    DMS:
      IsEnabled: *boolean_domain
      IsError: *boolean_domain
      IsWarning: *boolean_domain
    EBA:
      IsEnabled: *boolean_domain
      IsEngaged: *boolean_domain
      IsError: *boolean_domain
    EBD:
      IsEnabled: *boolean_domain
      IsEngaged: *boolean_domain
      IsError: *boolean_domain
    ESC:
      IsEnabled: *boolean_domain
      IsEngaged: *boolean_domain
      IsError: *boolean_domain
      IsStrongCrossWindDetected: *boolean_domain
      RoadFriction:
        LowerBound: *float_domain
        MostProbable: *float_domain
        UpperBound: *float_domain
    LaneDepartureDetection:
      IsEnabled: *boolean_domain
      IsError: *boolean_domain
      IsWarning: *boolean_domain
    ObstacleDetection:
      Front:
        Left:
          Distance: *float_domain
          IsEnabled: *boolean_domain
          IsError: *boolean_domain
          IsWarning: *boolean_domain
          TimeGap: *uint32_domain
          WarningType: *string_domain
        Center:
          Distance: *float_domain
          IsEnabled: *boolean_domain
          IsError: *boolean_domain
          IsWarning: *boolean_domain
          TimeGap: *uint32_domain
          WarningType: *string_domain
        Right:
          Distance: *float_domain
          IsEnabled: *boolean_domain
          IsError: *boolean_domain
          IsWarning: *boolean_domain
          TimeGap: *uint32_domain
          WarningType: *string_domain
      Rear:
        Left:
          Distance: *float_domain
          IsEnabled: *boolean_domain
          IsError: *boolean_domain
          IsWarning: *boolean_domain
          TimeGap: *uint32_domain
          WarningType: *string_domain
        Center:
          Distance: *float_domain
          IsEnabled: *boolean_domain
          IsError: *boolean_domain
          IsWarning: *boolean_domain
          TimeGap: *uint32_domain
          WarningType: *string_domain
        Right:
          Distance: *float_domain
          IsEnabled: *boolean_domain
          IsError: *boolean_domain
          IsWarning: *boolean_domain
          TimeGap: *uint32_domain
          WarningType: *string_domain
    SupportedAutonomyLevel: *string_domain
    TCS:
      IsEnabled: *boolean_domain
      IsEngaged: *boolean_domain
      IsError: *boolean_domain
  Acceleration:
    Lateral: *float_domain
    Longitudinal: *float_domain
    Vertical: *float_domain
  AngularVelocity:
    Pitch: *float_domain
    Roll: *float_domain
    Yaw: *float_domain
  AverageSpeed: *float_domain
  Body:
    BodyType: *string_domain
    Hood:
      <<: *MovableItem
    Horn:
      IsActive: *boolean_domain
    Lights:
      Backup:
        IsDefect: *boolean_domain
        IsOn: *boolean_domain
      Beam:
        Low:
          IsDefect: *boolean_domain
          IsOn: *boolean_domain
        High:
          IsDefect: *boolean_domain
          IsOn: *boolean_domain
      Brake:
        IsActive: *string_domain
        IsDefect: *boolean_domain
      DirectionIndicator:
        Left:
          IsDefect: *boolean_domain
          IsSignaling: *boolean_domain
        Right:
          IsDefect: *boolean_domain
          IsSignaling: *boolean_domain
      Fog:
        Rear:
          IsDefect: *boolean_domain
          IsOn: *boolean_domain
        Front:
          IsDefect: *boolean_domain
          IsOn: *boolean_domain
      Hazard:
        IsDefect: *boolean_domain
        IsSignaling: *boolean_domain
      IsHighBeamSwitchOn: *boolean_domain
      LicensePlate:
        IsDefect: *boolean_domain
        IsOn: *boolean_domain
      LightSwitch: *string_domain
      Parking:
        IsDefect: *boolean_domain
        IsOn: *boolean_domain
      Running:
        IsDefect: *boolean_domain
        IsOn: *boolean_domain
    Mirrors:
      DriverSide:
        IsFolded: *boolean_domain
        IsHeatingOn: *boolean_domain
        IsLocked: *boolean_domain
        Pan: *int8_domain
        Tilt: *int8_domain
        Yaw: *int8_domain
      PassengerSide:
        IsFolded: *boolean_domain
        IsHeatingOn: *boolean_domain
        IsLocked: *boolean_domain
        Pan: *int8_domain
        Tilt: *int8_domain
        Yaw: *int8_domain
    Raindetection:
      Intensity: *uint8_domain
    RearMainSpoilerPosition: *float_domain
    Trunk:
      Front:
        IsLightOn: *boolean_domain
        <<: *LockableMovableItem
      Rear:
        IsLightOn: *boolean_domain
        <<: *LockableMovableItem
    Windshield:
      Front:
        IsHeatingOn: *boolean_domain
        WasherFluid:
          IsLevelLow: *boolean_domain
          Level: *uint8_domain
        Wiping:
          Intensity: *uint8_domain
          IsWipersWorn: *boolean_domain
          Mode: *string_domain
          System:
            ActualPosition: *float_domain
            DriveCurrent: *float_domain
            Frequency: *uint8_domain
            IsBlocked: *boolean_domain
            IsEndingWipeCycle: *boolean_domain
            IsOverheated: *boolean_domain
            IsPositionReached: *boolean_domain
            IsWiperError: *boolean_domain
            IsWiping: *boolean_domain
            Mode: *string_domain
            TargetPosition: *float_domain
          WiperWear: *uint8_domain
      Rear:
        IsHeatingOn: *boolean_domain
        WasherFluid:
          IsLevelLow: *boolean_domain
          Level: *uint8_domain
        Wiping:
          Intensity: *uint8_domain
          IsWipersWorn: *boolean_domain
          Mode: *string_domain
          System:
            ActualPosition: *float_domain
            DriveCurrent: *float_domain
            Frequency: *uint8_domain
            IsBlocked: *boolean_domain
            IsEndingWipeCycle: *boolean_domain
            IsOverheated: *boolean_domain
            IsPositionReached: *boolean_domain
            IsWiperError: *boolean_domain
            IsWiping: *boolean_domain
            Mode: *string_domain
            TargetPosition: *float_domain
          WiperWear: *uint8_domain
  Cabin:
    Convertible:
      Status: *string_domain
    Door:
      Row1:
        DriverSide:
          <<: *DoorWindowShade
        PassengerSide:
          <<: *DoorWindowShade
      Row2:
        DriverSide:
          <<: *DoorWindowShade
        PassengerSide:
          <<: *DoorWindowShade
    DoorCount: *uint8_domain
    DriverPosition: *string_domain
    HVAC:
      AmbientAirTemperature: *float_domain
      IsAirConditioningActive: *boolean_domain
      IsFrontDefrosterActive: *boolean_domain
      IsRearDefrosterActive: *boolean_domain
      IsRecirculationActive: *boolean_domain
      Station:
        Row1:
          Driver:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
          Passenger:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
        Row2:
          Driver:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
          Passenger:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
        Row3:
          Driver:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
          Passenger:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
        Row4:
          Driver:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
          Passenger:
            AirDistribution: *string_domain
            FanSpeed: "uint8 [0..100]"
            Temperature: *float_domain
    Infotainment:
      HMI:
        Brightness: *float_domain
        CurrentLanguage: *string_domain
        DateFormat: *string_domain
        DayNightMode: *string_domain
        DisplayOffDuration: *uint16_domain
        DistanceUnit: *string_domain
        EVEconomyUnits: *string_domain
        EVEnergyUnits: *string_domain
        FontSize: *string_domain
        FuelEconomyUnits: *string_domain
        FuelVolumeUnit: *string_domain
        IsScreenAlwaysOn: *boolean_domain
        LastActionTime: *string_domain
        SpeedUnit: *string_domain
        TemperatureUnit: *string_domain
        TimeFormat: *string_domain
        TirePressureUnit: *string_domain
      Media:
        Action: *string_domain
        DeclinedURI: *string_domain
        Played:
          Album: *string_domain
          Artist: *string_domain
          Genre: *string_domain
          PlaybackRate: *float_domain
          Source: *string_domain
          Track: *string_domain
          URI: *string_domain
        SelectedURI: *string_domain
        Volume: *uint8_domain
      Navigation:
        DestinationSet:
          Latitude: "double [-90..90]"
          Longitude: "double [-180..180]"
        GuidanceVoice: *string_domain
        Map:
          IsAutoScaleModeUsed: *boolean_domain
        Mute: *string_domain
        Volume: *uint8_domain
      SmartphoneProjection:
        Active: *string_domain
        Source: *string_domain
        SupportedMode: *string_array_domain
      SmartphoneScreenMirroring:
        Active: *string_domain
        Source: *string_domain
    IsWindowChildLockEngaged: *boolean_domain
    Light:
      AmbientLight:
        Row1:
          DriverSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
          PassengerSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
        Row2:
          DriverSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
          PassengerSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
      InteractiveLightBar:
        Color: *string_domain
        Effect: *string_domain
        Intensity: *uint8_domain
        IsLightOn: *boolean_domain
      IsDomeOn: *boolean_domain
      IsGloveBoxOn: *boolean_domain
      PerceivedAmbientLight: *uint8_domain
      Spotlight:
        Row1:
          DriverSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
          PassengerSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
        Row2:
          DriverSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
          PassengerSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
        Row3:
          DriverSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
          PassengerSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
        Row4:
          DriverSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
          PassengerSide:
            Color: *string_domain
            Intensity: *uint8_domain
            IsLightOn: *boolean_domain
    RearShade:
      <<: *MovableItem
    RearviewMirror:
      DimmingLevel: *uint8_domain
    Seat:
      Row1:
        DriverSide:
          <<: *SeatOccupantPosition
        Middle:
          <<: *SeatOccupantPosition
        PassengerSide:
          <<: *SeatOccupantPosition
      Row2:
        DriverSide:
          <<: *SeatOccupantPosition
        Middle:
          <<: *SeatOccupantPosition
        PassengerSide:
          <<: *SeatOccupantPosition
    SeatPosCount: "uint8[] (array of counts; each item 0..255)"
    SeatRowCount: *uint8_domain
    Sunroof:
      Position: *int8_domain
      Shade:
        <<: *MovableItem
      Switch: "string {'INACTIVE','CLOSE','OPEN','ONE_SHOT_CLOSE','ONE_SHOT_OPEN'}"
  CargoVolume: *float_domain
  Chassis:
    Accelerator:
      PedalPosition: *uint8_domain
    Axle:
      Row1:
        AxleWidth: *uint16_domain
        SteeringAngle: *float_domain
        TireAspectRatio: *uint8_domain
        TireDiameter: *float_domain
        TireWidth: *uint16_domain
        Torque: *int16_domain
        TrackWidth: *uint16_domain
        TreadWidth: *uint16_domain
        Wheel:
          Left:
            <<: *WheelSide
          Right:
            <<: *WheelSide
        WheelCount: *uint8_domain
        WheelDiameter: *float_domain
        WheelWidth: *float_domain
      Row2:
        AxleWidth: *uint16_domain
        SteeringAngle: *float_domain
        TireAspectRatio: *uint8_domain
        TireDiameter: *float_domain
        TireWidth: *uint16_domain
        Torque: *int16_domain
        TrackWidth: *uint16_domain
        TreadWidth: *uint16_domain
        Wheel:
          Left:
            <<: *WheelSide
          Right:
            <<: *WheelSide
        WheelCount: *uint8_domain
        WheelDiameter: *float_domain
        WheelWidth: *float_domain
    AxleCount: *uint8_domain
    Brake:
      IsDriverEmergencyBrakingDetected: *boolean_domain
      PedalPosition: *uint8_domain
    ParkingBrake:
      IsAutoApplyEnabled: *boolean_domain
      IsEngaged: *boolean_domain
    SteeringWheel:
      Angle: *int16_domain
      Extension: *uint8_domain
      Tilt: *uint8_domain
    Wheelbase: *uint16_domain
  Connectivity:
    IsConnectivityAvailable: *boolean_domain
  ControlUnit:
    Central:
      <<: *ControlUnitNode
    FrontLeft:
      <<: *ControlUnitNode
    FrontRight:
      <<: *ControlUnitNode
    RearLeft1:
      <<: *ControlUnitNode
    RearLeft2:
      <<: *ControlUnitNode
    Trunk:
      <<: *ControlUnitNode
  CurbWeight: *uint16_domain
  CurrentLocation:
    Altitude: *double_domain
    GNSSReceiver:
      FixType: *string_domain
      MountingPosition:
        X: *int16_domain
        Y: *int16_domain
        Z: *int16_domain
    Heading: *double_domain
    HorizontalAccuracy: *double_domain
    Latitude: "double [-90..90]"
    Longitude: "double [-180..180]"
    Timestamp: *string_domain
    VerticalAccuracy: *double_domain
  CurrentOverallWeight: *uint16_domain
  Diagnostics:
    DTCCount: *uint8_domain
    DTCList: "string[]; each item is an OBD-II DTC in the form [P|C|B|U][0-3][0-9A-F][0-9A-F][0-9A-F]"
    DTCReference:
      FirstCharacter:
        P: "Powertrain (engine, transmission, emissions)"
        C: "Chassis (brakes, steering, suspension, ABS)"
        B: "Body (airbags, climate, seats, windows, lighting)"
        U: "Network / communication"
      SecondDigit:
        0: "Generic / SAE standardized"
        1: "Manufacturer-specific"
        2: "Platform-specific or manufacturer-extended"
        3: "Platform-specific or manufacturer-extended"
      ThirdDigitCommon:
        0: "Fuel and air metering or emission-related auxiliary controls"
        1: "Fuel and air metering"
        2: "Fuel and air metering injector circuit"
        3: "Ignition system or misfire"
        4: "Auxiliary emission controls"
        5: "Vehicle speed and idle control"
        6: "Computer and output circuits"
        7: "Transmission"
        8: "Transmission"
        9: "Transmission"
        A-C: "Hybrid propulsion"
      GenericExamples:
        Powertrain:
          P0001: "Fuel Volume Regulator Control Circuit/Open"
          P0002: "Fuel Volume Regulator Control Circuit Range/Performance"
          P0003: "Fuel Volume Regulator Control Circuit Low"
          P0004: "Fuel Volume Regulator Control Circuit High"
          P0010: "A Camshaft Position Actuator Circuit (Bank 1)"
          P0011: "A Camshaft Position Timing Over-Advanced or System Performance (Bank 1)"
          P0128: "Coolant Thermostat Below Thermostat Regulating Temperature"
          P0171: "System Too Lean (Bank 1)"
          P0172: "System Too Rich (Bank 1)"
          P0174: "System Too Lean (Bank 2)"
          P0300: "Random / multiple cylinder misfire detected"
          P0301: "Cylinder 1 misfire detected"
          P0302: "Cylinder 2 misfire detected"
          P0303: "Cylinder 3 misfire detected"
          P0304: "Cylinder 4 misfire detected"
          P0335: "Crankshaft Position Sensor A Circuit"
          P0340: "Camshaft Position Sensor Circuit"
          P0341: "Camshaft Position Sensor Circuit Range/Performance"
          P0401: "Exhaust Gas Recirculation Flow Insufficient Detected"
          P0411: "Secondary Air Injection System Incorrect Flow Detected"
          P0420: "Catalyst System Efficiency Below Threshold (Bank 1)"
          P0430: "Catalyst System Efficiency Below Threshold (Bank 2)"
          P0440: "Evaporative Emission Control System Malfunction"
          P0442: "Evaporative Emission Control System Leak Detected (Small Leak)"
          P0446: "Evaporative Emission Control System Vent Control Circuit"
          P0455: "Evaporative Emission Control System Leak Detected (Gross Leak)"
          P0456: "Evaporative Emission Control System Leak Detected (Very Small Leak)"
          P0500: "Vehicle Speed Sensor Malfunction"
          P0505: "Idle Control System Malfunction"
          P0507: "Idle Control System RPM Higher Than Expected"
          P0700: "Transmission Control System Malfunction"
        Network:
          U0001: "High Speed CAN Communication Bus"
          U0002: "High Speed CAN Communication Bus Performance"
          U0003: "High Speed CAN Communication Bus Open"
          U0004: "High Speed CAN Communication Bus Low"
          U0005: "High Speed CAN Communication Bus High"
          U0006: "Medium Speed CAN Communication Bus"
          U0007: "Medium Speed CAN Communication Bus Performance"
          U0008: "Medium Speed CAN Communication Bus Open"
          U0009: "Medium Speed CAN Communication Bus Low"
          U0010: "Medium Speed CAN Communication Bus High"
        Chassis:
          C0021: "Wheel Speed Sensor Front Left Circuit"
          C0022: "Wheel Speed Sensor Front Left Circuit Range/Performance"
          C0025: "Wheel Speed Sensor Front Right Circuit"
          C0026: "Wheel Speed Sensor Front Right Circuit Range/Performance"
          C0029: "Wheel Speed Sensor Rear Left Circuit"
          C0030: "Wheel Speed Sensor Rear Left Circuit Range/Performance"
          C0033: "Wheel Speed Sensor Rear Right Circuit"
          C0034: "Wheel Speed Sensor Rear Right Circuit Range/Performance"
          C0035: "Left Front Wheel Speed Sensor Circuit"
          C0040: "Right Front Wheel Speed Sensor Circuit"
  Driver:
    AttentiveProbability: *float_domain
    DistractionLevel: *float_domain
    FatigueLevel: *float_domain
    HeartRate: *uint16_domain
    IsEyesOnRoad: *boolean_domain
    IsHandsOnWheel: *boolean_domain
  EmissionsCO2: *int16_domain
  Exterior:
    AirPressure: *float_domain
    AirTemperature: *float_domain
    Humidity: "float [0..100]"
    LightIntensity: "float [0..100]"
    PrecipitationIntensity: "float [0..∞)"
    PrecipitationType: "uint8 {UNKNOWN:0,NONE:1,RAIN:2,MIXED_RAIN_SNOW:3,SNOW:4,HAIL:5}"
    RoadObstruction: "uint8 {UNKNOWN:0,TREE:1,AVALANCHE:2,ROCKFALLS:3,SHED_LOAD:4,LAND_SLIP:5,ANIMAL:6,ANIMAL_LARGE:7,ANIMAL_HERD:8,NONE:9,FLOODING:10}"
    RoadSurfaceCondition: "uint8 {UNKNOWN:0,DRY:1,WET:2,SNOW:3,ICE:4,SLUSH:5,WET_ICE:6,LOOSE_GRAVEL:7}"
    RoadSurfaceContaminant: "uint8 {UNKNOWN:0,MUD:1,CHIPPINGS:2,OIL:3,FUEL:4,NONE:5}"
    VisibilityCondition: "uint8 {UNKNOWN:0,CLEAR:1,MIST:2,LOW_HEAVY_RAIN:3,LOW_HEAVY_SNOW:4,LOW_SMOKE:5,LOW_FOG:6,LOW_SUN_GLARE:7}"
    VisibilityDistance: "float [0..∞)"
    WindDirection: "float [0..360]"
    WindSpeed: "float [0..∞)"
  GrossWeight: *uint16_domain
  Height: *uint16_domain
  IsBrokenDown: *boolean_domain
  IsMoving: *boolean_domain
  Length: *uint16_domain
  LowVoltageBattery:
    CurrentCurrent: *float_domain
    CurrentVoltage: *float_domain
    NominalCapacity: *uint16_domain
    NominalVoltage: *uint16_domain
  LowVoltageSystemState: *string_domain
  MaxTowBallWeight: *uint16_domain
  MaxTowWeight: *uint16_domain
  MotionManagement:
    Brake:
      Axle:
        Row1:
          TorqueDistributionFrictionRightMaximum: *uint16_domain
          TorqueDistributionFrictionRightMinimum: *uint16_domain
          TorqueElectricMinimum: *int16_domain
          TorqueFrictionDifferenceMaximum: *uint16_domain
          Wheel:
            Left:
              OmegaLower: *uint16_domain
              OmegaUpper: *uint16_domain
              Torque: *int16_domain
              TorqueArbitrated: *int16_domain
              TorqueFrictionMaximum: *int16_domain
              TorqueFrictionMinimum: *int16_domain
            Right:
              OmegaLower: *uint16_domain
              OmegaUpper: *uint16_domain
              Torque: *int16_domain
              TorqueArbitrated: *int16_domain
              TorqueFrictionMaximum: *int16_domain
              TorqueFrictionMinimum: *int16_domain
        Row2:
          TorqueDistributionFrictionRightMaximum: *uint16_domain
          TorqueDistributionFrictionRightMinimum: *uint16_domain
          TorqueElectricMinimum: *int16_domain
          TorqueFrictionDifferenceMaximum: *uint16_domain
          Wheel:
            Left:
              OmegaLower: *uint16_domain
              OmegaUpper: *uint16_domain
              Torque: *int16_domain
              TorqueArbitrated: *int16_domain
              TorqueFrictionMaximum: *int16_domain
              TorqueFrictionMinimum: *int16_domain
            Right:
              OmegaLower: *uint16_domain
              OmegaUpper: *uint16_domain
              Torque: *int16_domain
              TorqueArbitrated: *int16_domain
              TorqueFrictionMaximum: *int16_domain
              TorqueFrictionMinimum: *int16_domain
    ElectricAxle:
      Row1:
        RotationalSpeed: *int16_domain
        RotationalSpeedMaximumLimit: *int16_domain
        RotationalSpeedMinimumLimit: *int16_domain
        RotationalSpeedTarget: *int16_domain
        Torque: *int16_domain
        TorqueMaximum: *int16_domain
        TorqueMaximumLimit: *int16_domain
        TorqueMinimum: *int16_domain
        TorqueMinimumLimit: *int16_domain
        TorqueTarget: *int16_domain
      Row2:
        RotationalSpeed: *int16_domain
        RotationalSpeedMaximumLimit: *int16_domain
        RotationalSpeedMinimumLimit: *int16_domain
        RotationalSpeedTarget: *int16_domain
        Torque: *int16_domain
        TorqueMaximum: *int16_domain
        TorqueMaximumLimit: *int16_domain
        TorqueMinimum: *int16_domain
        TorqueMinimumLimit: *int16_domain
        TorqueTarget: *int16_domain
    Steering:
      Axle:
        Row1:
          PositionOffsetTargetMode: *uint8_domain
          PositionTargetMode: *uint8_domain
          RackPosition: *int16_domain
          RackPositionOffsetTarget: *int16_domain
          RackPositionTarget: *int16_domain
          SteerAngle: *int16_domain
          SteerAngleOffsetTarget: *int16_domain
          SteerAngleTarget: *int16_domain
        Row2:
          SteerAngle: *int16_domain
          SteerAngleTarget: *int16_domain
          SteerAngleVelocityTarget: *int16_domain
      SteeringWheel:
        Angle: *int16_domain
        AngleTarget: *int16_domain
        AngleTargetMode: *uint8_domain
        Torque: *int16_domain
        TorqueOffsetTarget: *int16_domain
        TorqueOffsetTargetMode: *uint8_domain
        TorqueTarget: *int16_domain
        TorqueTargetMode: *uint8_domain
    Suspension:
      Axle:
        Row1:
          RollTorque: *int16_domain
          Wheel:
            Left:
              DampingForce: *int16_domain
              DampingForceTarget: *int16_domain
              DampingRate: *uint8_domain
              DampingRateTarget: *uint8_domain
            Right:
              DampingForce: *int16_domain
              DampingForceTarget: *int16_domain
              DampingRate: *uint8_domain
              DampingRateTarget: *uint8_domain
        Row2:
          RollTorque: *int16_domain
          Wheel:
            Left:
              DampingForce: *int16_domain
              DampingForceTarget: *int16_domain
              DampingRate: *uint8_domain
              DampingRateTarget: *uint8_domain
            Right:
              DampingForce: *int16_domain
              DampingForceTarget: *int16_domain
              DampingRate: *uint8_domain
              DampingRateTarget: *uint8_domain
      DampingPrioTarget: *uint8_domain
      RollPrioTarget: *uint8_domain
      RollTorqueDistributionFrontMaximum: *uint8_domain
      RollTorqueDistributionFrontMinimum: *uint8_domain
      RollTorqueTarget: *int16_domain
  Occupant:
    Row1:
      DriverSide:
        <<: *OccupantNode
      Middle:
        <<: *OccupantNode
      PassengerSide:
        <<: *OccupantNode
    Row2:
      DriverSide:
        <<: *OccupantNode
      Middle:
        <<: *OccupantNode
      PassengerSide:
        <<: *OccupantNode
  Orientation:
    Pitch: *float_domain
    Roll: *float_domain
    Yaw: *float_domain
  Powertrain:
    AccumulatedBrakingEnergy: *float_domain
    CombustionEngine:
      AspirationType: "string {'UNKNOWN','NATURAL','SUPERCHARGER','TURBOCHARGER'}"
      Bore: *float_domain
      CompressionRatio: "string formatted like '9.2:1'"
      Configuration: "string {'UNKNOWN','STRAIGHT','V','BOXER','W','ROTARY','RADIAL','SQUARE','H','U','OPPOSED','X'}"
      DieselExhaustFluid:
        Capacity: *float_domain
        IsLevelLow: *boolean_domain
        Level: "uint8 [0..100]"
        Range: *uint32_domain
      DieselParticulateFilter:
        DeltaPressure: *float_domain
        InletTemperature: *float_domain
        OutletTemperature: *float_domain
      Displacement: *uint16_domain
      EOP: *uint16_domain
      EngineCode: *string_domain
      EngineCoolant:
        Capacity: *float_domain
        Level: *string_domain
        LifeRemaining: *int32_domain
        Temperature: *float_domain
      EngineHours: *float_domain
      EngineOil:
        Capacity: *float_domain
        Level: "string {'CRITICALLY_LOW','LOW','NORMAL','HIGH','CRITICALLY_HIGH'}"
        LifeRemaining: *int32_domain
        PressureStatus: "string {'NORMAL','WARNING','ALERT','ERROR'}"
        Temperature: *float_domain
      IdleHours: *float_domain
      IsRunning: *boolean_domain
      MAF: *uint16_domain
      MAP: *uint16_domain
      MaxPower: "uint16 [0..65535] (default 0)"
      MaxTorque: "uint16 [0..65535] (default 0)"
      NumberOfCylinders: *uint16_domain
      NumberOfValvesPerCylinder: *uint16_domain
      Power: *uint16_domain
      Speed: *float_domain
      StrokeLength: *float_domain
      TPS: "uint8 [0..100]"
      Torque: *int16_domain
    ElectricMotor:
      Front:
        <<: *ElectricMotorNode
      Rear:
        <<: *ElectricMotorNode
      FrontLeft:
        <<: *ElectricMotorNode
      FrontRight:
        <<: *ElectricMotorNode
      RearLeft:
        <<: *ElectricMotorNode
      RearRight:
        <<: *ElectricMotorNode
    FuelSystem:
      AbsoluteLevel: *float_domain
      AfterRefuelingFuelEconomy: *float_domain
      AverageConsumption: "float [0..∞)"
      ConsumptionSinceLastRefuel: *float_domain
      ConsumptionSinceStart: *float_domain
      CumulativeFuelEconomy: *float_domain
      DriveFuelEconomy: *float_domain
      HybridType: "string {'UNKNOWN','NOT_APPLICABLE','STOP_START','BELT_ISG','CIMG','PHEV'}"
      InstantConsumption: "float [0..∞)"
      InstantFuelEconomy: *float_domain
      InstantantFuelEconomy: *float_domain
      IsEngineStopStartEnabled: *boolean_domain
      IsFuelLevelEmpty: *boolean_domain
      IsFuelLevelLow: *boolean_domain
      IsFuelPortFlapOpen: *boolean_domain
      Range: *uint32_domain
      RefuelPortPosition: "string[] each from {'FRONT_LEFT','FRONT_MIDDLE','FRONT_RIGHT','REAR_LEFT','REAR_MIDDLE','REAR_RIGHT','LEFT_FRONT','LEFT_MIDDLE','LEFT_REAR','RIGHT_FRONT','RIGHT_MIDDLE','RIGHT_REAR'}"
      RelativeLevel: "uint8 [0..100]"
      SupportedFuel: "string[] each from {'E5_95','E5_98','E10_95','E10_98','E85','B7','B10','B20','B30','B100','XTL','LPG','CNG','LNG','H2','OTHER'}"
      SupportedFuelTypes: "string[] each from {'GASOLINE','DIESEL','E85','LPG','CNG','LNG','H2','OTHER'}"
      TankCapacity: *float_domain
      TimeRemaining: *uint32_domain
    Range: *uint32_domain
    RangeExtender:
      ChargeDepleting:
        EnergyConsumption: *float_domain
        Range: *uint32_domain
      ChargeSustaining:
        FuelEconomy: *float_domain
        Range: *uint32_domain
      CombinedFuelEconomy: *float_domain
      OperatingMode: "string {'CHARGE_DEPLETING','CHARGE_SUSTAINING','BLENDED'}"
    TimeRemaining: *uint32_domain
    TractionBattery:
      AccumulatedChargedEnergy: *float_domain
      AccumulatedChargedThroughput: *float_domain
      AccumulatedConsumedEnergy: *float_domain
      AccumulatedConsumedThroughput: *float_domain
      CellVoltage:
        CellVoltages: "float[] (array indexed by cell)"
        IdMax: *uint16_domain
        IdMin: *uint16_domain
        Max: *float_domain
        Min: *float_domain
      Charging:
        AveragePower: *float_domain
        ChargeCurrent:
          DC: *float_domain
          Phase1: *float_domain
          Phase2: *float_domain
          Phase3: *float_domain
        ChargeLimit: "uint8 [0..100] (default 100)"
        ChargeRate: *float_domain
        ChargeVoltage:
          DC: *float_domain
          Phase1: *float_domain
          Phase2: *float_domain
          Phase3: *float_domain
        ChargingPort:
          FrontLeft:
            <<: *ChargingPortNode
          FrontMiddle:
            <<: *ChargingPortNode
          FrontRight:
            <<: *ChargingPortNode
          RearLeft:
            <<: *ChargingPortNode
          RearMiddle:
            <<: *ChargingPortNode
          RearRight:
            <<: *ChargingPortNode
          AnyPosition:
            <<: *ChargingPortNode
        EvseId: *string_domain
        IsCharging: *boolean_domain
        IsDischarging: *boolean_domain
        Location:
          Altitude: *double_domain
          Latitude: "double [-90..90]"
          Longitude: "double [-180..180]"
        MaxPower: *float_domain
        MaximumChargingCurrent:
          DC: *float_domain
          Phase1: *float_domain
          Phase2: *float_domain
          Phase3: *float_domain
        PowerLoss: *float_domain
        StartStopCharging: "string {'START','STOP'}"
        Temperature: *float_domain
        TimeToComplete: *uint32_domain
        Timer:
          Mode: "string {'INACTIVE','START_TIME','END_TIME'}"
          Time: "string (ISO 8601 UTC timestamp)"
      CurrentCurrent: *float_domain
      CurrentPower: *float_domain
      CurrentVoltage: *float_domain
      DCDC:
        PowerLoss: *float_domain
        Temperature: *float_domain
      ErrorCodes: "string[] (format open; may contain OBD-II DTCs or OEM-specific codes)"
      GrossCapacity: *uint16_domain
      Id: *string_domain
      IsGroundConnected: *boolean_domain
      IsPowerConnected: *boolean_domain
      MaxVoltage: *uint16_domain
      NetCapacity: *uint16_domain
      NominalVoltage: *uint16_domain
      PowerLoss: *float_domain
      ProductionDate: "string (ISO 8601 date, e.g. YYYY-MM-DD)"
      Range: *uint32_domain
      StateOfCharge:
        Current: "float [0..100]"
        CurrentEnergy: *float_domain
        Displayed: "float [0..100]"
      StateOfHealth: "float [0..100]"
      Temperature:
        Average: *float_domain
        CellTemperature: "float[] (array indexed by cell)"
        Max: *float_domain
        Min: *float_domain
      TimeRemaining: *uint32_domain
    Transmission:
      ClutchEngagement: "float [0..100]"
      ClutchWear: "uint8 [0..100]"
      CurrentGear: "int8 {...,-2,-1,0,1,2,...}; 0=Neutral, positive=Forward, negative=Reverse"
      DiffLockFrontEngagement: "float [0..100]"
      DiffLockRearEngagement: "float [0..100]"
      DriveType: "string {'UNKNOWN','FORWARD_WHEEL_DRIVE','REAR_WHEEL_DRIVE','ALL_WHEEL_DRIVE'}"
      GearChangeMode: "string {'MANUAL','AUTOMATIC'}"
      GearCount: "int8; -1 indicates CVT"
      IsElectricalPowertrainEngaged: *boolean_domain
      IsLowRangeEngaged: *boolean_domain
      IsParkLockEngaged: *boolean_domain
      PerformanceMode: "string {'NORMAL','SPORT','ECONOMY','SNOW','RAIN'}"
      SelectedGear: "int8 {...,-2,-1,0,1,2,...,126,127}; 0=Neutral, 126=Park, 127=Drive"
      Temperature: *float_domain
      TorqueDistribution: "float [-100..100]"
      TravelledDistance: *float_domain
      Type: "string {'UNKNOWN','SEQUENTIAL','H','AUTOMATIC','DSG','CVT'}"
    Type: "string {'COMBUSTION','HYBRID','ELECTRIC'}"
  RoofLoad: *int16_domain
  Safety:
    IsFire: *boolean_domain
    IsSubmersed: *boolean_domain
    RoadIcingState: *string_domain
    Rollover: *string_domain
    VisibilityImpairment: *string_domain
  Service:
    DistanceToService: *float_domain
    IsServiceDue: *boolean_domain
    TimeToService: *int32_domain
  Speed: *float_domain
  StartTime: *string_domain
  Trailer:
    IsConnected: *boolean_domain
  TraveledDistance: *uint32_domain
  TraveledDistanceSinceStart: *uint32_domain
  TripDuration: *float_domain
  TripMeterReading: *uint32_domain
  TurningDiameter: *uint16_domain
  VersionVSS:
    Label: *string_domain
    Major: *uint32_domain
    Minor: *uint32_domain
    Patch: *uint32_domain
  WidthExcludingMirrors: *uint16_domain
  WidthFoldedMirrors: *uint16_domain
  WidthIncludingMirrors: *uint16_domain