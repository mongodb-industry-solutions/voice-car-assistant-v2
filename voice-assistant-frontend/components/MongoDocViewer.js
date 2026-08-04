"use client";

// Minimal pretty-printed JSON viewer (no external deps).
export default function MongoDocViewer({ doc, maxHeight = 260, empty = "// no document yet" }) {
  const text = doc && Object.keys(doc).length ? JSON.stringify(doc, null, 2) : empty;
  return <pre className="doc-viewer" style={{ maxHeight }}>{text}</pre>;
}
