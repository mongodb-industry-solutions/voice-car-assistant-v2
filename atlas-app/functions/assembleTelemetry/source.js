/**
 * assembleTelemetry — Atlas trigger on `objectbox_telemetry` inserts.
 *
 * Reshapes each synced `objectbox_telemetry` snapshot into the app-facing collections:
 *   - telemetry-data   : INSERT on every fire (~2 s) — time-series history (timeField `ts`).
 *   - telemetry-status : ONE document keyed by vehicleId, always current, written at
 *                        most once every 10 s (unique vehicleId index → single doc).
 *
 * `data`/`meta` are JsonToNative → nested objects (may also arrive as raw strings).
 * `data` is the full VSS `Vehicle` tree, passed through verbatim; we only add
 * CurrentLocation.locationGeoJson for geo queries.
 *
 * DIAGNOSTICS: every step logs with the [assembleTelemetry] prefix; errors are re-thrown,
 * so App Services → Logs shows exactly what happened. It also runs when invoked MANUALLY
 * (the "Run" button, no changeEvent): it processes the newest objectbox_telemetry doc and
 * returns a summary. Set DIAG=false to quiet the verbose lines once it's working.
 */
const DIAG = true;
const log = (...a) => { if (DIAG) console.log("[assembleTelemetry]", ...a); };

exports = async function (changeEvent) {
  try {
    // Resolve DB + linked data source first (needed for both trigger + manual test).
    const dbName = context.values.get("DATABASE_NAME") || (changeEvent && changeEvent.ns && changeEvent.ns.db);
    const service = context.services.get("mongodb-atlas");
    log("DATABASE_NAME:", dbName || "(NOT SET)",
        "| data source 'mongodb-atlas':", service ? "resolved" : "(NOT FOUND — rename linked cluster)");
    if (!dbName) { console.error("[assembleTelemetry] DATABASE_NAME not set — aborting"); return { error: "DATABASE_NAME not set" }; }
    if (!service) { console.error("[assembleTelemetry] data source 'mongodb-atlas' not found — aborting"); return { error: "data source not found" }; }
    const db = service.db(dbName);

    // Get the changed document. When invoked manually (Run button → no changeEvent),
    // fall back to the newest objectbox_telemetry doc so this runs end-to-end.
    let manual = false;
    let fullDoc = changeEvent && changeEvent.fullDocument;
    if (fullDoc) {
      log("=== trigger fired === op:", changeEvent.operationType,
          "ns:", changeEvent.ns && `${changeEvent.ns.db}.${changeEvent.ns.coll}`);
    } else {
      manual = true;
      log("=== MANUAL RUN === no changeEvent — reading newest objectbox_telemetry doc");
      fullDoc = await db.collection("objectbox_telemetry").findOne({}, { sort: { ts: -1 } });
      if (!fullDoc) {
        console.error("[assembleTelemetry] objectbox_telemetry is EMPTY in this DB — wrong DATABASE_NAME?");
        return { error: "objectbox_telemetry empty in " + dbName };
      }
    }

    log("doc keys:", Object.keys(fullDoc).join(", "));
    log("field types → vehicleId:", typeof fullDoc.vehicleId,
        "| data:", typeof fullDoc.data, "| meta:", typeof fullDoc.meta, "| ts:", typeof fullDoc.ts);

    // Parse JsonToNative fields (tolerate raw strings).
    let data = fullDoc.data;
    if (typeof data === "string") { try { data = JSON.parse(data); } catch (_) { data = {}; } }
    data = data || {};

    let meta = fullDoc.meta;
    if (typeof meta === "string") { try { meta = JSON.parse(meta); } catch (_) { meta = null; } }

    const vehicleId = fullDoc.vehicleId || data.vehicle_id || data.vehicleId || "VSS-DEMO-VIN-001";
    log("resolved vehicleId:", vehicleId, "| data top-level keys:", Object.keys(data).join(", ") || "(none)");

    // Enrich VSS CurrentLocation with a GeoJSON Point (lng-first, RFC 7946).
    const loc = data.CurrentLocation;
    if (loc && loc.Latitude != null && loc.Longitude != null) {
      loc.locationGeoJson = { type: "Point", coordinates: [loc.Longitude, loc.Latitude] };
    }

    const now = new Date();
    // `ts` is the time-series timeField and MUST be a BSON Date. The source doc's `ts`
    // is epoch-ms (Long) — convert it to a Date (fall back to now). `timestamp` is an alias.
    const eventTs = fullDoc.ts != null ? new Date(Number(fullDoc.ts)) : now;
    log("eventTs:", eventTs.toISOString(), "valid:", !isNaN(eventTs.getTime()));
    const unified = { vehicleId, ts: eventTs, timestamp: eventTs, meta: meta || null, data };

    // 1) Time-series history — append on every fire. Isolated so a failure here does
    //    NOT block the status write, and its error is logged explicitly.
    let dataOk = false, dataErr = null, insertedId = null;
    try {
      const ins = await db.collection("telemetry-data").insertOne(unified);
      insertedId = ins && ins.insertedId ? String(ins.insertedId) : null;
      log("telemetry-data insertOne returned insertedId:", insertedId);
      // Read back to confirm it actually persisted (catches TTL / bucketing surprises).
      const back = await db.collection("telemetry-data").findOne({ vehicleId }, { sort: { ts: -1 } });
      dataOk = !!back;
      log("telemetry-data read-back:", dataOk ? "FOUND ✓" : "NOT FOUND after insert ⚠ (TTL/expireAfterSeconds? recreate the collection)");
    } catch (e) {
      dataErr = e && e.message;
      console.error("[assembleTelemetry] telemetry-data INSERT FAILED:", dataErr);
    }

    // 2) Current status — single doc keyed by vehicleId, written at most every 10 s.
    //    Isolated too, so it always runs regardless of the time-series result.
    let statusWritten = false;
    try {
      const statusColl = db.collection("telemetry-status");
      const current = await statusColl.findOne({ vehicleId }, { projection: { lastUpdated: 1 } });
      const elapsed = current && current.lastUpdated
        ? now.getTime() - new Date(current.lastUpdated).getTime()
        : Infinity;
      if (elapsed >= 10000) {
        const res = await statusColl.replaceOne(
          { vehicleId },
          { ...unified, lastUpdated: now },
          { upsert: true }
        );
        statusWritten = true;
        log("telemetry-status upserted (matched:", res.matchedCount, "modified:", res.modifiedCount,
            "upsertedId:", res.upsertedId ? String(res.upsertedId) : "none", ")");
      } else {
        log(`telemetry-status skipped — last update ${Math.round(elapsed)}ms ago (<10000)`);
      }
    } catch (e) {
      console.error("[assembleTelemetry] telemetry-status write FAILED:", e && e.message);
    }

    const counts = {
      "telemetry-data": await db.collection("telemetry-data").count(),
      "telemetry-status": await db.collection("telemetry-status").count(),
    };
    log("counts after → telemetry-data:", counts["telemetry-data"], "| telemetry-status:", counts["telemetry-status"]);
    log("=== done ===");

    return { ok: true, mode: manual ? "manual" : "trigger", vehicleId,
             telemetryData: { insertedId, readBackFound: dataOk, error: dataErr },
             statusWritten, counts };
  } catch (e) {
    console.error("[assembleTelemetry] ERROR:", e && e.message, "\n", e && e.stack);
    throw e; // surface in the trigger's error logs
  }
};
