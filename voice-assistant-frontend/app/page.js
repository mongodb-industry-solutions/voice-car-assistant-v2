"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import InfoWizard from "@/components/InfoWizard";
import SyncPanel from "@/components/SyncPanel";
import DataModelPanel from "@/components/DataModelPanel";
import Walkthrough from "@/components/Walkthrough";

// ── Gauge geometry (270° sweep, gap at bottom) ────────────────────────────────
const G = { cx: 130, cy: 130, r: 100, rTick: 128, rTickInner: 85, a0: 225, span: 270 };
const NEEDLE_LEN = 82;
const RPM_MAX = 8000, SPD_MAX = 240;

function gpolar(deg, r) {
  const a = ((deg - 90) * Math.PI) / 180;
  return [G.cx + r * Math.cos(a), G.cy + r * Math.sin(a)];
}
function gArc(frac0, frac1, r) {
  const s = G.a0 + G.span * frac0, e = G.a0 + G.span * frac1;
  const [x0, y0] = gpolar(s, r), [x1, y1] = gpolar(e, r);
  const large = e - s <= 180 ? 0 : 1;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

function Gauge({ value, max, labelMax, labels, minorInterval, redline, unit, display, valueLg, gid = "g" }) {
  // labelMax scales the printed ticks (e.g. RPM shows 0–8); max scales the needle
  // value (e.g. RPM 0–8000). They differ when the dial labels aren't the raw value.
  const lMax = labelMax ?? max;

  // Power-on sweep: on mount the needle sweeps up to full, then settles onto the
  // live value (the CSS transition on the arc/needle animates the motion).
  const [boot, setBoot] = useState(0);
  useEffect(() => {
    const up = setTimeout(() => setBoot(max), 80);
    const settle = setTimeout(() => setBoot(null), 900);
    return () => { clearTimeout(up); clearTimeout(settle); };
  }, [max]);

  const shown = boot != null ? boot : value || 0;
  const frac = Math.max(0, Math.min(1, shown / max));
  const [nx, ny] = gpolar(G.a0 + G.span * frac, NEEDLE_LEN);
  const majorSet = new Set(labels.map((v) => v / lMax));

  const minors = [];
  if (minorInterval) {
    for (let v = 0; v <= lMax; v += minorInterval) {
      const f = v / lMax;
      if (majorSet.has(f)) continue;
      const ang = G.a0 + G.span * f;
      const [ix, iy] = gpolar(ang, G.r - 8), [ox, oy] = gpolar(ang, G.r);
      minors.push(<line key={`mi${v}`} x1={ix.toFixed(1)} y1={iy.toFixed(1)} x2={ox.toFixed(1)} y2={oy.toFixed(1)} className="g-tick-minor" />);
    }
  }
  const majors = labels.map((v) => {
    const ang = G.a0 + G.span * (v / lMax);
    const [tx, ty] = gpolar(ang, G.rTick), [ix, iy] = gpolar(ang, G.rTickInner), [ox, oy] = gpolar(ang, G.r);
    return (
      <g key={`ma${v}`}>
        <line x1={ix.toFixed(1)} y1={iy.toFixed(1)} x2={ox.toFixed(1)} y2={oy.toFixed(1)} />
        <text x={tx.toFixed(1)} y={ty.toFixed(1)}>{v}</text>
      </g>
    );
  });

  return (
    <section className="gauge-wrap">
      <svg className="gauge" viewBox="0 0 260 260" aria-hidden="true">
        <defs>
          <linearGradient id={`${gid}-grad`} x1="0" y1="0" x2="1" y2="1">
            <stop offset="0%" stopColor="#00684A" />
            <stop offset="55%" stopColor="#00b34c" />
            <stop offset="100%" stopColor="#00ED64" />
          </linearGradient>
          <filter id={`${gid}-glow`} x="-30%" y="-30%" width="160%" height="160%">
            <feGaussianBlur stdDeviation="2.4" result="b" />
            <feMerge><feMergeNode in="b" /><feMergeNode in="SourceGraphic" /></feMerge>
          </filter>
        </defs>
        <path className="g-track" d={gArc(0, 1, G.r)} />
        <path className="g-fill" style={{ stroke: `url(#${gid}-grad)`, filter: `url(#${gid}-glow)` }} d={frac > 0.001 ? gArc(0, frac, G.r) : ""} />
        {redline && <path className="g-redline" d={gArc(0.85, 1, G.r)} />}
        <g className="g-ticks">{minors}{majors}</g>
        <line className="g-needle" x1="130" y1="130" x2={nx.toFixed(1)} y2={ny.toFixed(1)} filter={`url(#${gid}-glow)`} />
        <circle className="g-pivot" cx="130" cy="130" r="9" />
        <circle className="g-pivot-dot" cx="130" cy="130" r="3.4" />
      </svg>
      <div className="gauge-center gauge-center--bottom">
        <div className={valueLg ? "g-value g-value-lg" : "g-value"}>{display}</div>
        <div className="g-unit">{unit}</div>
      </div>
    </section>
  );
}

// ── Tell-tale icons ────────────────────────────────────────────────────────────
const TT_ICONS = {
  "tt-mil": (<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round"><path d="M8 9V7h3v2" /><rect x="3" y="9" width="13" height="9" rx="1" /><path d="M3 12H1v3h2" /><path d="M1.2 12v-1.4h1.6v1.4" /><path d="M16 11.5h2a1 1 0 0 1 1 1v3a1 1 0 0 1-1 1h-2" /></svg>),
  "tt-abs": (<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"><circle cx="12" cy="12" r="7" /><path d="M3.5 6.5 A10 10 0 0 0 3.5 17.5" /><path d="M20.5 6.5 A10 10 0 0 1 20.5 17.5" /><text x="12" y="15.2" textAnchor="middle" fontSize="5.5" fontWeight="900" fill="currentColor" stroke="none" fontFamily="Arial,sans-serif">ABS</text></svg>),
  "tt-tpms": (<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.3" strokeLinecap="round" strokeLinejoin="round"><path d="M4.5 22 L4.5 13 Q4.5 4 12 4 Q19.5 4 19.5 13 L19.5 22" /><line x1="2" y1="22" x2="22" y2="22" /><rect x="10.5" y="5.5" width="3" height="9" rx="1.5" fill="currentColor" stroke="none" /><circle cx="12" cy="18.2" r="1.8" fill="currentColor" stroke="none" /></svg>),
  "tt-belt": (<svg viewBox="0 0 24 24" stroke="none"><circle cx="12" cy="4" r="3" fill="currentColor" /><path d="M7.5 8 C6 10 5.5 14 6 21h12 C18.5 14 18 10 16.5 8 Q14.5 7 12 7 Q9.5 7 7.5 8z" fill="currentColor" /><line x1="16" y1="9" x2="7.5" y2="20" stroke="var(--bg-0,#000)" strokeWidth="3" strokeLinecap="round" /></svg>),
  "tt-temp": (<svg viewBox="0 0 24 24" stroke="none"><rect x="9.5" y="1" width="5" height="13" rx="2.5" fill="currentColor" /><circle cx="12" cy="15.5" r="4.5" fill="currentColor" /><path d="M2 21 Q4 19.5 6 21 Q8 22.5 10 21 Q12 19.5 14 21 Q16 22.5 18 21 Q20 19.5 22 21" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" fill="none" /></svg>),
  "tt-batt": (<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round"><rect x="1" y="8" width="22" height="13" rx="2" /><path d="M13.5 11 L10.5 15h3.5 L11 21" /><line x1="3" y1="15" x2="6" y2="15" /><line x1="19" y1="13" x2="22" y2="13" /><line x1="20.5" y1="11.5" x2="20.5" y2="14.5" /></svg>),
  "tt-oil": (<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round"><path d="M4 20h13v-9H5z" /><path d="M5 11 L2 7" /><line x1="0.5" y1="6.5" x2="3.5" y2="6.5" /><path d="M19.5 16 Q22 17 22 19 Q22 21.5 19.5 21.5 Q17 21.5 17 19 Q17 16.5 19.5 16z" fill="currentColor" stroke="none" /></svg>),
  "tt-fuel": (<svg viewBox="0 0 24 24" stroke="none"><path d="M2 22 V4 Q2 2 4 2 H14 Q16 2 16 4 V22z" fill="currentColor" /><rect x="4" y="4" width="10" height="7" rx="1" fill="var(--bg-0,#000)" /><path d="M16 9 Q21 9 21 13 Q21 17 19 17" stroke="currentColor" fill="none" strokeWidth="2.8" strokeLinecap="round" /><rect x="17" y="15" width="4" height="5" rx="2" fill="currentColor" /></svg>),
};
const TT_ORDER = [["tt-mil", "ENG"], ["tt-abs", "ABS"], ["tt-tpms", "TPMS"], ["tt-belt", "BELT"], ["tt-temp", "TEMP"], ["tt-batt", "BATT"], ["tt-oil", "OIL"], ["tt-fuel", "FUEL"]];

const TOOL_LABELS = {
  search_car_manual_objectbox: { icon: "📦", label: "ObjectBox Search" },
  search_car_manual_atlas: { icon: "🍃", label: "MongoDB Atlas Vector Search" },
  navigate_to: { icon: "🗺️", label: "Navigation" },
  get_vehicle_status: { icon: "📡", label: "Vehicle Status" },
  get_powertrain_status: { icon: "⚙️", label: "Powertrain" },
  get_fuel_status: { icon: "⛽", label: "Fuel" },
  get_battery_status: { icon: "🔋", label: "Battery" },
  get_chassis_status: { icon: "🔧", label: "Chassis" },
  get_diagnostics: { icon: "🔧", label: "Diagnostics" },
};

// ── Telemetry helpers ──────────────────────────────────────────────────────────
function vssPick(data, path) {
  return path.split(".").reduce((o, k) => (o == null ? undefined : o[k]), data);
}
function worse(a, b) { const r = { off: 0, amber: 1, red: 2 }; return r[b] > r[a] ? b : a; }
const DTC_TELLTALE = { P1217: "temp", P1001: "fuel", P1002: "batt", C1001: "tpms", B1001: "belt", P0520: "oil" };

function deriveCockpit(data) {
  if (!data || data.error) return null;
  const rpm = vssPick(data, "Powertrain.CombustionEngine.Speed");
  const spd = vssPick(data, "Speed");
  const ext = vssPick(data, "Exterior.AirTemperature");
  const range = vssPick(data, "Powertrain.TractionBattery.Range");
  const econ = vssPick(data, "Powertrain.FuelSystem.InstantConsumption");
  const dist = vssPick(data, "TraveledDistance");
  const svc = vssPick(data, "Service.DistanceToService");
  const gearNum = vssPick(data, "Powertrain.Transmission.CurrentGear");

  const dg = vssPick(data, "Diagnostics") || {};
  const codes = Array.isArray(dg.DTCList) ? dg.DTCList : [];

  const coolant = vssPick(data, "Powertrain.CombustionEngine.EngineCoolant.Temperature");
  let temp = coolant == null ? "off" : coolant > 110 ? "red" : coolant > 100 ? "amber" : "off";
  const fuelPct = vssPick(data, "Powertrain.FuelSystem.RelativeLevel");
  let fuel = fuelPct == null ? "off" : fuelPct < 10 ? "red" : fuelPct < 20 ? "amber" : "off";
  const soc = vssPick(data, "Powertrain.TractionBattery.StateOfCharge.Current");
  let batt = soc == null ? "off" : soc < 15 ? "red" : soc < 25 ? "amber" : "off";
  let tpms = "off";
  ["Chassis.Axle.Row1.Wheel.Left.Tire.Pressure", "Chassis.Axle.Row1.Wheel.Right.Tire.Pressure",
   "Chassis.Axle.Row2.Wheel.Left.Tire.Pressure", "Chassis.Axle.Row2.Wheel.Right.Tire.Pressure"].forEach((p) => {
    const v = vssPick(data, p);
    if (v == null) return;
    tpms = worse(tpms, v < 193 ? "red" : v < 207 || v > 241 ? "amber" : "off");
  });
  const belted = vssPick(data, "Cabin.Seat.Row1.DriverSide.IsBelted");
  let belt = belted == null ? "off" : belted ? "off" : "red";
  const oilPsi = vssPick(data, "Powertrain.CombustionEngine.OilPressure");
  let oil = oilPsi == null ? "off" : oilPsi < 20 ? "red" : oilPsi < 35 ? "amber" : "off";

  let mil = "off", abs = "off";
  codes.forEach((c) => {
    const k = String(c)[0];
    const specific = DTC_TELLTALE[c];
    if (specific === "temp") temp = worse(temp, "amber");
    else if (specific === "fuel") fuel = worse(fuel, "amber");
    else if (specific === "batt") batt = worse(batt, "amber");
    else if (specific === "tpms") tpms = worse(tpms, "amber");
    else if (specific === "belt") belt = worse(belt, "amber");
    else if (specific === "oil") oil = worse(oil, "red");
    if (k === "P" || k === "U") mil = "amber";
    else if (k === "C" && !specific) abs = "amber";
  });

  const levels = { "tt-mil": mil, "tt-abs": abs, "tt-tpms": tpms, "tt-belt": belt, "tt-temp": temp, "tt-batt": batt, "tt-oil": oil, "tt-fuel": fuel };
  const displayCodes = [...codes];
  const ensure = (active, code) => { if (active !== "off" && !displayCodes.includes(code)) displayCodes.push(code); };
  ensure(temp, "P1217"); ensure(fuel, "P1001"); ensure(batt, "P1002");
  ensure(tpms, "C1001"); ensure(belt, "B1001"); ensure(oil, "P0520");

  const gear = gearNum == null ? null : gearNum < 0 ? "R" : gearNum === 0 ? "N" : "D";
  return {
    rpm, spd, ext, range, econ, dist, svc, gear, levels, displayCodes,
  };
}

const now = () => new Date().toLocaleTimeString("en-US", { hour: "numeric", minute: "2-digit" });

export default function Cockpit() {
  const [messages, setMessages] = useState([
    { id: 0, role: "assistant", text: "Welcome aboard. Ask me about your vehicle's status, a warning light, or say \"take me to the nearest charging station\" — I'll handle it.", time: "Just now" },
  ]);
  const [streaming, setStreaming] = useState(null); // {text}
  const [speakingId, setSpeakingId] = useState(null); // id of the message currently read aloud
  const [statusText, setStatusText] = useState("");
  const [busy, setBusy] = useState(false);
  const [listening, setListening] = useState(false);
  const [online, setOnline] = useState(true);
  const [vehicleId, setVehicleId] = useState(""); // this session's vehicle, shown in the header
  const [simRunning, setSimRunning] = useState(false);
  const [chunkCount, setChunkCount] = useState("--");
  const [clock, setClock] = useState("--:--");
  const [cockpit, setCockpit] = useState(null);
  const [navRoute, setNavRoute] = useState(null); // {destination, route}
  const [input, setInput] = useState("");
  const [infoOpen, setInfoOpen] = useState(false);
  const [sceneOpen, setSceneOpen] = useState(false);
  const [runTour, setRunTour] = useState(false);

  const messagesRef = useRef(null);
  const msgIdRef = useRef(1); // monotonic message id; 0 is the welcome message
  const vehicleIdRef = useRef(null); // per-session vehicle (one car per browser tab)
  // Stable conversation identity for the session — captured from the backend's `meta`
  // event on the first turn and re-sent on every subsequent request so the agent's
  // per-conversation memory (thread_id = conversation_id) actually carries context.
  const conversationIdRef = useRef(null);
  const userIdRef = useRef(null);
  const coordsRef = useRef({ lat: null, lon: null });
  const dtcCatalogRef = useRef({});
  const onlineRef = useRef(true);
  const audioRef = useRef(null);
  const audioUrlRef = useRef(null); // object URL of the audio currently loaded, so it can be revoked on stop/replace
  const mediaRef = useRef(null);
  const mapRef = useRef(null);
  const leafletRef = useRef(null);
  const mapObjectsRef = useRef({});

  useEffect(() => { onlineRef.current = online; }, [online]);

  // Per-session vehicle id (one car per browser tab). Persisted in sessionStorage so a
  // reload keeps the same vehicle, while a new tab/session gets a fresh one. Runs before
  // the telemetry/simulator polls below so their first fetch already carries the id.
  useEffect(() => {
    let vid;
    try {
      vid = sessionStorage.getItem("vca_vehicle_id");
      if (!vid) {
        const rand = (crypto?.randomUUID?.() || Math.random().toString(16).slice(2)).replace(/-/g, "").slice(0, 8);
        vid = `VEH-${rand}`;
        sessionStorage.setItem("vca_vehicle_id", vid);
      }
    } catch {
      vid = `VEH-${Math.random().toString(16).slice(2, 10)}`;
    }
    vehicleIdRef.current = vid;
    setVehicleId(vid);
  }, []);

  // Online/offline toggle. Beyond switching the manual-search source, going OFFLINE
  // really pauses ObjectBox↔Atlas replication (edge keeps buffering); ONLINE resumes it,
  // so the backlog catches up. Optimistically flip the UI, then roll back if the pause/
  // resume request fails — otherwise `online` (which also drives chat network_mode) would
  // drift out of sync with the real backend state the SyncPanel polls.
  const toggleOnline = () => {
    const vid = vehicleIdRef.current;
    // Don't toggle until this session's vehicle id exists — an empty id makes the backend
    // fall back to the default vehicle in session scope (pausing/resuming the wrong one).
    if (!vid) return;
    const next = !online;
    setOnline(next);
    fetch(next ? "/api/sync/resume" : "/api/sync/pause", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ vehicle_id: vid }),  // used in session scope; ignored globally
    })
      .then((r) => { if (!r.ok) setOnline(!next); })   // roll back on 409/500
      .catch(() => setOnline(!next));                   // roll back on network error
  };

  // The Sync & Data panel's own Pause/Resume button changes sync directly; mirror that
  // back into the header online/offline state (paused → offline) so the two never disagree.
  const handleSyncPaused = useCallback((paused) => setOnline(!paused), []);

  // Clock
  useEffect(() => {
    const t = () => setClock(new Date().toLocaleTimeString("en-US", { hour: "2-digit", minute: "2-digit" }));
    t();
    const id = setInterval(t, 1000);
    return () => clearInterval(id);
  }, []);

  // First-run guided tour — gated on sessionStorage (per tab/session), so it shows again
  // in a new tab or a fresh browser session, but stays dismissed on reloads of this tab.
  useEffect(() => {
    try { if (!sessionStorage.getItem("vca_tour_done")) setRunTour(true); } catch {}
  }, []);

  const endTour = useCallback(() => {
    setRunTour(false);
    try { sessionStorage.setItem("vca_tour_done", "1"); } catch {}
  }, []);

  // DTC catalog + geolocation (once)
  useEffect(() => {
    fetch("/dtc_catalog.json").then((r) => (r.ok ? r.json() : {})).then((c) => { dtcCatalogRef.current = c || {}; }).catch(() => {});
    if (navigator.geolocation) {
      navigator.geolocation.watchPosition(
        (pos) => { coordsRef.current = { lat: pos.coords.latitude, lon: pos.coords.longitude }; },
        (err) => console.warn("Geolocation:", err.message),
        { enableHighAccuracy: true, maximumAge: 30000, timeout: 10000 }
      );
    }
  }, []);

  // Manual chunk count — poll until populated. Chunks stream into the local store via
  // sync (rehydrated from Atlas), so the first fetch at load often sees 0 before the edge
  // store has finished syncing; keep polling until a non-zero count settles.
  useEffect(() => {
    let cancelled = false;
    let timer = null;
    // Self-scheduling loop: the next poll is only queued after the current request
    // finishes (no overlap if /api/stats is slow), and we stop as soon as the count
    // is positive. `cancelled` guards against a late response after unmount.
    const poll = async () => {
      try {
        const d = await (await fetch("/api/stats")).json();
        if (cancelled) return;
        if (d.chunk_count !== undefined) {
          setChunkCount(Number(d.chunk_count).toLocaleString());
          if (Number(d.chunk_count) > 0) return; // settled — don't reschedule
        }
      } catch {
        if (cancelled) return;
      }
      timer = setTimeout(poll, 5000);
    };
    poll();
    return () => { cancelled = true; if (timer) clearTimeout(timer); };
  }, []);

  // Telemetry poll
  useEffect(() => {
    const fetchTelemetry = async () => {
      try {
        const resp = await fetch(`/api/vss/latest?vehicleId=${encodeURIComponent(vehicleIdRef.current || "")}&t=${Date.now()}`);
        if (!resp.ok) { setCockpit(null); return; }
        const data = await resp.json();
        setCockpit(deriveCockpit(data));
      } catch { setCockpit(null); }
    };
    fetchTelemetry();
    const id = setInterval(fetchTelemetry, 3000);
    return () => clearInterval(id);
  }, []);

  // Simulator status poll
  useEffect(() => {
    const refresh = async () => {
      try {
        const resp = await fetch(`/api/vss/simulator/status?vehicleId=${encodeURIComponent(vehicleIdRef.current || "")}`);
        const d = await resp.json().catch(() => ({}));
        setSimRunning(!!d.running);
      } catch { setSimRunning(false); }
    };
    refresh();
    const id = setInterval(refresh, 5000);
    return () => clearInterval(id);
  }, []);

  // Auto-scroll chat
  useEffect(() => {
    if (messagesRef.current) messagesRef.current.scrollTop = messagesRef.current.scrollHeight;
  }, [messages, streaming]);

  // Stop playback and free the current object URL. Revoking here (not only in onended)
  // prevents a blob-URL leak when playback is interrupted before it finishes.
  const stopTts = useCallback(() => {
    if (audioRef.current) { audioRef.current.pause(); audioRef.current = null; }
    if (audioUrlRef.current) { URL.revokeObjectURL(audioUrlRef.current); audioUrlRef.current = null; }
    setSpeakingId(null);
  }, []);

  // Speak `text`, tagging the playing audio with `id` so a message can show a Stop
  // control and the button state stays in sync when playback ends or is interrupted.
  const playTts = useCallback(async (text, id = null) => {
    if (!text) return;
    try {
      stopTts(); // release any audio (and its object URL) already loaded
      setSpeakingId(id);
      const resp = await fetch("/api/tts", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ text }) });
      if (!resp.ok) { setSpeakingId(null); return; }
      const blob = await resp.blob();
      const url = URL.createObjectURL(blob);
      audioUrlRef.current = url;
      const audio = new Audio(url);
      audioRef.current = audio;
      const release = () => {
        URL.revokeObjectURL(url);
        if (audioUrlRef.current === url) audioUrlRef.current = null;
        if (audioRef.current === audio) audioRef.current = null;
      };
      audio.onended = () => { release(); setSpeakingId(null); };
      audio.play().catch(() => { release(); setSpeakingId(null); });
    } catch { setSpeakingId(null); }
  }, [stopTts]);

  const openMap = useCallback(async (route) => {
    setNavRoute(route);
    if (!leafletRef.current) leafletRef.current = (await import("leaflet")).default;
    const L = leafletRef.current;
    setTimeout(() => {
      if (!mapRef.current) return;
      let map = mapObjectsRef.current.map;
      if (!map) {
        map = L.map(mapRef.current, { zoomControl: true, attributionControl: true }).setView([48.8566, 2.3522], 13);
        L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", { attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>', maxZoom: 19 }).addTo(map);
        mapObjectsRef.current.map = map;
      }
      map.invalidateSize();
      const { destination, route: r } = route;
      const { destMarker, routeLayer } = mapObjectsRef.current;
      if (destMarker) map.removeLayer(destMarker);
      if (routeLayer) map.removeLayer(routeLayer);
      const icon = L.divIcon({ className: "", html: '<div style="font-size:2rem;line-height:1;filter:drop-shadow(0 2px 4px #000)">🏁</div>', iconSize: [32, 32], iconAnchor: [16, 28] });
      mapObjectsRef.current.destMarker = L.marker([destination.lat, destination.lon], { icon }).addTo(map).bindPopup(`<b>${destination.name}</b>`).openPopup();
      mapObjectsRef.current.routeLayer = L.geoJSON(r.geometry, { style: { color: "#00ED64", weight: 5, opacity: 0.85, lineCap: "round", lineJoin: "round" } }).addTo(map);
      map.fitBounds(mapObjectsRef.current.routeLayer.getBounds(), { padding: [40, 40] });
    }, 300);
  }, []);

  const sendMessage = useCallback(async (text) => {
    text = (text || "").trim();
    if (!text || busy) return;
    stopTts();
    setMessages((m) => [...m, { id: msgIdRef.current++, role: "user", text, time: now() }]);
    setStreaming({ text: "" });
    setBusy(true);
    setStatusText("");

    let acc = "";
    let done = null;
    try {
      const resp = await fetch("/api/chat/stream", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ message: text, network_mode: onlineRef.current ? "online" : "offline", lat: coordsRef.current.lat, lon: coordsRef.current.lon, conversation_id: conversationIdRef.current, user_id: userIdRef.current, vehicle_id: vehicleIdRef.current }),
      });
      const reader = resp.body.getReader();
      const decoder = new TextDecoder();
      let buf = "";
      while (true) {
        const { value, done: rdone } = await reader.read();
        if (rdone) break;
        buf += decoder.decode(value, { stream: true });
        const parts = buf.split("\n\n");
        buf = parts.pop();
        for (const part of parts) {
          const line = part.split("\n").find((l) => l.startsWith("data: "));
          if (!line) continue;
          let event;
          try { event = JSON.parse(line.slice(6)); } catch { continue; }
          if (event.meta) {
            // Adopt the backend-resolved ids so later turns reuse the same thread (memory).
            if (event.meta.conversation_id) conversationIdRef.current = event.meta.conversation_id;
            if (event.meta.user_id) userIdRef.current = event.meta.user_id;
          }
          else if (event.token) { acc += event.token; setStreaming({ text: acc }); }
          else if ("status" in event) { setStatusText(event.status || ""); }
          else if (event.done) { done = event; }
          else if (event.error) { acc = "⚠️ " + event.error; }
        }
      }
    } catch (e) {
      acc = acc || "⚠️ Sorry, I encountered an error: " + e.message;
    }

    const answer = (done && done.answer) || acc || "I couldn't generate a response.";
    const tools = (done && done.tools_used) || [];
    const sources = (done && done.sources) || [];
    setStreaming(null);
    setStatusText("");
    const assistantId = msgIdRef.current++;
    setMessages((m) => [...m, { id: assistantId, role: "assistant", text: answer, tools, sources, time: now() }]);
    setBusy(false);
    if (done && done.navigation) openMap(done.navigation);
    playTts(answer, assistantId);
  }, [busy, openMap, playTts, stopTts]);

  // ── Mic (MediaRecorder → /api/stt) ────────────────────────────────────────────
  const toggleMic = useCallback(async () => {
    if (listening) {
      if (mediaRef.current && mediaRef.current.state !== "inactive") mediaRef.current.stop();
      return;
    }
    if (!navigator.mediaDevices?.getUserMedia) {
      setMessages((m) => [...m, { id: msgIdRef.current++, role: "assistant", text: "Microphone is not available in this browser.", time: now() }]);
      return;
    }
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      const rec = new MediaRecorder(stream);
      const chunks = [];
      rec.ondataavailable = (e) => { if (e.data.size) chunks.push(e.data); };
      rec.onstop = async () => {
        stream.getTracks().forEach((t) => t.stop());
        setListening(false);
        setStatusText("Transcribing…");
        try {
          const blob = new Blob(chunks, { type: rec.mimeType || "audio/webm" });
          const fd = new FormData();
          fd.append("audio", blob, "speech.webm");
          const resp = await fetch("/api/stt", { method: "POST", body: fd });
          const d = await resp.json();
          setStatusText("");
          if (d.text) sendMessage(d.text);
        } catch { setStatusText(""); }
      };
      mediaRef.current = rec;
      rec.start();
      setListening(true);
      setStatusText("🎤 Listening…");
    } catch {
      setListening(false);
      setMessages((m) => [...m, { id: msgIdRef.current++, role: "assistant", text: "Microphone access was denied. You can type your message below.", time: now() }]);
    }
  }, [listening, sendMessage]);

  const toggleSim = useCallback(async () => {
    const action = simRunning ? "stop" : "start";
    const vid = vehicleIdRef.current || "";
    try {
      await fetch(`/api/vss/simulator/${action}`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ vehicle_id: vid }),
      });
    } catch {}
    try {
      const d = await (await fetch(`/api/vss/simulator/status?vehicleId=${encodeURIComponent(vid)}`)).json();
      setSimRunning(!!d.running);
    } catch {}
  }, [simRunning]);

  const c = cockpit;
  const dtcCatalog = dtcCatalogRef.current;

  return (
    <div className={`cockpit${online ? "" : " "}`} data-mode={online ? "online" : "offline"}>
      <OfflineBodyClass online={online} />

      {/* Top bar */}
      <header className="cockpit-top">
        <button className={`sim-btn${simRunning ? " running" : ""}`} onClick={toggleSim} title="Start or stop the telemetry simulator">
          <span className="sim-dot">{simRunning ? "⏹" : "▶"}</span>
          <span>{simRunning ? "STOP SIMULATION" : "START SIMULATION"}</span>
        </button>
        <div className="brand">
          <span className="brand-leaf">🍃</span>
          <span className="brand-name">MongoDB<span className="brand-x">×</span>ObjectBox</span>
          <span className="brand-tag">EDGE COCKPIT</span>
        </div>
        <div className="top-right">
          <button className="hdr-btn hdr-info" onClick={() => setInfoOpen(true)} title="How it works">ⓘ How it works</button>
          <button className="hdr-btn hdr-sync" onClick={() => setSceneOpen(true)} title="Live sync & data model">⧉ Sync &amp; Data</button>
          <button className="hdr-btn hdr-tour" onClick={() => setRunTour(true)} title="Guided tour">?</button>
          <button className={`conn-toggle${online ? " online" : ""}`} onClick={toggleOnline} title="Switch online / offline — also pauses/resumes Atlas sync">
            <span className="conn-dot" />
            <span>{online ? "ONLINE" : "OFFLINE"}</span>
          </button>
          <span className="veh-name">{vehicleId || "—"}</span>
        </div>
      </header>

      {/* Status row */}
      <div className="cockpit-status">
        <div className="status-left">
          <span className="clock">{clock}</span>
          <span className="sep">·</span>
          <span>{c?.ext == null ? "--°C" : `${Math.round(c.ext)}°C`}</span>
        </div>
        <div className="telltales">
          {TT_ORDER.map(([id, label]) => (
            <div key={id} className={`tt ${c?.levels?.[id] || "off"}`} title={label}>
              <span>{TT_ICONS[id]}</span>
              <label>{label}</label>
            </div>
          ))}
        </div>
        <div className="status-right"><span>📚 <span>{chunkCount}</span></span></div>
      </div>

      {/* Main */}
      <main className="cockpit-main">
        <Gauge gid="rpm" value={c?.rpm} max={RPM_MAX} labelMax={8} labels={[0, 1, 2, 3, 4, 5, 6, 7, 8]} redline unit="1/MIN × 1000" valueLg
               display={c?.rpm == null ? "N/A" : (c.rpm / 1000).toFixed(1)} />

        <section className="copilot">
          <div className="copilot-head">
            <div className="copilot-title"><span className="leaf">🍃</span><span>Leafy In Car Voice Assistant</span></div>
            <div className={`copilot-live${online ? " on" : ""}`}>{online ? "● LIVE TELEMETRY CONNECTED" : "● OFFLINE"}</div>
          </div>

          <div className="chat-messages" ref={messagesRef}>
            {messages.map((m) => (
              <Message
                key={m.id}
                m={m}
                speaking={speakingId === m.id}
                onSpeak={() => playTts(m.text, m.id)}
                onStop={stopTts}
              />
            ))}
            {streaming && (
              <div className="message assistant-message thinking-bubble">
                <div className="message-avatar">🤖</div>
                <div className="message-content">
                  <div className="message-text">
                    {streaming.text || "Processing"}
                    {!streaming.text && <span className="status-dots"><span /><span /><span /></span>}
                  </div>
                </div>
              </div>
            )}
          </div>

          <div className={`chat-status${statusText ? " active" : ""}`}>
            <div className="status-dots"><span /><span /><span /></div>
            <span className="status-text">{statusText}</span>
          </div>

          <div className="copilot-input">
            <input type="text" className="chat-input" placeholder="Ask Leafy In Car Voice Assistant…" autoComplete="off"
                   value={input} onChange={(e) => setInput(e.target.value)}
                   onKeyDown={(e) => { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); sendMessage(input); setInput(""); } }} />
            <button className={`mic-button${listening ? " listening" : ""}`} onClick={toggleMic} title="Tap to talk"><span>🎤</span></button>
            <button className="send-btn" onClick={() => { sendMessage(input); setInput(""); }} disabled={busy} title="Send">➤</button>
          </div>
        </section>

        <Gauge gid="spd" value={c?.spd} max={SPD_MAX} labels={[0, 40, 80, 120, 160, 200, 240]} minorInterval={20} unit="km/h" valueLg
               display={c?.spd == null ? "N/A" : Math.round(c.spd)} />

        {navRoute && (
          <div className="nav-panel">
            <div className="nav-panel-header">
              <div className="nav-panel-title"><span>📍</span><span>{navRoute.destination?.name || "Route"}</span></div>
              <button className="nav-panel-close" onClick={() => setNavRoute(null)}>✕</button>
            </div>
            <div className="map-container" ref={mapRef} />
          </div>
        )}
      </main>

      {/* Bottom bar */}
      <footer className="cockpit-bottom">
        <div className="dtc-ticker">
          <span className="dtc-ticker-label">ACTIVE DTC CODES</span>
          <span className="dtc-ticker-list">
            {!c ? <span className="dtc-none">Data not available</span> :
              (c.displayCodes.length === 0 ? <span className="dtc-none">No active fault codes</span> :
                c.displayCodes.map((code) => {
                  const c0 = String(code)[0];
                  const chassis = c0 === "C" || c0 === "B";
                  return <span key={code} className={`dtc-chip2${chassis ? " chassis" : ""}`} title={dtcCatalog[code] || "Unknown fault code"}>{code}</span>;
                }))}
          </span>
        </div>
        <div className="bottom-metrics">
          <div className="metric metric-range">
            <span className="m-label">RANGE POTENTIAL</span>
            <span className="m-val">{c?.range == null ? "-- km" : `${Math.round(c.range)} km`}</span>
            <div className="m-bar"><div className="m-bar-fill" style={{ width: `${Math.max(0, Math.min(100, ((c?.range || 0) / 500) * 100))}%` }} /></div>
          </div>
          <div className="metric"><span className="m-label">ECON</span><span className="m-val econ">{c?.econ == null ? "--" : `${c.econ.toFixed(1)} L/100km`}</span></div>
          <div className="metric"><span className="m-label">TOTAL DIST</span><span className="m-val">{c?.dist == null ? "--" : `${Number(c.dist).toLocaleString()} km`}</span></div>
          <div className="metric"><span className="m-label">NEXT SERVICE</span><span className="m-val">{c?.svc == null ? "--" : `${Math.round(c.svc).toLocaleString()} km`}</span></div>
          <div className="gear-select">
            {["P", "R", "N", "D", "S"].map((g) => <span key={g} className={c?.gear === g ? "active" : ""}>{g}</span>)}
          </div>
        </div>
      </footer>

      <InfoWizard open={infoOpen} onClose={() => setInfoOpen(false)} />

      {sceneOpen && (
        <div className="overlay" onClick={() => setSceneOpen(false)}>
          <div className="scene-modal" onClick={(e) => e.stopPropagation()}>
            <div className="iw-head">
              <span className="iw-title">🔄 Sync &amp; Data Model</span>
              <button className="overlay-close" onClick={() => setSceneOpen(false)}>✕</button>
            </div>
            <div className="scene-body">
              <SyncPanel onPausedChange={handleSyncPaused} vehicleId={vehicleId} />
              <DataModelPanel />
            </div>
          </div>
        </div>
      )}

      <Walkthrough run={runTour} onClose={endTour} />
    </div>
  );
}

