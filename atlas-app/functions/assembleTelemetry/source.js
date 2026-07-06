exports = async function (changeEvent) {
  const fullDoc = changeEvent.fullDocument;
  if (!fullDoc || !fullDoc.vehicleId) return;

  const vehicleId = fullDoc.vehicleId;
  const dbName = context.values.get("DATABASE_NAME");
  const atlas = context.services.get("mongodb-atlas");
  const db = atlas.db(dbName);

  // Read latest sample from each companion collection in parallel.
  // Use aggregate ($sort + $limit) — universally supported in Atlas App Services.
  // No vehicleId filter for single-vehicle demo simplicity.
  const latest = (coll) => db.collection(coll).find({}).sort({ ts: -1 }).limit(1).next();

  const [battery, chassis, cabin, location, adas, meta] = await Promise.all([
    latest("BatterySample"),
    latest("ChassisSample"),
    latest("CabinSample"),
    latest("LocationSample"),
    latest("AdasSample"),
    db.collection("VehicleMeta").findOne({ vehicleId }),
  ]);

  console.log("assembleTelemetry debug", JSON.stringify({
    vehicleId,
    battery: battery ? "found" : "null",
    chassis: chassis ? "found" : "null",
    cabin: cabin ? "found" : "null",
    location: location ? "found" : "null",
    adas: adas ? "found" : "null",
    meta: meta ? "found" : "null",
  }));

  // Strip ObjectBox internal fields and sample bookkeeping fields
  function clean(doc) {
    if (!doc) return null;
    const out = {};
    for (const k of Object.keys(doc)) {
      if (k !== "_id" && k !== "id" && k !== "syncClock" && k !== "vehicleId" && k !== "ts") {
        out[k] = doc[k];
      }
    }
    return out;
  }

  // Powertrain data comes directly from the trigger document (PowertrainSample)
  const powertrainData = clean(fullDoc);

  // Parse locationGeoJson from stored JSON string to a proper GeoJSON object
  const locationData = clean(location);
  if (locationData && typeof locationData.locationGeoJson === "string") {
    try { locationData.locationGeoJson = JSON.parse(locationData.locationGeoJson); } catch (_) {}
  }

  const now = new Date();

  const unified = {
    vehicleId,
    timestamp: now,
    meta: clean(meta),
    data: {
      powertrain: powertrainData,
      battery: clean(battery),
      chassis: clean(chassis),
      cabin: clean(cabin),
      location: locationData,
      adas: clean(adas),
    },
  };

  // Insert a new record into the time-series collection on every trigger fire (~every 2s)
  await db.collection("telemetry-data").insertOne(unified);

  // Upsert telemetry-status at most once every 10 seconds
  const statusColl = db.collection("telemetry-status");
  const current = await statusColl.findOne(
    { vehicleId },
    { projection: { lastUpdated: 1 } }
  );

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
