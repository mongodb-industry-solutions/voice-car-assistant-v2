"""
Preprocess the VSS domain spec into a flat, machine-readable JSON the simulator
loads at runtime.

Input  : values-vss-data.md  (the full VSS `Vehicle` tree annotated with value
         domains — ranges/enums — plus YAML anchors/merge keys, which PyYAML
         resolves natively).
Output : vss_model.json       (flat map: "Dotted.VSS.Path" -> value spec).

Each leaf's free-form domain string (e.g. "uint8 [0..255]", "boolean {true,false}",
"string {'A','B'}", "float [0..100]", "string[] each from {...}") is parsed into a
compact spec the generator can turn into a random value:

    {"t":"bool"}
    {"t":"int",  "min":0, "max":255}
    {"t":"float","min":0.0, "max":100.0}
    {"t":"enum", "vals":["A","B"]}          # string enum
    {"t":"enumnum","vals":[0,1,2]}          # numeric enum
    {"t":"str"}                             # free-form string
    {"t":"arr",  "item":{...}, "min":1, "max":3}
    {"t":"branch"}                          # placeholder branch -> {}

Run once (and whenever values-vss-data.md changes):
    python gen_vss_spec.py            # reads ../values-vss-data.md, writes vss_model.json
"""

import json
import os
import re
import sys

try:
    import yaml
except ImportError:
    sys.exit("PyYAML required: pip install pyyaml")

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_IN = os.path.join(HERE, "..", "values-vss-data.md")
DEFAULT_OUT = os.path.join(HERE, "vss_model.json")
# DTC catalog is emitted here (used by the generator) and mirrored into the
# dashboard's static dir (used by the UI diagnostics panel).
DTC_OUT = os.path.join(HERE, "dtc_catalog.json")
DTC_OUT_UI = os.path.join(HERE, "..", "voice-assistant-backend", "static", "dtc_catalog.json")

_DTC_CODE_RE = re.compile(r"^[PCBU][0-9A-F]{4}$")

# DTCReference is documentation (code→meaning tables), not real VSS signals — skip it.
SKIP_SUBPATHS = {"Diagnostics.DTCReference"}

# Type ranges used when a numeric domain gives no explicit [min..max].
_INT_DEFAULTS = {
    "uint8":  (0, 255),
    "uint16": (0, 65535),
    "uint32": (0, 4294967295),
    "int8":   (-128, 127),
    "int16":  (-32768, 32767),
    "int32":  (-2147483648, 2147483647),
}
_INT_BASES = set(_INT_DEFAULTS)
_FLOAT_BASES = {"float", "double"}

_RANGE_RE = re.compile(r"\[\s*(-?\d+(?:\.\d+)?)\s*\.\.\s*(-?\d+(?:\.\d+)?)\s*\]")
_QUOTED_RE = re.compile(r"'([^']*)'")
_NUMENUM_RE = re.compile(r":\s*(-?\d+)")


def _base_type(domain: str) -> str:
    """First bare word of the domain string, e.g. 'uint8 [0..255]' -> 'uint8'."""
    m = re.match(r"\s*([A-Za-z0-9]+)", domain)
    return m.group(1).lower() if m else ""


def _numeric_spec(base: str, domain: str) -> dict:
    """Build an int/float spec from a base type + optional [min..max] (∞ tolerated)."""
    is_float = base in _FLOAT_BASES
    m = _RANGE_RE.search(domain)
    if m:
        lo, hi = m.group(1), m.group(2)
        if is_float:
            return {"t": "float", "min": float(lo), "max": float(hi)}
        return {"t": "int", "min": int(float(lo)), "max": int(float(hi))}
    # "[0..∞)" style — take the lower bound, cap the upper at a sane value.
    m2 = re.search(r"\[\s*(-?\d+(?:\.\d+)?)\s*\.\.\s*[∞)]", domain)
    lo = float(m2.group(1)) if m2 else None
    if is_float:
        return {"t": "float", "min": (lo if lo is not None else 0.0), "max": 1000.0}
    dlo, dhi = _INT_DEFAULTS.get(base, (0, 1000))
    return {"t": "int", "min": (int(lo) if lo is not None else dlo), "max": dhi}