function Message({ m, onSpeak, onStop, speaking }) {
  const isUser = m.role === "user";
  return (
    <div className={`message ${isUser ? "user-message" : "assistant-message"}`}>
      <div className="message-avatar">{isUser ? "👤" : "🤖"}</div>
      <div className="message-content">
        <div className="message-text">
          {m.text}
          {m.sources && m.sources.length > 0 && (
            <div className="message-sources">
              {m.sources.map((s, i) => (
                <div className="source-chip" key={i}>
                  <div className="source-chip-header">
                    <span>📄 Car manual</span>
                    <span>Match: {s.score !== undefined ? (s.score * 100).toFixed(1) + "%" : "N/A"}</span>
                  </div>
                  <div>{s.text?.length > 150 ? s.text.slice(0, 150) + "..." : s.text}</div>
                </div>
              ))}
            </div>
          )}
        </div>
        {m.tools && m.tools.length > 0 && (
          <div className="tool-badges">
            {m.tools.map((name, i) => {
              const t = TOOL_LABELS[name] || { icon: "🔩", label: name };
              return <span className="tool-badge" key={i}>{t.icon} {t.label}</span>;
            })}
          </div>
        )}
        <div className="message-meta">
          {!isUser && (
            <button
              type="button"
              className={`tts-btn${speaking ? " speaking" : ""}`}
              onClick={speaking ? onStop : onSpeak}
              title={speaking ? "Stop reading" : "Read aloud"}
              aria-label={speaking ? "Stop reading" : "Read aloud"}
            >
              {speaking ? "🔊" : "🔇"}
            </button>
          )}
          <span className="message-time">{m.time}</span>
        </div>
      </div>
    </div>
  );
}

// Toggle the body.offline-mode class (orange frame) as a side effect.
function OfflineBodyClass({ online }) {
  useEffect(() => {
    document.body.classList.toggle("offline-mode", !online);
  }, [online]);
  return null;
}
