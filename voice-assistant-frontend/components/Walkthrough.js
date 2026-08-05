"use client";

import { useEffect, useLayoutEffect, useState } from "react";

// Lightweight spotlight tour — highlights real cockpit elements by CSS selector,
// dims the rest, and shows a tooltip. No external dependency.
export const TOUR_STEPS = [
  { selector: ".sim-btn", title: "Start the simulation", text: "Generates live VSS telemetry into the on-edge ObjectBox store." },
  { selector: ".gauge-wrap", title: "Live telemetry", text: "RPM & speed are read from the edge database — works fully offline." },
  { selector: ".telltales", title: "Warning lights", text: "Tell-tales light from live sensor values and active OBD-II fault codes." },
  { selector: ".chat-input", title: "Ask the assistant", text: "Try “What does the check-engine light mean?” or “What’s my fuel level?”." },
  { selector: ".mic-button", title: "Talk to it", text: "Tap to speak — Whisper transcribes server-side, Piper speaks the reply." },
  { selector: ".conn-toggle", title: "Online / offline", text: "Switch the manual search between edge ObjectBox and Atlas Vector Search." },
  { selector: ".status-right", title: "Car-manual embeddings", text: "The 📚 count is the number of car-manual chunks embedded (voyage-4-nano, 1024-d) and indexed for vector search — in ObjectBox HNSW on the edge, and Atlas Vector Search in the cloud." },
  { selector: ".dtc-ticker", title: "Active fault codes", text: "Live DTCs decoded from the telemetry stream." },
  { selector: ".hdr-sync", title: "See the sync", text: "Open Sync & Data to watch ObjectBox ⇄ Atlas replication live — and pause it." },
];

export default function Walkthrough({ run, steps = TOUR_STEPS, onClose }) {
  const [i, setI] = useState(0);
  const [rect, setRect] = useState(null);

  useLayoutEffect(() => {
    if (!run) return;
    const el = document.querySelector(steps[i]?.selector);
    if (el) {
      el.scrollIntoView({ block: "center", behavior: "smooth" });
      setRect(el.getBoundingClientRect());
    } else {
      setRect(null);
    }
  }, [run, i, steps]);

  useEffect(() => {
    if (!run) return;
    const onResize = () => {
      const el = document.querySelector(steps[i]?.selector);
      setRect(el ? el.getBoundingClientRect() : null);
    };
    window.addEventListener("resize", onResize);
    return () => window.removeEventListener("resize", onResize);
  }, [run, i, steps]);

  if (!run) return null;
  const step = steps[i];
  const pad = 8;
  const ring = rect
    ? { top: rect.top - pad, left: rect.left - pad, width: rect.width + pad * 2, height: rect.height + pad * 2 }
    : null;

  // Tooltip: prefer below the ring, else above; always clamped inside the viewport
  // so it never runs off-screen (e.g. beside the tall gauges).
  const TIP_W = 320, TIP_H = 190;
  let tipTop = null, tipLeft = null;
  if (ring) {
    const below = ring.top + ring.height + 14;
    const above = ring.top - TIP_H - 14;
    tipTop = below + TIP_H + 12 <= window.innerHeight ? below : above >= 12 ? above : below;
    tipTop = Math.max(12, Math.min(tipTop, window.innerHeight - TIP_H - 12));
    tipLeft = Math.max(12, Math.min(ring.left, window.innerWidth - TIP_W - 12));
  }

  const done = () => { setI(0); onClose(); };

  return (
    <div className="tour">
      {ring ? (
        <div className="tour-ring" style={{ top: ring.top, left: ring.left, width: ring.width, height: ring.height }} />
      ) : (
        <div className="tour-dim" />
      )}
      <div
        className="tour-tip"
        style={ring ? { top: tipTop, left: tipLeft } : { top: "50%", left: "50%", transform: "translate(-50%,-50%)" }}
      >
        <div className="tour-step-n">{i + 1} / {steps.length}</div>
        <div className="tour-title">{step.title}</div>
        <div className="tour-text">{step.text}</div>
        <div className="tour-actions">
          <button className="tour-skip" onClick={done}>Skip</button>
          <div className="tour-nav">
            {i > 0 && <button onClick={() => setI(i - 1)}>Back</button>}
            {i < steps.length - 1
              ? <button className="tour-next" onClick={() => setI(i + 1)}>Next</button>
              : <button className="tour-next" onClick={done}>Done</button>}
          </div>
        </div>
      </div>
    </div>
  );
}
