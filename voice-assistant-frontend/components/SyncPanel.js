"use client";

import { useEffect, useRef, useState } from "react";

const STAGES = [
  { icon: "🎛️", label: "Simulator", sub: "VSS snapshots" },
  { icon: "📦", label: "ObjectBox · edge", sub: "vss-telemetry-service" },
  { icon: "🔄", label: "Sync Server", sub: "ObjectBox Sync" },
  { icon: "🍃", label: "MongoDB Atlas", sub: "objectbox_telemetry" },
  { icon: "⚙️", label: "Atlas trigger", sub: "telemetry-data / -status" },
];

export default function SyncPanel({ onPausedChange, vehicleId }) {
  // vehicleId is used only in session scope (Kanopy); global scope ignores it.
  const vidQS = vehicleId ? `?vehicleId=${encodeURIComponent(vehicleId)}` : "";
  const [state, setState] = useState(null);
  const [paused, setPaused] = useState(false);
  const [busy, setBusy] = useState(false);
  const [flow, setFlow] = useState(false);
  const prevEdge = useRef(null);
  // Track the last reported paused state so the parent (home page online/offline) is
  // notified only when it actually flips, whether the change came from this panel's
  // button or from the header toggle.
  const prevPaused = useRef(null);

  useEffect(() => {
    let cancelled = false;
    const tick = async () => {
      try {
        const d = await (await fetch(`/api/sync/state${vidQS}`)).json();
        if (cancelled) return;
        setState(d);
        const ec = d?.edge?.local_count;
        if (prevEdge.current != null && ec != null && ec !== prevEdge.current) {
          setFlow(true);
          setTimeout(() => setFlow(false), 800);
        }
        prevEdge.current = ec;
        const p = !!d?.edge?.paused;
        setPaused(p);
        if (prevPaused.current !== p) {
          prevPaused.current = p;
          onPausedChange?.(p);   // keep the home page online/offline in sync with real state
        }
      } catch {}
    };
    tick();
    const id = setInterval(tick, 2000);
    return () => { cancelled = true; clearInterval(id); };
  }, []);

  const toggle = async () => {
    setBusy(true);
    try {
      await fetch(paused ? "/api/sync/resume" : "/api/sync/pause", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ vehicle_id: vehicleId }),  // used in session scope; ignored globally
      });
    } catch {}
    setBusy(false);
  };

  const edge = state?.edge || {};
  const cloud = state?.cloud || {};
  const edgeCount = edge.local_count;
  const cloudCount = cloud.objectbox_telemetry;
  const available = edge.available !== false; // false only when the service reports no sync client
  const connected = !!edge.connected;
  // Authoritative backlog: the ObjectBox client's outgoing-queue depth (writes not yet
  // acked by the server). Rises while paused, drains on resume — retention-independent,
  // unlike an edge−cloud count delta.
  const buffered = edge.buffered != null ? edge.buffered : null;

  return (
    <div className="sync-panel">
      <div className="sync-head">
        <span className={`sync-badge${connected ? " on" : " off"}`}>
          {!available ? "● NO SYNC" : connected ? "● SYNC LIVE" : paused ? "● SYNC PAUSED" : "● OFFLINE"}
        </span>
        <button className="sync-toggle" onClick={toggle} disabled={busy || !available} title={!available ? "Sync not available in this deployment" : ""}>
          {paused ? "▶ Resume sync" : "⏸ Pause sync"}
        </button>
      </div>

      {/* Pipeline */}
      <div className="pipeline">
        {STAGES.map((s, i) => (
          <div className="pl-stage" key={i}>
            <div className="pl-node"><span>{s.icon}</span></div>
            <div className="pl-text">
              <div className="pl-label">{s.label}</div>
              <div className="pl-sub">{s.sub}</div>
            </div>
            {i < STAGES.length - 1 && (
              <div className={`pl-arrow${flow ? " flowing" : ""}${!connected && i >= 1 ? " broken" : ""}`}>▸</div>
            )}
          </div>
        ))}
      </div>

      {/* Edge vs cloud counters */}
      <div className="sync-counts">
        <div className="sc-card">
          <div className="sc-label">📦 Edge · ObjectBox</div>
          <div className="sc-num">{edgeCount ?? "—"}</div>
          <div className="sc-sub">objectbox_telemetry · local</div>
        </div>
        <div className={`sc-gap${buffered ? " buffering" : ""}`}>
          {buffered != null ? `${buffered} buffered` : "—"}
          <div className="sc-gap-arrow">
            {!available ? "⇢ no sync ⇢" : connected ? "→ syncing →" : paused ? "⇢ paused ⇢" : "⇢ offline ⇢"}
          </div>
        </div>
        <div className="sc-card">
          <div className="sc-label">🍃 Cloud · Atlas</div>
          <div className="sc-num">{cloudCount ?? "—"}</div>
          <div className="sc-sub">objectbox_telemetry · synced</div>
        </div>
      </div>

      <div className="sync-hint">
        Pause sync → the edge count keeps rising while the cloud holds (offline buffering).
        Resume → the backlog replicates and the counts converge.
      </div>
    </div>
  );
}
