"""
Navigation Service - Natural language route planning
Parses intent with Ollama, finds POIs via Overpass API, routes via OSRM
"""

import json
import math
import os

import ollama
import requests
from flask import Flask, jsonify, request
from flask_cors import CORS

app = Flask(__name__)
CORS(app)


@app.errorhandler(Exception)
def handle_exception(e):
    """Catch any unhandled exception and return JSON instead of an HTML 500 page."""
    import traceback
    traceback.print_exc()
    return jsonify({"error": str(e) or "Internal server error"}), 500

OVERPASS_URL = "https://overpass-api.de/api/interpreter"
OSRM_URL = "https://router.project-osrm.org/route/v1/driving"
NOMINATIM_URL = "https://nominatim.openstreetmap.org/search"
OLLAMA_HOST = os.getenv("OLLAMA_HOST", "http://localhost:11434")
LLM_MODEL = os.getenv("LLM_MODEL", "llama3.2")
HEADERS = {"User-Agent": "VoiceCarAssistant/2.0"}

# OSM tag mappings for common destination types
OSM_TAGS = {
    "gas station": {"amenity": "fuel"},
    "fuel": {"amenity": "fuel"},
    "petrol station": {"amenity": "fuel"},
    "charging station": {"amenity": "charging_station"},
    "restaurant": {"amenity": "restaurant"},
    "cafe": {"amenity": "cafe"},
    "coffee shop": {"amenity": "cafe"},
    "coffee": {"amenity": "cafe"},
    "fast food": {"amenity": "fast_food"},
    "burger": {"amenity": "fast_food"},
    "bar": {"amenity": "bar"},
    "pub": {"amenity": "pub"},
    "hospital": {"amenity": "hospital"},
    "pharmacy": {"amenity": "pharmacy"},
    "clinic": {"amenity": "clinic"},
    "doctor": {"amenity": "clinic"},
    "parking": {"amenity": "parking"},
    "atm": {"amenity": "atm"},
    "bank": {"amenity": "bank"},
    "post office": {"amenity": "post_office"},
    "supermarket": {"shop": "supermarket"},
    "grocery": {"shop": "grocery"},
    "hotel": {"tourism": "hotel"},
    "motel": {"tourism": "motel"},
    "car wash": {"amenity": "car_wash"},
    "mechanic": {"shop": "car_repair"},
    "car repair": {"shop": "car_repair"},
}


def _keyword_fallback(query: str) -> dict:
    """Keyword-based intent extraction used when Ollama is unavailable.

    Scans the query for known destination type strings (longest match first to
    avoid 'car' matching inside 'car wash') so that e.g. 'nearest gas station'
    still resolves to the right Overpass tag instead of a Nominatim search.
    """
    q = query.lower()
    for type_name in sorted(OSM_TAGS, key=len, reverse=True):
        if type_name in q:
            return {"destination_type": type_name, "destination_name": None, "preference": "nearest"}
    # Nothing matched — treat the whole query as a named place
    return {"destination_type": "custom", "destination_name": query, "preference": "nearest"}


def parse_navigation_intent(query: str) -> dict:
    """Use Ollama to extract structured destination info from natural language.
    Falls back to keyword extraction if Ollama is unreachable.
    """
    prompt = f"""Extract the navigation destination from this voice request. Return ONLY a JSON object, no explanation.

Request: "{query}"

Return JSON with:
- "destination_type": category like "gas station", "restaurant", "hospital", "coffee shop", "supermarket", "pharmacy", "hotel", "parking", "fast food", "atm", "bank", or "custom" for a specific named place
- "destination_name": the specific place name if mentioned (e.g. "McDonald's", "Central Park"), otherwise null
- "preference": any user preference like "nearest", "cheapest", "open now", or null

Examples:
- "take me to the nearest gas station" → {{"destination_type": "gas station", "destination_name": null, "preference": "nearest"}}
- "navigate to McDonald's" → {{"destination_type": "fast food", "destination_name": "McDonald's", "preference": null}}
- "find a coffee shop nearby" → {{"destination_type": "coffee shop", "destination_name": null, "preference": "nearest"}}
- "directions to Central Park" → {{"destination_type": "custom", "destination_name": "Central Park", "preference": null}}

JSON:"""

    try:
        client = ollama.Client(host=OLLAMA_HOST)
        response = client.generate(model=LLM_MODEL, prompt=prompt)
        text = response["response"].strip()
        start = text.find("{")
        end = text.rfind("}") + 1
        if start >= 0 and end > start:
            return json.loads(text[start:end])
    except Exception as e:
        print(f"Intent parsing error (falling back to keywords): {e}")

    return _keyword_fallback(query)


def search_pois_overpass(lat: float, lon: float, osm_key: str, osm_value: str, radius: int = 10000) -> list:
    """Query Overpass API for nearby POIs by OSM tag."""
    query = f"""
[out:json][timeout:25];
(
  node["{osm_key}"="{osm_value}"](around:{radius},{lat},{lon});
  way["{osm_key}"="{osm_value}"](around:{radius},{lat},{lon});
);
out center body;
"""
    try:
        response = requests.post(OVERPASS_URL, data=query, headers=HEADERS, timeout=30)
        if not response.ok:
            return []

        pois = []
        for el in response.json().get("elements", []):
            tags = el.get("tags", {})
            if el["type"] == "node":
                poi_lat, poi_lon = el["lat"], el["lon"]
            elif el["type"] == "way" and "center" in el:
                poi_lat, poi_lon = el["center"]["lat"], el["center"]["lon"]
            else:
                continue

            addr_parts = [tags.get("addr:street", ""), tags.get("addr:housenumber", ""), tags.get("addr:city", "")]
            address = ", ".join(p for p in addr_parts if p)

            pois.append({
                "name": tags.get("name", f"{osm_value.replace('_', ' ').title()} (unnamed)"),
                "lat": poi_lat,
                "lon": poi_lon,
                "address": address,
                "distance_m": haversine(lat, lon, poi_lat, poi_lon),
            })

        pois.sort(key=lambda x: x["distance_m"])
        return pois[:5]

    except Exception as e:
        print(f"Overpass error: {e}")
        return []


