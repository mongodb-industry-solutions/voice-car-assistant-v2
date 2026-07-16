"""
Generates a full VSS telemetry snapshot with *realistic* values.

The complete VSS `Vehicle` tree (~1300 signals) is emitted verbatim using exact
VSS paths. Values are realistic rather than random-within-domain:

  • Dynamic core — a smooth, correlated drive-cycle simulation drives the signals
    that consumers surface (speed↔gear↔RPM↔throttle, coolant warm-up, fuel/SOC
    depletion, tyre drift, GPS along a path, cruise, HV battery, cabin temps),
    written at their exact VSS paths.
  • Realistic bands — every other signal gets a plausible value via type/name
    heuristics (faults mostly false, temps in sane ranges, enums a "normal"
    default, tight numeric bands) instead of full-domain noise.
  • Fault episodes ("faulty times") — occasionally a subsystem faults: the related
    signal is pushed out of range AND a matching OBD-II code from the file catalog
    (dtc_catalog.json, extracted from values-vss-data.md) is set for a dwell, then
    it recovers. Diagnostics.DTCList is drawn ONLY from that catalog and
    Diagnostics.DTCCount === DTCList.length.

Storage/consumers are unchanged — the tree flows through the opaque JsonToNative
`data` blob to Atlas; the API/dashboard read exact VSS paths.
"""

import json
import math
import os
import random
import string
import time

VEHICLE_ID = "VSS-DEMO-VIN-001"
VIN        = "WBA12345VSS00001"
TRIP_ID    = "trip-001"

META = {
    "vin":                VIN,
    "oem":                "MongoDB",
    "model":              "Leafy 1.0",
    "platform":           "VSS-v4",
    "softwareVersion":    "1.0.0",
    "fuelTankCapacityL":  60.0,
    "batteryCapacityKwh": 75.0,
    "wheelbaseMm":        2875,
    "curbWeightKg":       1800,
    "powertrainType":     "HEV",
    "drivetrainType":     "AWD",
}

BASE_LAT, BASE_LON = 48.8566, 2.3522

_HERE = os.path.dirname(os.path.abspath(__file__))
_SPEC_PATH = os.path.join(_HERE, "vss_model.json")
_DTC_PATH = os.path.join(_HERE, "dtc_catalog.json")

# ── Fault episodes: subsystem → catalog codes (filtered to what the file lists) ──
FAULT_EPISODES = {
    "misfire":      {"codes": ["P0300", "P0301", "P0302", "P0303", "P0304"], "pick": True},
    "coolant":      {"codes": ["P0128"], "pick": False},
    "catalyst":     {"codes": ["P0420", "P0430"], "pick": True},
    "fuel_trim":    {"codes": ["P0171", "P0172"], "pick": True},
    "evap":         {"codes": ["P0442", "P0455"], "pick": True},
    "speed_sensor": {"codes": ["P0500"], "pick": False},
    "transmission": {"codes": ["P0700"], "pick": False},
    "electrical":   {"codes": ["U0001", "U0002", "U0006"], "pick": True},
    "wheel_speed":  {"codes": ["C0021", "C0035", "C0040"], "pick": True},
    # custom codes — not standard OBD-II; added for dashboard tell-tale demonstration
    "overheat":     {"codes": ["P1217"], "pick": False},  # → TEMP tell-tale
    "fuel_low":     {"codes": ["P1001"], "pick": False},  # → FUEL tell-tale
    "batt_low":     {"codes": ["P1002"], "pick": False},  # → BATT tell-tale
    "tpms_warn":    {"codes": ["C1001"], "pick": False},  # → TPMS tell-tale
    "belt_warn":    {"codes": ["B1001"], "pick": False},  # → BELT tell-tale
    "oil_pressure": {"codes": ["P0520"], "pick": False},  # → OIL tell-tale
}
FAULT_DWELL_MIN, FAULT_DWELL_MAX = 15, 45   # ticks (~30–90 s at 2 s/tick)
FAULT_MAX_ACTIVE = 4

_STR_ALPHABET = string.ascii_uppercase + string.digits
_NORMAL_ENUM = {"NORMAL", "OK", "NONE", "INACTIVE", "OFF", "CLEAR", "UNKNOWN",
                "NOT_APPLICABLE", "STOP", "CLOSED", "DRY", "AUTOMATIC"}
_NEG_BOOL = ("error", "defect", "fault", "warn", "worn", "blocked", "overheat",
             "fire", "submers", "broken", "stall", "emergency", "crosswind",
             "icing", "rollover", "deployed", "triggered", "islevellow",
             "isfuellevellow", "isfuellevelempty", "ispressurelow", "isbrakeswom",
             "isdriveremergencybraking")


