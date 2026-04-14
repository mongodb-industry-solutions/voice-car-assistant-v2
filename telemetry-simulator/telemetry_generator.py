"""
Realistic automotive telemetry data generator.
Simulates sensor readings with gradual changes, correlations, and anomalies.
"""

import random
import time
import math
from typing import Dict, Any, Optional
from thresholds import check_status, get_unit, OBD_PIDS, THRESHOLDS


class TelemetryGenerator:
    """Generate realistic automotive telemetry data with configurable anomalies."""
    
    def __init__(self, vehicle_id: str = "VIN12345678901234"):
        self.vehicle_id = vehicle_id
        self.anomaly_probability = 0.2  # 20% chance of anomaly
        self.start_time = time.time()
        
        # Initialize baseline values (normal operating conditions)
        self.current_state = {
            "engine": {
                "oil_pressure": 40.0,
                "oil_level": 85.0,
                "coolant_temp": 95.0,
                "rpm": 800.0  # Idle
            },
            "fuel": {
                "level": 75.0,
                "pressure": 58.0
            },
            "battery": {
                "voltage": 12.6,
                "state_of_charge": 85.0,
                "health": 95.0
            },
            "tires": {
                "front_left": {"pressure": 32.0, "temp": 25.0},
                "front_right": {"pressure": 32.0, "temp": 25.0},
                "rear_left": {"pressure": 32.0, "temp": 26.0},
                "rear_right": {"pressure": 31.0, "temp": 25.0}
            },
            "transmission": {
                "oil_temp": 85.0,
                "gear": 1
            },
            "brakes": {
                "fluid_level": 95.0,
                "pad_wear_front": 75.0,
                "pad_wear_rear": 80.0
            }
        }
        
        # Driving scenario (affects RPM, temps, etc.)
        self.driving_mode = "idle"  # idle, city, highway
    
    def set_anomaly_probability(self, probability: float):
        """Set the probability of generating anomalous values (0.0 to 1.0)."""
        self.anomaly_probability = max(0.0, min(1.0, probability))
    
    def _apply_drift(self, current: float, target: float, max_change: float) -> float:
        """Apply gradual drift towards target value."""
        diff = target - current
        change = max(-max_change, min(max_change, diff * 0.1))
        return current + change + random.gauss(0, max_change * 0.1)
    
    def _simulate_engine(self) -> Dict[str, Any]:
        """Simulate engine sensors with correlations."""
        engine = self.current_state["engine"]
        
        # Simulate RPM based on driving mode
        if self.driving_mode == "idle":
            target_rpm = random.uniform(750, 900)
        elif self.driving_mode == "city":
            target_rpm = random.uniform(1500, 3000)
        else:  # highway
            target_rpm = random.uniform(2000, 3500)
        
        engine["rpm"] = self._apply_drift(engine["rpm"], target_rpm, 100)
        
        # Oil pressure correlates with RPM
        base_oil_pressure = 20 + (engine["rpm"] / 100)
        engine["oil_pressure"] = self._apply_drift(
            engine["oil_pressure"], base_oil_pressure, 2.0
        )
        
        # Coolant temp increases with RPM and time
        base_temp = 85 + (engine["rpm"] / 500)
        engine["coolant_temp"] = self._apply_drift(
            engine["coolant_temp"], base_temp, 1.0
        )
        
        # Oil level decreases slowly over time (consumption)
        engine["oil_level"] -= random.uniform(0.001, 0.005)
        engine["oil_level"] = max(0, engine["oil_level"])
        
        # Apply anomalies
        if random.random() < self.anomaly_probability:
            anomaly_type = random.choice(["low_oil_pressure", "high_temp", "low_oil"])
            if anomaly_type == "low_oil_pressure":
                engine["oil_pressure"] = random.uniform(8, 15)
            elif anomaly_type == "high_temp":
                engine["coolant_temp"] = random.uniform(108, 120)
            elif anomaly_type == "low_oil":
                engine["oil_level"] = random.uniform(5, 18)
        
        return {
            "oil_pressure": {
                "value": round(engine["oil_pressure"], 1),
                "unit": get_unit("engine", "oil_pressure"),
                "status": check_status("engine", "oil_pressure", engine["oil_pressure"]),
                "pid": OBD_PIDS["engine_oil_pressure"]
            },
            "oil_level": {
                "value": round(engine["oil_level"], 1),
                "unit": get_unit("engine", "oil_level"),
                "status": check_status("engine", "oil_level", engine["oil_level"]),
                "pid": OBD_PIDS["engine_oil_level"]
            },
            "coolant_temp": {
                "value": round(engine["coolant_temp"], 1),
                "unit": get_unit("engine", "coolant_temp"),
                "status": check_status("engine", "coolant_temp", engine["coolant_temp"]),
                "pid": OBD_PIDS["coolant_temp"]
            },
            "rpm": {
                "value": int(engine["rpm"]),
                "unit": get_unit("engine", "rpm"),
                "status": check_status("engine", "rpm", engine["rpm"]),
                "pid": OBD_PIDS["engine_rpm"]
            }
        }
    
    def _simulate_fuel(self) -> Dict[str, Any]:
        """Simulate fuel system."""
        fuel = self.current_state["fuel"]
        
        # Fuel consumption based on RPM
        rpm = self.current_state["engine"]["rpm"]
        consumption_rate = 0.001 + (rpm / 1000000)
        fuel["level"] -= consumption_rate
        fuel["level"] = max(0, fuel["level"])
        
        # Fuel pressure varies slightly
        fuel["pressure"] = self._apply_drift(fuel["pressure"], 58.0, 1.0)
        
        # Anomaly: Low fuel
        if random.random() < self.anomaly_probability * 0.5:
            fuel["level"] = random.uniform(3, 12)
        
        return {
            "level": {
                "value": round(fuel["level"], 1),
                "unit": get_unit("fuel", "level"),
                "status": check_status("fuel", "level", fuel["level"]),
                "pid": OBD_PIDS["fuel_level"]
            },
            "pressure": {
                "value": round(fuel["pressure"], 1),
                "unit": get_unit("fuel", "pressure"),
                "status": check_status("fuel", "pressure", fuel["pressure"]),
                "pid": OBD_PIDS["fuel_pressure"]
            }
        }
    
    def _simulate_battery(self) -> Dict[str, Any]:
        """Simulate battery system."""
        battery = self.current_state["battery"]
        
        # Voltage varies with charge state
        target_voltage = 12.0 + (battery["state_of_charge"] / 50)
        battery["voltage"] = self._apply_drift(battery["voltage"], target_voltage, 0.1)
        
        # SOC decreases slowly
        battery["state_of_charge"] -= random.uniform(0.001, 0.01)
        battery["state_of_charge"] = max(0, min(100, battery["state_of_charge"]))
        
        # Health degrades very slowly
        battery["health"] -= random.uniform(0, 0.0001)
        
        # Anomaly: Low battery
        if random.random() < self.anomaly_probability * 0.3:
            battery["state_of_charge"] = random.uniform(8, 18)
            battery["voltage"] = random.uniform(11.2, 11.7)
        
        return {
            "voltage": {
                "value": round(battery["voltage"], 2),
                "unit": get_unit("battery", "voltage"),
                "status": check_status("battery", "voltage", battery["voltage"]),
                "pid": OBD_PIDS["battery_voltage"]
            },
            "state_of_charge": {
                "value": round(battery["state_of_charge"], 1),
                "unit": get_unit("battery", "state_of_charge"),
                "status": check_status("battery", "state_of_charge", battery["state_of_charge"]),
                "pid": OBD_PIDS["battery_soc"]
            },
            "health": {
                "value": round(battery["health"], 1),
                "unit": get_unit("battery", "health"),
                "status": check_status("battery", "health", battery["health"]),
                "pid": OBD_PIDS["battery_health"]
            }
        }
    
    def _simulate_tires(self) -> Dict[str, Any]:
        """Simulate tire pressure monitoring system (TPMS)."""
        tires = self.current_state["tires"]
        
        result = {}
        for position, tire in tires.items():
            # Pressure varies slightly with temperature
            tire["pressure"] = self._apply_drift(tire["pressure"], 32.0, 0.2)
            
            # Temperature varies with driving
            rpm = self.current_state["engine"]["rpm"]
            target_temp = 25 + (rpm / 200)
            tire["temp"] = self._apply_drift(tire["temp"], target_temp, 0.5)
            
            # Anomaly: Low pressure in one tire
            if random.random() < self.anomaly_probability * 0.15:
                tire["pressure"] = random.uniform(22, 27)
            
            result[position] = {
                "pressure": round(tire["pressure"], 1),
                "temp": round(tire["temp"], 1),
                "unit": f"{get_unit('tires', 'pressure')}/{get_unit('tires', 'temp')}",
                "status": check_status("tires", "pressure", tire["pressure"])
            }
        
        return result
    
    def _simulate_transmission(self) -> Dict[str, Any]:
        """Simulate transmission."""
        trans = self.current_state["transmission"]
        
        # Temperature increases with RPM
        rpm = self.current_state["engine"]["rpm"]
        target_temp = 70 + (rpm / 100)
        trans["oil_temp"] = self._apply_drift(trans["oil_temp"], target_temp, 1.0)
        
        # Gear changes with RPM (simplified)
        if rpm < 1500:
            trans["gear"] = 1
        elif rpm < 2500:
            trans["gear"] = 2
        elif rpm < 3500:
            trans["gear"] = 3
        else:
            trans["gear"] = 4
        
        return {
            "oil_temp": {
                "value": round(trans["oil_temp"], 1),
                "unit": get_unit("transmission", "oil_temp"),
                "status": check_status("transmission", "oil_temp", trans["oil_temp"]),
                "pid": OBD_PIDS["transmission_temp"]
            },
            "gear": {
                "value": trans["gear"],
                "unit": "gear",
                "status": "normal"
            }
        }
    
    def _simulate_brakes(self) -> Dict[str, Any]:
        """Simulate brake system."""
        brakes = self.current_state["brakes"]
        
        # Fluid level decreases very slowly
        brakes["fluid_level"] -= random.uniform(0, 0.001)
        brakes["fluid_level"] = max(0, brakes["fluid_level"])
        
        # Pad wear increases slowly
        brakes["pad_wear_front"] -= random.uniform(0, 0.002)
        brakes["pad_wear_rear"] -= random.uniform(0, 0.001)
        
        return {
            "fluid_level": {
                "value": round(brakes["fluid_level"], 1),
                "unit": get_unit("brakes", "fluid_level"),
                "status": check_status("brakes", "fluid_level", brakes["fluid_level"])
            },
            "pad_wear_front": {
                "value": round(brakes["pad_wear_front"], 1),
                "unit": get_unit("brakes", "pad_wear"),
                "status": check_status("brakes", "pad_wear", brakes["pad_wear_front"])
            },
            "pad_wear_rear": {
                "value": round(brakes["pad_wear_rear"], 1),
                "unit": get_unit("brakes", "pad_wear"),
                "status": check_status("brakes", "pad_wear", brakes["pad_wear_rear"])
            }
        }
    
    def generate_snapshot(self) -> Dict[str, Any]:
        """Generate a complete telemetry snapshot."""
        # Occasionally change driving mode
        if random.random() < 0.05:
            self.driving_mode = random.choice(["idle", "city", "highway"])
        
        telemetry_batch = {
            "engine": self._simulate_engine(),
            "fuel": self._simulate_fuel(),
            "battery": self._simulate_battery(),
            "tires": self._simulate_tires(),
            "transmission": self._simulate_transmission(),
            "brakes": self._simulate_brakes()
        }
        
        # Count anomalies
        anomaly_count = sum(
            1 for category in telemetry_batch.values()
            for sensor in (category.values() if isinstance(category, dict) else [])
            for key, value in (sensor.items() if isinstance(sensor, dict) else [])
            if key == "status" and value in ["warning", "critical"]
        )
        
        return {
            "timestamp": int(time.time() * 1000),
            "vehicle_id": self.vehicle_id,
            "driving_mode": self.driving_mode,
            "anomaly_count": anomaly_count,
            "telemetry_batch": telemetry_batch
        }
