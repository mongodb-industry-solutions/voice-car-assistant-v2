"""
Telemetry thresholds and status definitions for automotive sensors.
Based on typical automotive specifications and OBD-II standards.
"""

# Sensor thresholds (min, max, critical values)
THRESHOLDS = {
    "engine": {
        "oil_pressure": {
            "min": 20,
            "max": 60,
            "critical_min": 10,
            "critical_max": 80,
            "unit": "psi"
        },
        "oil_level": {
            "min": 20,
            "critical_min": 10,
            "unit": "%"
        },
        "coolant_temp": {
            "min": 80,
            "max": 105,
            "critical_max": 115,
            "unit": "°C"
        },
        "rpm": {
            "min": 600,
            "max": 6500,
            "critical_max": 7000,
            "unit": "rpm"
        }
    },
    "fuel": {
        "level": {
            "min": 15,
            "critical_min": 5,
            "unit": "%"
        },
        "pressure": {
            "min": 50,
            "max": 65,
            "critical_min": 40,
            "critical_max": 75,
            "unit": "kPa"
        }
    },
    "battery": {
        "voltage": {
            "min": 11.8,
            "max": 14.5,
            "critical_min": 11.0,
            "critical_max": 15.0,
            "unit": "V"
        },
        "state_of_charge": {
            "min": 20,
            "critical_min": 10,
            "unit": "%"
        },
        "health": {
            "min": 70,
            "critical_min": 50,
            "unit": "%"
        }
    },
    "tires": {
        "pressure": {
            "min": 28,
            "max": 38,
            "critical_min": 24,
            "critical_max": 42,
            "unit": "psi"
        },
        "temp": {
            "max": 80,
            "critical_max": 95,
            "unit": "°C"
        }
    },
    "transmission": {
        "oil_temp": {
            "min": 60,
            "max": 95,
            "critical_max": 110,
            "unit": "°C"
        }
    },
    "brakes": {
        "fluid_level": {
            "min": 30,
            "critical_min": 15,
            "unit": "%"
        },
        "pad_wear": {
            "min": 20,
            "critical_min": 10,
            "unit": "%"
        }
    }
}

# OBD-II PIDs (Parameter IDs)
OBD_PIDS = {
    "engine_oil_pressure": "0x0A",
    "engine_oil_level": "0xOIL_LEVEL",
    "coolant_temp": "0x05",
    "engine_rpm": "0x0C",
    "fuel_level": "0x2F",
    "fuel_pressure": "0x0A",
    "battery_voltage": "0xBAT_VOLT",
    "battery_soc": "0xBAT_SOC",
    "battery_health": "0xBAT_HEALTH",
    "transmission_temp": "0xTRANS_TEMP"
}


def check_status(category: str, sensor: str, value: float) -> str:
    """
    Determine status based on thresholds.
    
    Returns:
        "critical" - Dangerous value requiring immediate attention
        "warning" - Value outside normal range
        "normal" - Value within acceptable range
    """
    if category not in THRESHOLDS:
        return "normal"
    
    if sensor not in THRESHOLDS[category]:
        return "normal"
    
    threshold = THRESHOLDS[category][sensor]
    
    # Check critical thresholds first
    if "critical_min" in threshold and value < threshold["critical_min"]:
        return "critical"
    if "critical_max" in threshold and value > threshold["critical_max"]:
        return "critical"
    
    # Check warning thresholds
    if "min" in threshold and value < threshold["min"]:
        return "warning"
    if "max" in threshold and value > threshold["max"]:
        return "warning"
    
    return "normal"


def get_unit(category: str, sensor: str) -> str:
    """Get the unit for a sensor."""
    if category in THRESHOLDS and sensor in THRESHOLDS[category]:
        return THRESHOLDS[category][sensor].get("unit", "")
    return ""
