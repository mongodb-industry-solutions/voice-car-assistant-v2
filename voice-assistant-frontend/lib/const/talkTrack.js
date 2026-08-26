// Talk track for the InfoWizard modal (same shape as the MongoDB IS reference demos):
// [{ heading, content: [{ heading, body: string | (string | {heading, body:[]})[] }] }]
export const TALK_TRACK = [
  {
    heading: "Overview",
    content: [
      {
        heading: "Mobile Edge In-Car Voice Assistant",
        body: "An offline-first in-vehicle assistant. An on-edge ObjectBox database holds the car-manual, conversations and live vehicle telemetry, so the car keeps working with no connectivity. ObjectBox Sync replicates every change to MongoDB Atlas in the cloud, and pulls cloud data back down — the same objects, edge and cloud.",
      },
      {
        heading: "What to look for",
        body: [
          "The RPM/speed gauges and warning lights are driven by live VSS telemetry stored on the edge in ObjectBox.",
          "Ask about a warning light or 'how do I…' — answered by vector search over the car manual (ObjectBox HNSW offline, Atlas Vector Search online).",
          "The online/offline toggle switches the assistant's search source; the Sync panel shows data replicating edge → cloud in real time.",
          "Pause replication in the Sync panel to watch the edge keep recording while the cloud freezes — then resume and see it catch up.",
        ],
      },
    ],
  },
  {
    heading: "How to Demo",
    content: [
      {
        heading: "Steps",
        body: [
          "Start the simulation (top-left) — telemetry begins flowing into the edge ObjectBox store.",
          "Ask 'What is the current status of my car?' — the agent calls the live-telemetry tools.",
          "Ask 'What does the check-engine light mean?' — the agent runs a vector search over the car manual and cites sources.",
          "Open the Sync panel: watch the edge and cloud document counts converge as ObjectBox Sync replicates to Atlas.",
          "Hit 'Pause sync' — the edge count keeps rising, the cloud count holds. Resume — the backlog syncs up.",
        ],
      },
    ],
  },
  {
    heading: "Behind the Scenes",
    content: [
      {
        heading: "Architecture",
        body: "The simulator emits VSS vehicle snapshots to a C++ service that writes them to a local ObjectBox store. ObjectBox Sync streams those rows to the ObjectBox Sync Server, which replicates them into MongoDB Atlas. An Atlas trigger reshapes each snapshot into time-series and current-status collections that the telemetry tools read. The LangChain agent answers using car-manual vector search, live telemetry and navigation.",
      },
      {
        heading: "Data flow",
        body: [
          "Simulator → vss-telemetry-service (ObjectBox, edge)",
          "ObjectBox local DB → ObjectBox Sync Server → MongoDB Atlas (objectbox_telemetry)",
          "Atlas trigger → telemetry-data (time-series) + telemetry-status (current)",
          "Agent → car-manual vector search + telemetry tools + navigation",
        ],
      },
    ],
  },
  {
    heading: "Why MongoDB + ObjectBox",
    content: [
      {
        heading: "Offline-first at the edge",
        body: "ObjectBox is a fast on-device database; the car runs fully offline and reconciles automatically when connectivity returns.",
      },
      {
        heading: "One object model, edge to cloud",
        body: "ObjectBox Sync replicates the same objects to MongoDB Atlas — no bespoke ETL. Nested JSON (the full VSS Vehicle tree) lands as a native document in Atlas.",
      },
      {
        heading: "Atlas Vector Search",
        body: "The car manual is embedded once (voyage-4-nano, 1024-d) and searched with Atlas Vector Search online, or ObjectBox HNSW on the edge offline — the same embeddings on both sides.",
      },
      {
        heading: "Cloud storage for history",
        body: "On-edge storage is limited, so the ObjectBox store keeps only a rolling window (pruned after a few hours) while MongoDB Atlas is the durable home for the full telemetry history — powering the time-series collection, analytics and long-term queries the edge can't hold.",
      },
    ],
  },
];