# ── Realism classification (done once at load) ──────────────────────────────────

def _refine(path: str, spec: dict) -> dict:
    """Annotate a leaf spec with realistic bounds/weights for non-core signals."""
    name = path.lower()
    t = spec.get("t")

    if t == "bool":
        if any(k in name for k in _NEG_BOOL):
            spec["p_true"] = 0.03
        elif "islocked" in name:
            spec["p_true"] = 0.85
        elif "isopen" in name or name.endswith(".open"):
            spec["p_true"] = 0.08
        elif any(k in name for k in ("ison", "isactive", "isengaged", "isenabled",
                                     "isavailable", "isconnected", "ischarging")):
            spec["p_true"] = 0.3
        else:
            spec["p_true"] = 0.35
        return spec

    if t in ("int", "float"):
        lo, hi = spec["min"], spec["max"]
        span = hi - lo
        def band(a, b):
            return (max(lo, a), min(hi, b))
        if "temp" in name:
            if any(k in name for k in ("coolant", "oil", "exhaust", "catalyst", "engine")):
                r = band(70, 100)
            elif "battery" in name or "cell" in name:
                r = band(20, 40)
            else:
                r = band(10, 35)
        elif any(k in name for k in ("angle", "pitch", "roll", "yaw", "tilt", "pan",
                                     "lateral", "longitudinal", "vertical", "steer")):
            r = band(-15, 15)
        elif (lo == 0 and hi == 100) or any(k in name for k in (
                "percent", "level", "position", "support", "intensity", "brightness",
                "volume", "load", "utilization", "fanspeed", "dimming", "recline",
                "soc", "soh", "charge", "humidity", "wear", "friction")):
            r = band(10, 90)
        elif "voltage" in name:
            r = band(11, 14) if hi <= 100 else band(320, 400)
        elif "current" in name:
            r = band(-50, 50) if lo < 0 else band(0, 50)
        elif "pressure" in name:
            r = band(lo, lo + span * 0.05)
        elif any(k in name for k in ("range", "distance", "traveled", "odometer",
                                     "capacity", "hours", "duration")):
            r = band(lo, lo + min(span, 500))
        elif "speed" in name or "rpm" in name:
            r = band(0, min(hi, 120))
        else:
            if lo < 0 < hi:
                w = min(span * 0.05, 50)
                r = band(-w, w)
            else:
                r = band(lo, lo + min(span * 0.15, 100))
        spec["rmin"], spec["rmax"] = r
        return spec

    if t == "enum":
        vals = spec.get("vals") or []
        pref = next((v for v in vals if str(v).upper() in _NORMAL_ENUM), vals[0] if vals else None)
        spec["prefer"] = pref
        return spec

    if t == "enumnum":
        vals = spec.get("vals") or []
        spec["prefer"] = 0 if 0 in vals else (1 if 1 in vals else (vals[0] if vals else None))
        return spec

    return spec


def _load_template(spec_path: str) -> dict:
    with open(spec_path, "r", encoding="utf-8") as f:
        flat = json.load(f)
    template: dict = {}
    for path, spec in flat.items():
        _refine(path, spec)
        node = template
        parts = path.split(".")
        for key in parts[:-1]:
            node = node.setdefault(key, {})
        node[parts[-1]] = spec
    return template


def _load_catalog(path: str) -> dict:
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


_TEMPLATE = _load_template(_SPEC_PATH)
_CATALOG = _load_catalog(_DTC_PATH)
# code → subsystem, for applying fault effects to core signals
_CODE_TO_KIND = {
    c: kind for kind, cfg in FAULT_EPISODES.items()
    for c in cfg["codes"] if c in _CATALOG
}


def _realistic_leaf(spec: dict):
    t = spec.get("t")
    if t == "bool":
        return random.random() < spec.get("p_true", 0.3)
    if t == "int":
        return random.randint(round(spec.get("rmin", spec["min"])),
                              round(spec.get("rmax", spec["max"])))
    if t == "float":
        return round(random.uniform(spec.get("rmin", spec["min"]),
                                    spec.get("rmax", spec["max"])), 2)
    if t == "enum" or t == "enumnum":
        vals = spec.get("vals") or []
        if not vals:
            return None
        pref = spec.get("prefer")
        if pref is not None and random.random() < 0.8:
            return pref
        return random.choice(vals)
    if t == "arr":
        n = random.randint(spec.get("min", 1), min(2, spec.get("max", 2)))
        return [_realistic_leaf(spec["item"]) for _ in range(n)]
    if t == "branch":
        return {}
    return random.choice(["OK", "NORMAL", "N/A"])


