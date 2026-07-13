/**
 * assembleTelemetry — Atlas trigger on `objectbox_telemetry` inserts.
 *
 * The vss-telemetry-service stores each snapshot as one `objectbox_telemetry` document
 * with two JsonToNative fields (nested objects; may also arrive as raw JSON strings):
 *   - `data` : the domain payload (powertrain/battery/chassis/cabin/location/adas)
 *   - `meta` : the vehicle metadata (sibling of data — no separate VehicleMeta collection)
 * This function reshapes them into the app-facing collections:
 *   - telemetry-data   : append every fire (~2 s)  — time-series history
 *   - telemetry-status : upsert at most every 10 s — current state (read by the API)
 */
exports = async function (changeEvent) {
  const fullDoc = changeEvent.fullDocument;
  if (!fullDoc) return;

  // `data`/`meta` are JsonToNative → nested objects; tolerate raw strings too.
  let data = fullDoc.data;
  if (typeof data === "string") { try { data = JSON.parse(data); } catch (_) { data = {}; } }
  data = data || {};

  let meta = fullDoc.meta;
  if (typeof meta === "string") { try { meta = JSON.parse(meta); } catch (_) { meta = null; } }

  const vehicleId = fullDoc.vehicleId || data.vehicle_id;
  if (!vehicleId) return;

  const dbName = context.values.get("DATABASE_NAME");
  const db = context.services.get("mongodb-atlas").db(dbName);

  // Location: the snapshot carries latitude/longitude; rebuild a GeoJSON Point
  // (lng-first, RFC 7946) so downstream consumers keep working unchanged.
  let location = null;
  if (data.location) {
    location = { ...data.location };
    if (location.latitude != null && location.longitude != null) {
      location.locationGeoJson = { type: "Point", coordinates: [location.longitude, location.latitude] };
    }
  }

  const now = new Date();
  const unified = {
    vehicleId,
    timestamp: now,
    meta: meta || null,
    data: {
      powertrain: data.powertrain || null,
      battery:    data.battery    || null,
      chassis:    data.chassis    || null,
      cabin:      data.cabin      || null,
      location:   location,
      adas:       data.adas       || null,
      diagnostics: data.diagnostics || null,
    },
  };

  // Append to the time-series history on every fire (~every 2 s).
  await db.collection("telemetry-data").insertOne(unified);

  // Upsert current state at most once every 10 s.
  const statusColl = db.collection("telemetry-status");
  const current = await statusColl.findOne({ vehicleId }, { projection: { lastUpdated: 1 } });
  const elapsed =
    current && current.lastUpdated
      ? now.getTime() - new Date(current.lastUpdated).getTime()
      : Infinity;

  if (elapsed >= 10000) {
    await statusColl.replaceOne(
      { vehicleId },
      { ...unified, lastUpdated: now },
      { upsert: true }
    );
  }
};
