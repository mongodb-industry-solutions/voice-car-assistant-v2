"""
Generates a full VSS (Vehicle Signal Specification) telemetry snapshot.

The complete VSS `Vehicle` tree (~1300 signals) is emitted verbatim using exact
VSS paths. Every leaf is randomized within its domain each tick, driven by the
preprocessed spec in vss_model.json (produced by gen_vss_spec.py from
values-vss-data.md).

The snapshot `data` (everything except the envelope) IS the VSS Vehicle node's
contents, e.g. data.Powertrain.CombustionEngine.Speed, data.Chassis.Axle.Row1...
The vss-telemetry-service stores it unchanged in the opaque JsonToNative `data`
blob, so no ObjectBox/C++ change is needed for the full model.

NOTE (Phase 1): downstream consumers (Atlas trigger, telemetry API, dashboard,
agent) still read the old flat camelCase keys and will not find data until they
are rewired to VSS paths in Phase 2.
"""

import json
import os
import random
import string
import time

VEHICLE_ID = "VSS-DEMO-VIN-001"
VIN        = "WBA12345VSS00001"
TRIP_ID    = "trip-001"

# Static vehicle metadata — rides alongside the VSS tree as the snapshot `meta`
# sibling (VSS has no VIN/OEM identity node), stored as objectbox_telemetry.meta.
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

_SPEC_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "vss_model.json")


def _load_template(spec_path: str) -> dict:
    """
    Load the flat {path: spec} map and build a nested template mirroring the VSS
    tree. Leaves are the spec dicts (identified by their "t" key); branches are
    plain nested dicts. Built once at import.
    """
    with open(spec_path, "r", encoding="utf-8") as f:
        flat = json.load(f)

    template: dict = {}
    for path, spec in flat.items():
        parts = path.split(".")
        node = template
        for key in parts[:-1]:
            node = node.setdefault(key, {})
        node[parts[-1]] = spec
    return template


_TEMPLATE = _load_template(_SPEC_PATH)

_STR_ALPHABET = string.ascii_uppercase + string.digits


def _rand_leaf(spec: dict):
    """Produce a random value for one leaf spec, within its domain."""
    t = spec.get("t")
    if t == "bool":
        return random.random() < 0.5
    if t == "int":
        return random.randint(spec["min"], spec["max"])
    if t == "float":
        return round(random.uniform(spec["min"], spec["max"]), 2)
    if t == "enum" or t == "enumnum":
        vals = spec.get("vals") or []
        return random.choice(vals) if vals else None
    if t == "arr":
        n = random.randint(spec.get("min", 1), spec.get("max", 3))
        return [_rand_leaf(spec["item"]) for _ in range(n)]
    if t == "branch":
        return {}
    # free-form string
    return "".join(random.choices(_STR_ALPHABET, k=6))


def _gen(node):
    """Recursively realize a template node into random values."""
    if isinstance(node, dict):
        # Leaf spec dicts carry a "t" key; branch nodes are PascalCase VSS keys.
        if "t" in node:
            return _rand_leaf(node)
        return {k: _gen(v) for k, v in node.items()}
    return node


class VssGenerator:
    def __init__(self):
        self.tick = 0

    def generate_snapshot(self) -> dict:
        self.tick += 1
        # data = the VSS Vehicle node's contents (top-level: ADAS, Body, Powertrain, ...)
        tree = {domain: _gen(sub) for domain, sub in _TEMPLATE.items()}

        snapshot = {
            "vehicle_id": VEHICLE_ID,
            "ts": int(time.time() * 1000),
            "trip_id": TRIP_ID,
            "meta": META,
        }
        # The vss-telemetry-service strips the envelope (vehicle_id/ts/trip_id/meta)
        # and stores the remaining keys as `data` — i.e. the VSS tree.
        snapshot.update(tree)
        return snapshot