def _parse_leaf(domain) -> dict:
    """Turn one leaf domain value into a generation spec (best-effort)."""
    if not isinstance(domain, str):
        return {"t": "str"}
    d = domain.strip()
    low = d.lower()

    if low.startswith("branch"):
        return {"t": "branch"}

    # Arrays: "<base>[] ..." (e.g. "string[] each from {...}", "uint8[] (... 0..255)")
    arr_m = re.match(r"\s*([A-Za-z0-9]+)\s*\[\s*\]", d)
    if arr_m:
        base = arr_m.group(1).lower()
        rest = d[arr_m.end():]
        quoted = _QUOTED_RE.findall(rest)
        if base == "string" and quoted:
            item = {"t": "enum", "vals": quoted}
        elif base == "string":
            item = {"t": "str"}
        elif base in _INT_BASES or base in _FLOAT_BASES:
            item = _numeric_spec(base, rest)
        else:
            item = {"t": "str"}
        return {"t": "arr", "item": item, "min": 1, "max": 3}

    base = _base_type(d)

    if base in ("boolean", "bool"):
        return {"t": "bool"}

    # Enumerations in { ... }
    if "{" in d:
        inside = d[d.find("{") + 1: d.rfind("}")] if "}" in d else ""
        quoted = _QUOTED_RE.findall(inside)
        if quoted:
            # e.g. "string {'START','STOP'}" — but "formatted like '9.2:1'" also matches;
            # a single quoted token just becomes a (near-)constant, which is fine.
            return {"t": "enum", "vals": quoted}
        nums = _NUMENUM_RE.findall(inside)
        if nums and "..." not in inside:
            # e.g. "uint8 {UNKNOWN:0,NONE:1,RAIN:2,...}"
            return {"t": "enumnum", "vals": [int(n) for n in nums]}
        # "int8 {...,-2,-1,0,1,2,...}" — open-ended → treat as plain numeric.
        if base in _INT_BASES or base in _FLOAT_BASES:
            return _numeric_spec(base, d)

    if base in _INT_BASES or base in _FLOAT_BASES:
        return _numeric_spec(base, d)

    # string (free-form / ISO 8601 / "formatted like ...") and anything unrecognized
    return {"t": "str"}


def flatten(node, prefix, out):
    if isinstance(node, dict):
        for key, val in node.items():
            path = f"{prefix}.{key}" if prefix else str(key)
            # Strip the leading "Vehicle." — `data` is the Vehicle node's contents.
            rel = path[len("Vehicle."):] if path.startswith("Vehicle.") else path
            if any(rel == s or rel.startswith(s + ".") for s in SKIP_SUBPATHS):
                continue
            flatten(val, path, out)
    else:
        rel = prefix[len("Vehicle."):] if prefix.startswith("Vehicle.") else prefix
        out[rel] = _parse_leaf(node)


def extract_dtc_catalog(doc) -> dict:
    """
    Collect the official OBD-II codes listed under
    Vehicle.Diagnostics.DTCReference.GenericExamples → { "P0128": "description", ... }.
    These are the only codes the simulator may emit.
    """
    node = (((doc.get("Vehicle") or {}).get("Diagnostics") or {})
            .get("DTCReference") or {}).get("GenericExamples") or {}
    catalog = {}

    def walk(n):
        if isinstance(n, dict):
            for k, v in n.items():
                if isinstance(v, str) and _DTC_CODE_RE.match(str(k)):
                    catalog[str(k)] = v
                else:
                    walk(v)

    walk(node)
    return catalog


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IN
    dst = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT

    with open(src, "r", encoding="utf-8") as f:
        doc = yaml.safe_load(f)

    vehicle = doc.get("Vehicle")
    if not isinstance(vehicle, dict):
        sys.exit("No 'Vehicle' mapping found in input")

    flat = {}
    flatten(vehicle, "Vehicle", flat)

    with open(dst, "w", encoding="utf-8") as f:
        json.dump(flat, f, indent=1, sort_keys=True)

    # DTC catalog → generator copy + dashboard static copy (kept in sync from here).
    catalog = extract_dtc_catalog(doc)
    for path in (DTC_OUT, DTC_OUT_UI):
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(catalog, f, indent=1, sort_keys=True)
        except OSError as e:
            print(f"  (skip {path}: {e})")

    # Summary
    from collections import Counter
    kinds = Counter(spec["t"] for spec in flat.values())
    print(f"Wrote {dst}")
    print(f"  leaves: {len(flat)}")
    print(f"  by kind: {dict(kinds)}")
    print(f"Wrote {DTC_OUT}  ({len(catalog)} DTC codes)")


if __name__ == "__main__":
    main()
