"""
Generates realistic VSS (Vehicle Signal Specification) telemetry data
for a single simulated vehicle.
"""

import random
import math
import time
import json
import geohash2 as geohash
from vss_thresholds import classify, VSS_THRESHOLDS

VEHICLE_ID = "VSS-DEMO-VIN-001"
VIN        = "WBA12345VSS00001"
TRIP_ID    = "trip-001"

# Starting GPS position (Paris outskirts)
BASE_LAT, BASE_LON = 48.8566, 2.3522


class VssGenerator:
    def __init__(self):
        self.tick = 0

        # Powertrain state
        self.speed_kph = 0.0
        self.odometer_km = 12345.0
        self.fuel_level_pct = 75.0
        self.coolant_temp_c = 20.0  # starts cold
        self.throttle_pct = 0.0
        self.gear = 1

        # Battery state
        self.soc_pct = 85.0
        self.soh_pct = 97.5
        self.battery_temp_c = 22.0
        self.charging_state = "not_charging"
        self.charging_power_kw = 0.0
        self.voltage_v = 13.8

        # Chassis state
        self.steering_angle_deg = 0.0
        self.brake_pedal_pct = 0.0
        self.tire_fl = 221.0
        self.tire_fr = 220.0
        self.tire_rl = 219.0
        self.tire_rr = 222.0

        # Cabin state
        self.inside_temp_c = 22.0
        self.outside_temp_c = 15.0
        self.hvac_mode = "auto"
        self.fan_speed = 2

        # Location state
        self.path_angle = 0.0   # angle along circular path in radians
        self.altitude_m = 40.0
        self.accuracy_m = 3.5

        # ADAS state
        self.cruise_set_speed_kph = 0.0
        self.autopilot_mode = "none"

        # Anomaly injection state
        self._anomaly_active = None

    # ------------------------------------------------------------------ #
    #  Internal helpers                                                    #
    # ------------------------------------------------------------------ #

    def _drift(self, value: float, target: float, rate: float, noise: float = 0.0) -> float:
        """Drift value toward target with optional noise."""
        v = value + (target - value) * rate
        if noise:
            v += random.gauss(0, noise)
        return v

    def _clamp(self, value: float, lo: float, hi: float) -> float:
        return max(lo, min(hi, value))

    def _speed_target(self) -> float:
        """Oscillate between city and highway speeds in a slow cycle."""
        cycle = math.sin(self.tick * 0.02)  # slow cycle
        if cycle > 0.3:
            return 100.0 + cycle * 30.0   # highway 100-130 kph
        elif cycle > -0.3:
            return 50.0 + cycle * 20.0    # suburban 44-56 kph
        else:
            return 20.0 + (cycle + 0.3) * 10.0  # city slow 17-23 kph

    def _compute_gear(self, speed: float) -> int:
        if speed < 15:
            return 1
        elif speed < 30:
            return 2
        elif speed < 50:
            return 3
        elif speed < 80:
            return 4
        elif speed < 110:
            return 5
        else:
            return 6

    def _inject_anomaly(self) -> list:
        """With 15% probability, inject a random anomaly and return events."""
        events = []
        if random.random() > 0.15:
            self._anomaly_active = None
            return events

        anomaly = random.choice([
            "low_fuel",
            "low_battery",
            "high_coolant",
            "tire_pressure",
            "high_rpm",
        ])
        self._anomaly_active = anomaly

        if anomaly == "low_fuel":
            self.fuel_level_pct = random.uniform(5.0, 12.0)
            severity = classify("fuelLevelPct", self.fuel_level_pct)
            events.append({
                "eventType": "FuelLow",
                "severity": severity,
                "vssPath": "Vehicle.Powertrain.FuelSystem.Level",
                "code": "FUEL_LOW",
                "description": f"Fuel level low: {self.fuel_level_pct:.1f}%",
                "payloadJson": json.dumps({"fuelLevelPct": round(self.fuel_level_pct, 1)}),
            })

        elif anomaly == "low_battery":
            self.soc_pct = random.uniform(8.0, 18.0)
            severity = classify("socPct", self.soc_pct)
            events.append({
                "eventType": "BatteryLow",
                "severity": severity,
                "vssPath": "Vehicle.Powertrain.TractionBattery.StateOfCharge.Current",
                "code": "SOC_LOW",
                "description": f"Battery state of charge low: {self.soc_pct:.1f}%",
                "payloadJson": json.dumps({"socPct": round(self.soc_pct, 1)}),
            })

        elif anomaly == "high_coolant":
            self.coolant_temp_c = random.uniform(105.0, 118.0)
            severity = classify("coolantTempC", self.coolant_temp_c)
            events.append({
                "eventType": "CoolantTempHigh",
                "severity": severity,
                "vssPath": "Vehicle.Powertrain.CombustionEngine.ECT",
                "code": "COOLANT_TEMP_HIGH",
                "description": f"Coolant temperature high: {self.coolant_temp_c:.1f}°C",
                "payloadJson": json.dumps({"coolantTempC": round(self.coolant_temp_c, 1)}),
            })

        elif anomaly == "tire_pressure":
            tire_key = random.choice(["FL", "FR", "RL", "RR"])
            low_pressure = random.uniform(150.0, 195.0)
            severity = classify("tirePressureKpa", low_pressure)
            attr = f"tire_{tire_key.lower()}"
            setattr(self, attr, low_pressure)
            vss_corner = {
                "FL": "FrontLeft", "FR": "FrontRight",
                "RL": "RearLeft",  "RR": "RearRight",
            }[tire_key]
            events.append({
                "eventType": "TirePressureLow",
                "severity": severity,
                "vssPath": f"Vehicle.Chassis.Axle.Row1.Wheel.{vss_corner}.Tire.Pressure",
                "code": "TIRE_PRESSURE_LOW",
                "description": f"Tire pressure low ({tire_key}): {low_pressure:.0f} kPa",
                "payloadJson": json.dumps({
                    "corner": tire_key,
                    "pressureKpa": round(low_pressure, 1),
                }),
            })

        elif anomaly == "high_rpm":
            self.speed_kph = self._clamp(self.speed_kph, 80.0, 130.0)
            rpm_anomaly = random.uniform(5400.0, 6200.0)
            severity = classify("engineRpm", rpm_anomaly)
            events.append({
                "eventType": "EngineRpmHigh",
                "severity": severity,
                "vssPath": "Vehicle.Powertrain.CombustionEngine.Speed",
                "code": "ENGINE_RPM_HIGH",
                "description": f"Engine RPM elevated: {rpm_anomaly:.0f} RPM",
                "payloadJson": json.dumps({"engineRpm": round(rpm_anomaly, 0)}),
            })

        return events

    # ------------------------------------------------------------------ #
    #  Main snapshot generator                                             #
    # ------------------------------------------------------------------ #

    def generate_snapshot(self) -> dict:
        self.tick += 1
        dt = 2.0  # nominal tick interval in seconds

        # ---- Powertrain ----
        target_speed = self._speed_target()
        self.speed_kph = self._drift(self.speed_kph, target_speed, 0.08, noise=0.5)
        self.speed_kph = self._clamp(self.speed_kph, 0.0, 130.0)

        self.gear = self._compute_gear(self.speed_kph)

        # Engine RPM correlated with speed and gear
        base_rpm = 800.0 + (self.speed_kph / 130.0) * 3200.0 * (7 - self.gear) / 6
        engine_rpm = self._clamp(base_rpm + random.gauss(0, 50), 600.0, 5000.0)

        # Throttle correlated with speed delta
        self.throttle_pct = self._clamp(
            (self.speed_kph / 130.0) * 70.0 + random.gauss(0, 3), 0.0, 100.0
        )

        # Fuel consumption
        fuel_rate_lph = self._clamp(
            (engine_rpm / 3000.0) * 8.0 + random.gauss(0, 0.3), 0.5, 20.0
        )
        fuel_consumed_pct = (fuel_rate_lph / 3600.0) * dt * (100.0 / 60.0)
        self.fuel_level_pct = self._clamp(self.fuel_level_pct - fuel_consumed_pct, 0.0, 100.0)
        if self.fuel_level_pct < 5.0:
            self.fuel_level_pct = 80.0  # refuel

        # Odometer
        distance_km = (self.speed_kph / 3600.0) * dt
        self.odometer_km += distance_km

        # Coolant warms up after cold start, stays between 85-95 normal
        coolant_target = 90.0 if self.tick > 30 else 20.0 + self.tick * 2.5
        self.coolant_temp_c = self._drift(
            self.coolant_temp_c,
            min(coolant_target, 95.0),
            0.05,
            noise=0.2,
        )
        self.coolant_temp_c = self._clamp(self.coolant_temp_c, 20.0, 105.0)

        # ---- Battery ----
        # SOC depletes with load, charges back when very low
        if self.charging_state == "charging":
            soc_delta = (self.charging_power_kw / 80.0) * (dt / 3600.0) * 100.0
            self.soc_pct = self._clamp(self.soc_pct + soc_delta * 50, 0.0, 100.0)
            if self.soc_pct >= 95.0:
                self.charging_state = "not_charging"
                self.charging_power_kw = 0.0
        else:
            load_factor = (self.speed_kph / 130.0) * 0.005
            self.soc_pct = self._clamp(
                self.soc_pct - load_factor - 0.001, 0.0, 100.0
            )
            if self.soc_pct <= 15.0:
                self.charging_state = "charging"
                self.charging_power_kw = random.uniform(7.2, 50.0)

        self.soh_pct = self._clamp(
            self.soh_pct + random.gauss(0, 0.002), 94.0, 100.0
        )

        # Battery temp correlated with load
        batt_temp_target = 22.0 + (self.speed_kph / 130.0) * 15.0
        self.battery_temp_c = self._drift(self.battery_temp_c, batt_temp_target, 0.03, noise=0.1)
        self.battery_temp_c = self._clamp(self.battery_temp_c, 15.0, 50.0)

        estimated_range_km = self._clamp((self.soc_pct / 100.0) * 400.0, 0.0, 400.0)

        # Voltage: higher when charging, lower under heavy load
        if self.charging_state == "charging":
            self.voltage_v = self._drift(self.voltage_v, 14.2, 0.1, noise=0.05)
        else:
            v_target = 12.6 + (self.soc_pct / 100.0) * 1.4
            self.voltage_v = self._drift(self.voltage_v, v_target, 0.05, noise=0.02)
        self.voltage_v = self._clamp(self.voltage_v, 11.0, 14.8)

        current_a = (
            (self.charging_power_kw * 1000.0 / max(self.voltage_v, 0.1))
            if self.charging_state == "charging"
            else -(self.speed_kph / 130.0) * 80.0 - 5.0
        )

        # ---- Chassis ----
        # Steering oscillates gently while driving
        steering_target = 15.0 * math.sin(self.tick * 0.1)
        self.steering_angle_deg = self._drift(self.steering_angle_deg, steering_target, 0.15, noise=0.5)
        self.steering_angle_deg = self._clamp(self.steering_angle_deg, -540.0, 540.0)

        # Brake pedal: mostly 0, occasional moderate braking
        if random.random() < 0.1:
            self.brake_pedal_pct = random.uniform(10.0, 60.0)
        else:
            self.brake_pedal_pct = self._drift(self.brake_pedal_pct, 0.0, 0.3)
        self.brake_pedal_pct = self._clamp(self.brake_pedal_pct, 0.0, 80.0)

        abs_active = self.brake_pedal_pct > 50.0 and self.speed_kph > 30.0 and random.random() < 0.3
        traction_control_active = self.speed_kph < 20.0 and random.random() < 0.05

        # Tire pressures — slow drift with small noise
        self.tire_fl = self._clamp(self.tire_fl + random.gauss(0, 0.3), 200.0, 240.0)
        self.tire_fr = self._clamp(self.tire_fr + random.gauss(0, 0.3), 200.0, 240.0)
        self.tire_rl = self._clamp(self.tire_rl + random.gauss(0, 0.3), 200.0, 240.0)
        self.tire_rr = self._clamp(self.tire_rr + random.gauss(0, 0.3), 200.0, 240.0)

        # ---- Cabin ----
        self.outside_temp_c = self._drift(
            self.outside_temp_c, 15.0 + 5.0 * math.sin(self.tick * 0.005), 0.01, noise=0.05
        )
        self.inside_temp_c = self._drift(self.inside_temp_c, 22.0, 0.02, noise=0.1)
        self.inside_temp_c = self._clamp(self.inside_temp_c, 18.0, 26.0)

        if self.inside_temp_c < 20.0:
            self.hvac_mode = "heat"
            self.fan_speed = 3
        elif self.inside_temp_c > 24.0:
            self.hvac_mode = "cool"
            self.fan_speed = 3
        else:
            self.hvac_mode = "auto"
            self.fan_speed = 2

        # ---- Location ----
        # Circular path around base point
        radius_deg = 0.05
        self.path_angle += 0.001  # advance per tick
        if self.path_angle >= 2.0 * math.pi:
            self.path_angle -= 2.0 * math.pi

        lat = BASE_LAT + radius_deg * math.cos(self.path_angle)
        lon = BASE_LON + radius_deg * math.sin(self.path_angle)

        heading_deg = math.degrees(self.path_angle + math.pi / 2.0) % 360.0

        self.altitude_m = self._drift(self.altitude_m, 40.0, 0.02, noise=0.2)
        self.altitude_m = self._clamp(self.altitude_m, 30.0, 50.0)

        self.accuracy_m = self._clamp(
            self.accuracy_m + random.gauss(0, 0.1), 2.0, 6.0
        )

        geo = geohash.encode(lat, lon, precision=7)

        # ---- ADAS ----
        cruise_enabled = self.speed_kph > 80.0
        if cruise_enabled:
            self.cruise_set_speed_kph = self._clamp(
                round(self.speed_kph / 10.0) * 10.0, 80.0, 150.0
            )
            self.autopilot_mode = "highway_assist"
        else:
            self.cruise_set_speed_kph = 0.0
            self.autopilot_mode = "none"

        collision_warning_active = random.random() < 0.01

        # ---- Anomaly injection ----
        events = self._inject_anomaly()

        # Build snapshot
        snapshot = {
            "vehicle_id": VEHICLE_ID,
            "ts": int(time.time() * 1000),
            "trip_id": TRIP_ID,
            "powertrain": {
                "speedKph": round(self.speed_kph, 1),
                "engineRpm": round(engine_rpm, 0),
                "odometerKm": round(self.odometer_km, 2),
                "fuelLevelPct": round(self.fuel_level_pct, 1),
                "fuelRateLph": round(fuel_rate_lph, 2),
                "coolantTempC": round(self.coolant_temp_c, 1),
                "throttlePct": round(self.throttle_pct, 1),
                "gear": self.gear,
                "ignitionOn": True,
            },
            "battery": {
                "socPct": round(self.soc_pct, 1),
                "sohPct": round(self.soh_pct, 1),
                "batteryTempC": round(self.battery_temp_c, 1),
                "chargingState": self.charging_state,
                "chargingPowerKw": round(self.charging_power_kw, 1),
                "estimatedRangeKm": round(estimated_range_km, 1),
                "voltageV": round(self.voltage_v, 2),
                "currentA": round(current_a, 1),
            },
            "chassis": {
                "steeringAngleDeg": round(self.steering_angle_deg, 1),
                "brakePedalPct": round(self.brake_pedal_pct, 1),
                "tirePressureFlKpa": round(self.tire_fl, 1),
                "tirePressureFrKpa": round(self.tire_fr, 1),
                "tirePressureRlKpa": round(self.tire_rl, 1),
                "tirePressureRrKpa": round(self.tire_rr, 1),
                "absActive": abs_active,
                "tractionControlActive": traction_control_active,
            },
            "cabin": {
                "insideTempC": round(self.inside_temp_c, 1),
                "outsideTempC": round(self.outside_temp_c, 1),
                "hvacMode": self.hvac_mode,
                "fanSpeed": self.fan_speed,
                "driverDoorOpen": False,
                "passengerDoorOpen": False,
                "rearLeftDoorOpen": False,
                "rearRightDoorOpen": False,
                "doorsLocked": True,
                "seatbeltDriverFastened": True,
            },
            "location": {
                "latitude": round(lat, 6),
                "longitude": round(lon, 6),
                "altitudeM": round(self.altitude_m, 1),
                "headingDeg": round(heading_deg, 1),
                "speedKph": round(self.speed_kph, 1),
                "accuracyM": round(self.accuracy_m, 1),
                "geohash": geo,
            },
            "adas": {
                "cruiseEnabled": cruise_enabled,
                "cruiseSetSpeedKph": round(self.cruise_set_speed_kph, 1),
                "laneKeepAssistOn": True,
                "parkingAssistOn": False,
                "collisionWarningActive": collision_warning_active,
                "autopilotMode": self.autopilot_mode,
            },
            "events": events,
        }

        return snapshot
