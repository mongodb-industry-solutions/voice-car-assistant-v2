/**
 * Creates the two unified telemetry collections in MongoDB Atlas.
 *
 * Run from the atlas-app directory:
 *   npm install
 *   node setup_collections.js
 *
 * Reads credentials from atlas-app/.env.
 */

import { readFileSync } from "fs";
import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import { MongoClient } from "mongodb";

// ── Load .env ──────────────────────────────────────────────────────────────────

const __dirname = dirname(fileURLToPath(import.meta.url));

function loadEnv(filepath) {
  const raw = readFileSync(filepath, "utf8");
  const env = {};
  for (const line of raw.split("\n")) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) continue;
    const eq = trimmed.indexOf("=");
    if (eq === -1) continue;
    env[trimmed.slice(0, eq).trim()] = trimmed.slice(eq + 1).trim();
  }
  return env;
}

const env = loadEnv(resolve(__dirname, ".env"));

const MONGODB_USER    = env.MONGODB_USER    || process.env.MONGODB_USER;
const MONGODB_PASS    = env.MONGODB_PASS    || process.env.MONGODB_PASS;
const MONGODB_CLUSTER = env.MONGODB_CLUSTER || process.env.MONGODB_CLUSTER;
const DB_NAME         = env.MONGODB_DATABASE || process.env.MONGODB_DATABASE;

if (!MONGODB_USER || !MONGODB_PASS || !MONGODB_CLUSTER || !DB_NAME) {
  console.error("Missing required variables in atlas-app/.env:");
  console.error("  MONGODB_USER, MONGODB_PASS, MONGODB_CLUSTER, MONGODB_DATABASE");
  process.exit(1);
}

const MONGODB_URI = `mongodb+srv://${MONGODB_USER}:${MONGODB_PASS}@${MONGODB_CLUSTER}/?retryWrites=true&w=majority&tls=true`;

// ── Create collections ─────────────────────────────────────────────────────────

const client = new MongoClient(MONGODB_URI, { serverSelectionTimeoutMS: 10000 });

try {
  await client.connect();
  console.log(`Connected to Atlas — database: ${DB_NAME}`);
  const db = client.db(DB_NAME);

  // telemetry-data: native time-series (timeField/metaField cannot change after creation)
  try {
    await db.createCollection("telemetry-data", {
      timeseries: {
        timeField: "timestamp",
        metaField: "vehicleId",
        granularity: "seconds",
      },
    });
    console.log("Created time-series collection: telemetry-data");
  } catch (e) {
    if (e.codeName === "NamespaceExists") {
      console.log("telemetry-data already exists — skipping");
    } else {
      throw e;
    }
  }

  // telemetry-status: standard collection, one document per vehicle
  try {
    await db.createCollection("telemetry-status");
    console.log("Created collection: telemetry-status");
  } catch (e) {
    if (e.codeName === "NamespaceExists") {
      console.log("telemetry-status already exists — skipping");
    } else {
      throw e;
    }
  }

  await db.collection("telemetry-status").createIndex(
    { vehicleId: 1 },
    { unique: true, name: "vehicleId_unique" }
  );
  console.log("Unique index on telemetry-status.vehicleId ensured");

  console.log("\nSetup complete.");
} finally {
  await client.close();
}
