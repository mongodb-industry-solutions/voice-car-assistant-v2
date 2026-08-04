"use client";

import { useState } from "react";
import { TALK_TRACK } from "@/lib/const/talkTrack";

// Tabbed "Tell me more" modal — same structure as the MongoDB IS InfoWizard,
// restyled for the cockpit (no LeafyGreen dependency).
function Section({ section }) {
  return (
    <div className="iw-section">
      {section.heading && <h3 className="iw-h3">{section.heading}</h3>}
      {Array.isArray(section.body) ? (
        <ul className="iw-list">
          {section.body.map((item, i) =>
            typeof item === "object" ? (
              <li key={i}>
                {item.heading}
                <ul className="iw-list">
                  {item.body?.map((sub, j) => <li key={j}>{sub}</li>)}
                </ul>
              </li>
            ) : (
              <li key={i}>{item}</li>
            )
          )}
        </ul>
      ) : (
        <p className="iw-body">{section.body}</p>
      )}
    </div>
  );
}

export default function InfoWizard({ open, onClose, sections = TALK_TRACK }) {
  const [selected, setSelected] = useState(0);
  if (!open) return null;
  return (
    <div className="overlay" onClick={onClose}>
      <div className="iw-modal" onClick={(e) => e.stopPropagation()}>
        <div className="iw-head">
          <span className="iw-title">🍃 How it works</span>
          <button className="overlay-close" onClick={onClose}>✕</button>
        </div>
        <div className="iw-tabs">
          {sections.map((t, i) => (
            <button
              key={i}
              className={`iw-tab${i === selected ? " active" : ""}`}
              onClick={() => setSelected(i)}
            >
              {t.heading}
            </button>
          ))}
        </div>
        <div className="iw-content">
          {sections[selected].content.map((s, i) => <Section key={i} section={s} />)}
        </div>
      </div>
    </div>
  );
}
