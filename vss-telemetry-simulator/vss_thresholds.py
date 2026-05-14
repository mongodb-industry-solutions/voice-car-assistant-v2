"""
VSS-aligned thresholds for all telemetry signals.
"""

VSS_THRESHOLDS = {
    # Powertrain
    "engineRpm":      {"normal": (600, 4000), "warning": (4000, 5500), "critical_hi": 5500},
    "coolantTempC":   {"normal": (70, 100),   "warning": (100, 110),   "critical_hi": 110},
    "fuelLevelPct":   {"critical_lo": 10,     "warning_lo": 25},
    "odometerKm":     {},  # informational only

    # Battery
    "socPct":         {"critical_lo": 10, "warning_lo": 20},
    "sohPct":         {"critical_lo": 70, "warning_lo": 80},
    "batteryTempC":   {"normal": (-10, 45), "warning": (45, 55), "critical_hi": 55},
    "voltageV":       {"critical_lo": 11.0, "warning_lo": 11.8, "normal_hi": 14.5},
    "chargingPowerKw":{"informational": True},

    # Chassis (tires in kPa; ~220 kPa = ~32 psi)
    "tirePressureKpa":{"critical_lo": 165, "warning_lo": 193, "warning_hi": 276, "critical_hi": 290},
    "steeringAngleDeg":{"warning_hi": 450, "critical_hi": 540},
    "brakePedalPct":  {},

    # Cabin
    "insideTempC":    {"warning_lo": 5, "warning_hi": 35},
    "outsideTempC":   {},  # informational
    "fanSpeed":       {"normal": (0, 5)},

    # ADAS
    "cruiseSetSpeedKph": {"warning_hi": 150, "critical_hi": 180},
}


def classify(signal: str, value: float) -> str:
    """Return 'normal', 'warning', or 'critical'."""
    t = VSS_THRESHOLDS.get(signal, {})
    if not t:
        return "normal"
    if "critical_hi" in t and value >= t["critical_hi"]:
        return "critical"
    if "critical_lo" in t and value <= t["critical_lo"]:
        return "critical"
    if "warning_hi" in t and value >= t["warning_hi"]:
        return "warning"
    if "warning_lo" in t and value <= t["warning_lo"]:
        return "warning"
    return "normal"
