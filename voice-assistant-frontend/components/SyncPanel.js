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
  // Edge count captured at the moment we go offline; buffered backlog = growth since then.
  const baselineRef = useRef(null);

  useEffect(() => {
    let cancelled = false;
    const tick = async () => {
      try {
        const d = await (await fetch(`/api/sync/state${vidQS}`)).json();
        if (cancelled) return;
        setState(d);
        const ec = d?.edge?.local_count;
        const p = !!d?.edge?.paused;
        const wasPaused = prevPaused.current;   // last tick's paused (null on first)
        // On the online→offline transition, snapshot the edge count (use the previous, still-
        // online count so the transition tick's write isn't missed). Clear it when back online.
        if (p && wasPaused !== true) {
          baselineRef.current = prevEdge.current != null ? prevEdge.current : (ec ?? 0);
        } else if (!p) {
          baselineRef.current = null;
        }
        if (prevEdge.current != null && ec != null && ec !== prevEdge.current) {
          setFlow(true);
          setTimeout(() => setFlow(false), 800);
        }
        prevEdge.current = ec;
        setPaused(p);
        if (wasPaused !== p) {
          prevPaused.current = p;
          onPausedChange?.(p);   // keep the home page online/offline in sync with real state
        }
      } catch {}
    };
    tick();
    const id = setInterval(tick, 2000);
    return () => { cancelled = true; clearInterval(id); };
    // Restart the interval if the session vehicle id becomes available/changes, so polling
    // never sticks with a stale/empty ?vehicleId= (session scope needs it on every request).
  }, [vidQS, onPausedChange]);

  const toggle = async () => {
    // Skip until the session vehicle id exists — an empty id would fall back to the default
    // vehicle in session scope (pausing/resuming the wrong one).
    if (!vehicleId) return;
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
  // Backlog while offline = growth in the edge count since we paused. local_count keeps rising
  // in both scopes while paused (global: writes keep hitting the store; session: local_count =
  // synced + buffer), so this is the real "not yet in Atlas" count. It works even when the
  // ObjectBox outgoing-queue metric reads 0 — a cut connection (global scope) forms no outgoing
  // messages. Falls back to the service-reported buffered if the panel opened mid-pause.
  const buffered = !paused
    ? 0
    : (baselineRef.current != null && edgeCount != null)
      ? Math.max(0, edgeCount - baselineRef.current)
      : (edge.buffered ?? 0);

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
          {paused ? `${buffered} buffered` : "in sync"}
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