def _walk(node):
    if isinstance(node, dict):
        if "t" in node:
            return _realistic_leaf(node)
        return {k: _walk(v) for k, v in node.items()}
    return node


def _set_path(tree: dict, path: str, value) -> None:
    parts = path.split(".")
    node = tree
    for key in parts[:-1]:
        nxt = node.get(key)
        if not isinstance(nxt, dict):
            nxt = {}
            node[key] = nxt
        node = nxt
    node[parts[-1]] = value


class VssGenerator:
    def __init__(self):
        self.tick = 0
        # drive-cycle state
        self.speed = 0.0
        self.fuel_pct = 72.0
        self.coolant = 20.0
        self.oil_pressure = 55.0  # kPa, normal ~40-80
        self.throttle = 0.0
        self.gear = 1
        self.soc = 82.0
        self.soh = 96.5
        self.batt_temp = 24.0
        self.charging = False
        self.charge_kw = 0.0
        self.hv_voltage = 360.0
        self.tire = {"fl": 221.0, "fr": 220.0, "rl": 219.0, "rr": 222.0}
        self.inside_temp = 21.0
        self.outside_temp = 15.0
        self.path_angle = 0.0
        self.altitude = 40.0
        self.brake = 0.0
        self.cruise_set = 0.0
        # faults — pre-seed tick counter mid-cycle so correlated triggers fire sooner
        self.tick = 80
        self.faults: dict = {}          # code -> expiry tick
        # seed one fault immediately so the demo ticker shows a code on first snapshot
        for kind in random.sample(list(FAULT_EPISODES.keys()), 2):
            self._start(kind)
        self.tick = 0  # reset after seeding (expiry values are large, fault persists)

    # ---- helpers ----
    @staticmethod
    def _drift(v, target, rate, noise=0.0):
        v += (target - v) * rate
        if noise:
            v += random.gauss(0, noise)
        return v

    @staticmethod
    def _clamp(v, lo, hi):
        return max(lo, min(hi, v))

    def _speed_target(self):
        c = math.sin(self.tick * 0.02)
        if c > 0.3:
            return 100.0 + c * 30.0
        if c > -0.3:
            return 50.0 + c * 20.0
        return 20.0 + (c + 0.3) * 10.0

    def _gear_for(self, s):
        return 1 if s < 15 else 2 if s < 30 else 3 if s < 50 else 4 if s < 80 else 5 if s < 110 else 6

    # ---- fault engine ----
    def _start(self, kind):
        cfg = FAULT_EPISODES.get(kind)
        codes = [c for c in cfg["codes"] if c in _CATALOG] if cfg else []
        if not codes:
            return
        chosen = [random.choice(codes)] if cfg["pick"] else codes
        for c in chosen:
            if c not in self.faults and len(self.faults) >= FAULT_MAX_ACTIVE:
                continue
            self.faults[c] = self.tick + random.randint(FAULT_DWELL_MIN, FAULT_DWELL_MAX)

    def _update_faults(self, rpm):
        # expire
        self.faults = {c: e for c, e in self.faults.items() if e > self.tick}
        # correlated triggers (values are realistic, so conditions genuinely occur)
        if self.throttle > 60 and rpm > 3500 and random.random() < 0.02:
            self._start("misfire")
        if self.soc < 20 and random.random() < 0.03:
            self._start("electrical")
        if self.brake > 60 and self.speed > 40 and random.random() < 0.05:
            self._start("wheel_speed")
        # baseline: any episode fires ~every 30-60 ticks so the demo always has active codes
        if random.random() < 0.04:
            self._start(random.choice(list(FAULT_EPISODES)))
        active_kinds = {_CODE_TO_KIND[c] for c in self.faults if c in _CODE_TO_KIND}
        return active_kinds

    # ---- core drive-cycle ----
    def _core(self) -> dict:
        dt = 2.0
        # speed / gear / rpm / throttle
        self.speed = self._clamp(self._drift(self.speed, self._speed_target(), 0.08, 0.5), 0.0, 130.0)
        self.gear = self._gear_for(self.speed)
        self.throttle = self._clamp((self.speed / 130.0) * 70.0 + random.gauss(0, 3), 0.0, 100.0)
        rpm = self._clamp(900.0 + self.speed * 20.0 + self.throttle * 12.0 + random.gauss(0, 80), 700.0, 6000.0)

        # fuel
        rate = self._clamp((rpm / 3000.0) * 8.0 + random.gauss(0, 0.3), 0.5, 20.0)
        self.fuel_pct = self._clamp(self.fuel_pct - (rate / 3600.0) * dt * (100.0 / 60.0), 0.0, 100.0)
        if self.fuel_pct < 5.0:
            self.fuel_pct = 80.0

        # coolant warm-up
        target = 90.0 if self.tick > 30 else 20.0 + self.tick * 2.5
        self.coolant = self._clamp(self._drift(self.coolant, min(target, 95.0), 0.05, 0.2), 20.0, 115.0)

        # HV battery
        if self.charging:
            self.soc = self._clamp(self.soc + (self.charge_kw / 75.0) * (dt / 3600.0) * 100.0 * 40, 0.0, 100.0)
            if self.soc >= 95.0:
                self.charging = False
                self.charge_kw = 0.0
        else:
            self.soc = self._clamp(self.soc - (self.speed / 130.0) * 0.05 - 0.005, 0.0, 100.0)
            if self.soc <= 15.0:
                self.charging = True
                self.charge_kw = random.uniform(7.2, 50.0)
        self.soh = self._clamp(self.soh + random.gauss(0, 0.002), 94.0, 100.0)
        self.batt_temp = self._clamp(self._drift(self.batt_temp, 24.0 + (self.speed / 130.0) * 15.0, 0.03, 0.1), 15.0, 50.0)
        self.hv_voltage = self._clamp(self._drift(self.hv_voltage, 320.0 + (self.soc / 100.0) * 80.0, 0.05, 0.3), 300.0, 410.0)
        hv_current = (self.charge_kw * 1000.0 / max(self.hv_voltage, 1.0)) if self.charging else -(self.speed / 130.0) * 150.0 - 5.0
        est_range = round((self.soc / 100.0) * 380.0)
        fuel_range = round((self.fuel_pct / 100.0) * META["fuelTankCapacityL"] * 14.0)

        # tyres
        for k in self.tire:
            self.tire[k] = self._clamp(self.tire[k] + random.gauss(0, 0.3), 200.0, 240.0)

        # brake + ABS/TCS events
        self.brake = random.uniform(10, 60) if random.random() < 0.1 else self._clamp(self._drift(self.brake, 0.0, 0.3), 0.0, 80.0)
        abs_evt = self.brake > 50 and self.speed > 30 and random.random() < 0.3
        tcs_evt = self.speed < 20 and random.random() < 0.05

        # cabin / exterior
        self.outside_temp = self._drift(self.outside_temp, 15.0 + 5.0 * math.sin(self.tick * 0.005), 0.01, 0.05)
        self.inside_temp = self._clamp(self._drift(self.inside_temp, 22.0, 0.02, 0.1), 18.0, 26.0)

        # location on a circular path
        self.path_angle = (self.path_angle + 0.001) % (2 * math.pi)
        lat = BASE_LAT + 0.05 * math.cos(self.path_angle)
        lon = BASE_LON + 0.05 * math.sin(self.path_angle)
        heading = math.degrees(self.path_angle + math.pi / 2.0) % 360.0
        self.altitude = self._clamp(self._drift(self.altitude, 40.0, 0.02, 0.2), 30.0, 50.0)

        # cruise
        cruise = self.speed > 80.0
        self.cruise_set = round(self.speed / 10.0) * 10.0 if cruise else 0.0

        # ---- fault effects ----
        active = self._update_faults(rpm)
        if "misfire" in active:
            rpm = self._clamp(rpm + random.gauss(0, 350), 500, 6000)
            self.throttle = self._clamp(self.throttle + random.gauss(0, 10), 0, 100)
        if "coolant" in active:
            self.coolant = self._clamp(self.coolant - random.uniform(20, 35), 20, 115)  # stuck cold (P0128)
        if "electrical" in active:
            self.hv_voltage = self._clamp(self.hv_voltage - random.uniform(20, 45), 250, 410)
        if "speed_sensor" in active and random.random() < 0.5:
            self.speed = 0.0
        if "transmission" in active:
            self.gear = 0
        if "wheel_speed" in active:
            abs_evt = True
        # custom fault effects — push the matching sensor into the warning range
        if "overheat" in active:
            self.coolant = self._clamp(self.coolant + random.uniform(15, 25), 20, 115)  # → above 100°C (P1217)
        if "fuel_low" in active:
            self.fuel_pct = self._clamp(self.fuel_pct - random.uniform(8, 18), 0, 100)  # → below 20% (P1001)
        if "batt_low" in active:
            self.soc = self._clamp(self.soc - random.uniform(5, 12), 0, 100)  # → below 25% (P1002)
        if "tpms_warn" in active:
            k = random.choice(list(self.tire.keys()))
            self.tire[k] = self._clamp(self.tire[k] - random.uniform(20, 40), 150, 240)  # → below 193 kPa (C1001)
        belted = not ("belt_warn" in active)  # B1001 — unbelted during episode
        self.oil_pressure = self._clamp(self._drift(self.oil_pressure, 55.0, 0.05, 0.3), 10.0, 90.0)
        if "oil_pressure" in active:
            self.oil_pressure = self._clamp(self.oil_pressure - random.uniform(10, 25), 10.0, 90.0)  # → below 35 kPa (P0520)

        codes = sorted(self.faults.keys())

        return {
            "Speed": round(self.speed, 1),
            "IsMoving": self.speed > 1.0,
            "TraveledDistance": self.tick * 30,
            "Powertrain.CombustionEngine.Speed": round(rpm, 0),
            "Powertrain.CombustionEngine.IsRunning": True,
            "Powertrain.CombustionEngine.TPS": round(self.throttle),
            "Powertrain.CombustionEngine.EngineCoolant.Temperature": round(self.coolant, 1),
            "Powertrain.Transmission.CurrentGear": self.gear,
            "Powertrain.Transmission.SelectedGear": self.gear,
            "Powertrain.FuelSystem.RelativeLevel": round(self.fuel_pct),
            "Powertrain.FuelSystem.AbsoluteLevel": round(self.fuel_pct / 100.0 * META["fuelTankCapacityL"], 1),
            "Powertrain.FuelSystem.Range": fuel_range,
            "Powertrain.FuelSystem.InstantConsumption": round(rate / max(self.speed, 1.0) * 100.0, 1),
            "Powertrain.TractionBattery.StateOfCharge.Current": round(self.soc, 1),
            "Powertrain.TractionBattery.StateOfCharge.Displayed": round(self.soc, 1),
            "Powertrain.TractionBattery.StateOfHealth": round(self.soh, 1),
            "Powertrain.TractionBattery.Range": est_range,
            "Powertrain.TractionBattery.CurrentVoltage": round(self.hv_voltage, 1),
            "Powertrain.TractionBattery.CurrentCurrent": round(hv_current, 1),
            "Powertrain.TractionBattery.Temperature.Average": round(self.batt_temp, 1),
            "Powertrain.TractionBattery.Charging.IsCharging": self.charging,
            "Powertrain.TractionBattery.Charging.ChargeRate": round(self.charge_kw, 1),
            "Chassis.Axle.Row1.Wheel.Left.Tire.Pressure": round(self.tire["fl"], 1),
            "Chassis.Axle.Row1.Wheel.Right.Tire.Pressure": round(self.tire["fr"], 1),
            "Chassis.Axle.Row2.Wheel.Left.Tire.Pressure": round(self.tire["rl"], 1),
            "Chassis.Axle.Row2.Wheel.Right.Tire.Pressure": round(self.tire["rr"], 1),
            "Chassis.Brake.PedalPosition": round(self.brake),
            "ADAS.ABS.IsEngaged": abs_evt,
            "ADAS.ABS.IsEnabled": True,
            "ADAS.TCS.IsEngaged": tcs_evt,
            "ADAS.TCS.IsEnabled": True,
            "ADAS.CruiseControl.IsActive": cruise,
            "ADAS.CruiseControl.IsEnabled": cruise,
            "ADAS.CruiseControl.SpeedSet": round(self.cruise_set, 1),
            "Cabin.Seat.Row1.DriverSide.IsBelted": belted,
            "Powertrain.CombustionEngine.OilPressure": round(self.oil_pressure, 1),
            "Cabin.HVAC.AmbientAirTemperature": round(self.inside_temp, 1),
            "Exterior.AirTemperature": round(self.outside_temp, 1),
            "CurrentLocation.Latitude": round(lat, 6),
            "CurrentLocation.Longitude": round(lon, 6),
            "CurrentLocation.Heading": round(heading, 1),
            "CurrentLocation.Altitude": round(self.altitude, 1),
            "Diagnostics.DTCCount": len(codes),
            "Diagnostics.DTCList": codes,
        }

    def generate_snapshot(self) -> dict:
        self.tick += 1
        core = self._core()

        tree = {domain: _walk(sub) for domain, sub in _TEMPLATE.items()}
        for path, value in core.items():
            _set_path(tree, path, value)

        snapshot = {
            "vehicle_id": VEHICLE_ID,
            "ts": int(time.time() * 1000),
            "trip_id": TRIP_ID,
            "meta": META,
        }
        snapshot.update(tree)
        return snapshot