def search_pois_nominatim(lat: float, lon: float, name: str) -> list:
    """Use Nominatim to geocode a specific named place."""
    try:
        params = {"q": name, "format": "json", "limit": 5, "lat": lat, "lon": lon}
        response = requests.get(NOMINATIM_URL, params=params, headers=HEADERS, timeout=10)
        if not response.ok:
            return []

        results = []
        for r in response.json()[:3]:
            display = r.get("display_name", name)
            results.append({
                "name": display.split(",")[0].strip(),
                "lat": float(r["lat"]),
                "lon": float(r["lon"]),
                "address": display,
                "distance_m": haversine(lat, lon, float(r["lat"]), float(r["lon"])),
            })
        results.sort(key=lambda x: x["distance_m"])
        return results

    except Exception as e:
        print(f"Nominatim error: {e}")
        return []


def get_route(origin_lat: float, origin_lon: float, dest_lat: float, dest_lon: float) -> dict | None:
    """Calculate driving route using OSRM."""
    url = f"{OSRM_URL}/{origin_lon},{origin_lat};{dest_lon},{dest_lat}"
    params = {"steps": "true", "geometries": "geojson", "overview": "full"}

    try:
        response = requests.get(url, params=params, headers=HEADERS, timeout=15)
        if not response.ok:
            return None

        data = response.json()
        if data.get("code") != "Ok" or not data.get("routes"):
            return None

        route = data["routes"][0]
        leg = route["legs"][0]

        steps = []
        for step in leg.get("steps", []):
            maneuver = step.get("maneuver", {})
            m_type = maneuver.get("type", "")
            modifier = maneuver.get("modifier", "")
            name = step.get("name", "")
            distance = step.get("distance", 0)

            if m_type == "depart":
                text = f"Head {modifier} on {name}" if name else "Depart"
            elif m_type == "arrive":
                text = "Arrive at destination"
            elif m_type == "turn":
                text = f"Turn {modifier}" + (f" onto {name}" if name else "")
            elif m_type == "continue":
                text = f"Continue on {name}" if name else "Continue straight"
            elif m_type == "roundabout":
                exit_num = maneuver.get("exit", "")
                text = f"Take exit {exit_num} at roundabout" + (f" onto {name}" if name else "")
            else:
                text = m_type.replace("_", " ").title()
                if modifier:
                    text += f" {modifier}"
                if name:
                    text += f" onto {name}"

            steps.append({
                "instruction": text,
                "distance_m": distance,
                "distance_text": fmt_distance(distance),
            })

        return {
            "geometry": route["geometry"],
            "distance_m": route["distance"],
            "distance_text": fmt_distance(route["distance"]),
            "duration_s": route["duration"],
            "duration_text": fmt_duration(route["duration"]),
            "steps": steps,
        }

    except Exception as e:
        print(f"OSRM error: {e}")
        return None


def haversine(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    R = 6_371_000
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
    return 2 * R * math.asin(math.sqrt(a))


def fmt_distance(meters: float) -> str:
    if meters < 1000:
        return f"{int(meters)} m"
    return f"{meters / 1000:.1f} km"


def fmt_duration(seconds: float) -> str:
    minutes = int(seconds / 60)
    if minutes < 60:
        return f"{minutes} min"
    h, m = divmod(minutes, 60)
    return f"{h}h {m}min" if m else f"{h}h"


@app.route("/health")
def health():
    return jsonify({"status": "ok", "service": "navigation-service"})


@app.route("/navigate", methods=["POST"])
def navigate():
    data = request.json or {}
    query = data.get("query", "").strip()
    lat = data.get("lat")
    lon = data.get("lon")

    if not query or lat is None or lon is None:
        return jsonify({"error": "query, lat, and lon are required"}), 400

    print(f"Navigation: '{query}' from ({lat:.4f}, {lon:.4f})")

    # 1. Parse intent with LLM
    intent = parse_navigation_intent(query)
    dest_type = intent.get("destination_type", "custom")
    dest_name = intent.get("destination_name")
    print(f"  Intent → type={dest_type!r}, name={dest_name!r}")

    # 2. Find nearby POIs
    if dest_name:
        pois = search_pois_nominatim(lat, lon, dest_name)
        if not pois and dest_type in OSM_TAGS:
            tag = OSM_TAGS[dest_type]
            k, v = next(iter(tag.items()))
            pois = search_pois_overpass(lat, lon, k, v)
    elif dest_type in OSM_TAGS:
        tag = OSM_TAGS[dest_type]
        k, v = next(iter(tag.items()))
        pois = search_pois_overpass(lat, lon, k, v)
    else:
        pois = search_pois_nominatim(lat, lon, dest_type)

    if not pois:
        return jsonify({"error": f"No {dest_type} found nearby"}), 404

    destination = pois[0]
    print(f"  Destination → {destination['name']} ({destination['distance_m']:.0f} m)")

    # 3. Calculate route
    route = get_route(lat, lon, destination["lat"], destination["lon"])
    if not route:
        return jsonify({"error": "Could not calculate a driving route"}), 500

    return jsonify({
        "destination": destination,
        "nearby_options": pois,
        "route": route,
        "intent": intent,
    })


if __name__ == "__main__":
    port = int(os.getenv("PORT", 5001))
    app.run(host="0.0.0.0", port=port, debug=False)
