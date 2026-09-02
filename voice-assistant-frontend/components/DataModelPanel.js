"use client";

import { useEffect, useState } from "react";
import MongoDocViewer from "./MongoDocViewer";

const ENTITIES = [
  { name: "manual_chunks", owner: "search-service", fields: "text · embedding[1024] · source_file", note: "Atlas Vector Search / ObjectBox HNSW" },
  { name: "conversations", owner: "conversation-service", fields: "conversation_id · role · message · sources · tools_used", note: "sources/tools_used → native BSON arrays" },
  { name: "objectbox_telemetry", owner: "vss-telemetry-service", fields: "vehicleId · ts · data · meta", note: "data/meta (JsonToNative) → nested docs in Atlas" },
];

export default function DataModelPanel({ vehicleId }) {
  const [doc, setDoc] = useState(null);
  // Scope the live document to THIS session's vehicle (not the default one).
  const vidQS = vehicleId ? `&vehicleId=${encodeURIComponent(vehicleId)}` : "";
  useEffect(() => {
    let cancelled = false;
    const tick = async () => {
      try {
        const d = await (await fetch(`/api/vss/latest?t=${Date.now()}${vidQS}`)).json();
        if (!cancelled && !d.error) setDoc(d);
      } catch {}
    };
    tick();
    const id = setInterval(tick, 3000);
    return () => { cancelled = true; clearInterval(id); };
  }, [vidQS]);

  return (
    <div className="dm-panel">
      <div className="dm-entities">
        {ENTITIES.map((e) => (
          <div className="dm-card" key={e.name}>
            <div className="dm-name">{e.name}</div>
            <div className="dm-owner">{e.owner}</div>
            <div className="dm-fields">{e.fields}</div>
            <div className="dm-arrow">↕ ObjectBox Sync ↕</div>
            <div className="dm-atlas">Atlas · {e.name}</div>
            <div className="dm-note">{e.note}</div>
          </div>
        ))}
      </div>
      <div className="dm-live">
        <div className="dm-live-head">Live <code>objectbox_telemetry</code> document (edge → Atlas, verbatim)</div>
        <MongoDocViewer doc={doc} maxHeight={220} empty="// start the simulation to see a live snapshot" />
      </div>
    </div>
  );
}
