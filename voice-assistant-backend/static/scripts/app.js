// Car Cockpit — Copilot Assistant + live VSS telemetry
// Connects to the Python backend (Socket.IO) and renders the cockpit.

// Initialize Socket.IO connection
const socket = io();

// State management
let isListening = false;
let currentAudio = null;

// DOM Elements
const micButton = document.getElementById('micButton');
const micIcon = document.getElementById('micIcon');
const chatMessages = document.getElementById('chatMessages');
const chatStatus = document.getElementById('chatStatus');
const statusText = document.getElementById('statusText');
const chunkCount = document.getElementById('chunkCount');
const chatInput = document.getElementById('chatInput');
const sendBtn = document.getElementById('sendBtn');

// Helper: Get current time
function getCurrentTime() {
    const now = new Date();
    return now.toLocaleTimeString('en-US', { hour: 'numeric', minute: '2-digit' });
}

// Helper: Scroll chat to bottom
function scrollToBottom() {
    chatMessages.scrollTop = chatMessages.scrollHeight;
}

// Helper: Add user message to chat
function addUserMessage(text) {
    const messageDiv = document.createElement('div');
    messageDiv.className = 'message user-message';
    messageDiv.innerHTML = `
        <div class="message-avatar">👤</div>
        <div class="message-content">
            <div class="message-text">${escapeHtml(text)}</div>
            <div class="message-time">${getCurrentTime()}</div>
        </div>
    `;
    chatMessages.appendChild(messageDiv);
    scrollToBottom();
}

const TOOL_LABELS = {
    'search_car_manual_objectbox': { icon: '📦', label: 'ObjectBox Search' },
    'search_car_manual_atlas':     { icon: '🍃', label: 'MongoDB Atlas Vector Search' },
    'navigate_to':              { icon: '🗺️', label: 'Navigation' },
    'get_vehicle_status':       { icon: '📡', label: 'Vehicle Status' },
    'get_powertrain_status':    { icon: '⚙️', label: 'Powertrain' },
    'get_battery_status':       { icon: '🔋', label: 'Battery' },
    'get_chassis_status':       { icon: '🔧', label: 'Chassis' },
    'get_cabin_status':         { icon: '🚪', label: 'Cabin' },
    'get_location':             { icon: '📍', label: 'Location' },
    'get_adas_status':          { icon: '🛡️', label: 'ADAS' },
    'get_diagnostics':          { icon: '🔧', label: 'Diagnostics' },
    'get_vehicle_events':       { icon: '⚠️', label: 'Vehicle Events' },
    'get_driving_history':      { icon: '📈', label: 'Driving History' },
};

// ── Thinking bubble ───────────────────────────────────────────────────────────

let _thinkingBubble = null;

function showThinkingBubble() {
    removeThinkingBubble();
    const div = document.createElement('div');
    div.className = 'message assistant-message thinking-bubble';
    div.innerHTML = `
        <div class="message-avatar">🤖</div>
        <div class="message-content">
            <div class="message-text">
                Processing
                <span class="status-dots"><span></span><span></span><span></span></span>
            </div>
        </div>
    `;
    chatMessages.appendChild(div);
    scrollToBottom();
    _thinkingBubble = div;
}

function removeThinkingBubble() {
    if (_thinkingBubble) {
        _thinkingBubble.remove();
        _thinkingBubble = null;
    }
}

// Helper: Add assistant message to chat
function addAssistantMessage(text, sources = null, toolsUsed = []) {
    const messageDiv = document.createElement('div');
    messageDiv.className = 'message assistant-message';

    let sourcesHtml = '';
    if (sources && sources.length > 0) {
        sourcesHtml = '<div class="message-sources">';
        sources.forEach(source => {
            const score = source.score !== undefined ? (source.score * 100).toFixed(1) + '%' : 'N/A';
            const sourceText = source.text.length > 150 ? source.text.substring(0, 150) + '...' : source.text;
            sourcesHtml += `
                <div class="source-chip">
                    <div class="source-chip-header">
                        <span>📄 Car manual</span>
                        <span>Match: ${score}</span>
                    </div>
                    <div>${escapeHtml(sourceText)}</div>
                </div>
            `;
        });
        sourcesHtml += '</div>';
    }

    let toolsHtml = '';
    if (toolsUsed && toolsUsed.length > 0) {
        const badges = toolsUsed.map(name => {
            const t = TOOL_LABELS[name] || { icon: '🔩', label: name };
            return `<span class="tool-badge">${t.icon} ${t.label}</span>`;
        }).join('');
        toolsHtml = `<div class="tool-badges">${badges}</div>`;
    }

    messageDiv.innerHTML = `
        <div class="message-avatar">🤖</div>
        <div class="message-content">
            <div class="message-text">${escapeHtml(text)}${sourcesHtml}</div>
            ${toolsHtml}
            <div class="message-time">${getCurrentTime()}</div>
        </div>
    `;
    chatMessages.appendChild(messageDiv);
    scrollToBottom();
}

// Helper: Escape HTML
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Helper: Show/hide status
function showStatus(message) {
    statusText.textContent = message;
    chatStatus.classList.add('active');
}

function hideStatus() {
    chatStatus.classList.remove('active');
}

// Connection events
socket.on('connect', () => {
    console.log('✅ Connected to backend server');
    hideStatus();
    micButton.disabled = false;
});

socket.on('disconnect', () => {
    console.log('❌ Disconnected from backend');
    micButton.disabled = true;
    isListening = false;
    micButton.classList.remove('listening');
    hideStatus();
});

// Status updates
socket.on('status', (data) => {
    switch(data.state) {
        case 'listening':
            showStatus('🎤 Listening...');
            micIcon.textContent = '🎤';
            break;
        case 'processing':
            hideStatus();
            break;
        case 'speaking':
            showStatus('🔊 Speaking...');
            micIcon.textContent = '🔊';
            break;
        case 'ready':
            hideStatus();
            micIcon.textContent = '🎤';
            break;
    }
});

// Statistics
socket.on('stats', (data) => {
    if (data.chunk_count !== undefined) {
        chunkCount.textContent = data.chunk_count.toLocaleString();
    }
});

// Question received — only used for voice path; text path adds the message locally
let _pendingTextMessage = null;
socket.on('question', (data) => {
    if (_pendingTextMessage && _pendingTextMessage === data.text) {
        _pendingTextMessage = null;
        return;
    }
    _pendingTextMessage = null;
    addUserMessage(data.text);
    showThinkingBubble();
});

// Search results (stored for answer)
let currentSources = [];
socket.on('search_results', (data) => {
    currentSources = data.chunks || [];
});

// Incremental token stream — update the thinking bubble text as tokens arrive
let _streamedText = '';
socket.on('answer_token', (data) => {
    _streamedText += data.text;
    if (_thinkingBubble) {
        const textEl = _thinkingBubble.querySelector('.message-text');
        if (textEl) textEl.textContent = _streamedText;
        scrollToBottom();
    }
});

// Answer received — finalise the streamed bubble with tools/sources metadata
socket.on('answer', (data) => {
    _streamedText = '';
    removeThinkingBubble();
    addAssistantMessage(data.text, currentSources, data.tools_used || []);
    currentSources = [];
    sendBtn.disabled = false;
});

// Audio pushed by backend — play when received
socket.on('audio', (data) => {
    if (currentAudio) {
        currentAudio.pause();
        URL.revokeObjectURL(currentAudio.src);
        currentAudio = null;
    }
    try {
        const bytes = Uint8Array.from(atob(data.data), c => c.charCodeAt(0));
        const blob  = new Blob([bytes], { type: 'audio/wav' });
        const url   = URL.createObjectURL(blob);
        currentAudio = new Audio(url);
        currentAudio.onended = () => { URL.revokeObjectURL(url); currentAudio = null; };
        currentAudio.play().catch(() => {});
    } catch (e) {
        console.error('Audio playback error:', e);
    }
});

// Error handling
socket.on('error', (data) => {
    console.error('❌ Error:', data.message);
    removeThinkingBubble();
    addAssistantMessage('⚠️ Sorry, I encountered an error: ' + data.message);
    isListening = false;
    micButton.classList.remove('listening');
    hideStatus();
    sendBtn.disabled = false;
});

socket.on('session_complete', () => {
    hideStatus();
});

// Tool-call progress indicator — shown while the agent waits for a tool to execute
socket.on('agent_status', (data) => {
    if (data.text) {
        showStatus(data.text);
    } else {
        hideStatus();
    }
});

// ── Browser speech recognition (Web Speech API) ───────────────────────────────

let recognition = null;

function initRecognition() {
    const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SR) return false;

    recognition = new SR();
    recognition.continuous = false;
    recognition.interimResults = false;
    recognition.lang = 'en-US';

    recognition.onstart = () => {
        showStatus('🎤 Listening...');
        micIcon.textContent = '🎤';
    };

    recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript.trim();
        if (transcript) {
            socket.emit('send_message', { text: transcript });
        }
    };

    recognition.onerror = (event) => {
        console.error('Speech error:', event.error);
        if (event.error === 'not-allowed' || event.error === 'service-not-allowed') {
            addAssistantMessage('Microphone access was denied. Please allow it in the browser address bar and try again.');
        } else if (event.error !== 'no-speech') {
            addAssistantMessage(`Microphone error: ${event.error}. You can also type your message below.`);
        }
        stopListening();
    };

    recognition.onend = () => stopListening();

    return true;
}

function startListening() {
    if (!recognition && !initRecognition()) {
        addAssistantMessage('Speech recognition is not supported in this browser. Please use Chrome or Edge, or type your message below.');
        return;
    }
    isListening = true;
    micButton.classList.add('listening');
    try { recognition.start(); } catch (e) { stopListening(); }
}

function stopListening() {
    isListening = false;
    micButton.classList.remove('listening');
    hideStatus();
    micIcon.textContent = '🎤';
}

micButton.addEventListener('click', () => {
    if (isListening) {
        if (recognition) try { recognition.stop(); } catch (e) {}
        stopListening();
    } else {
        startListening();
    }
});

// Text input send
function sendTextMessage() {
    const text = chatInput.value.trim();
    if (!text) return;
    if (currentAudio) { currentAudio.pause(); currentAudio = null; }
    addUserMessage(text);
    showThinkingBubble();
    _pendingTextMessage = text;
    chatInput.value = '';
    sendBtn.disabled = true;
    socket.emit('send_message', { text });
}

sendBtn.addEventListener('click', sendTextMessage);

chatInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        sendTextMessage();
    }
});

console.log('🚗 Car Cockpit UI initialized');

// ── Live clock ─────────────────────────────────────────────────────────────────
const clockEl = document.getElementById('clock');
function tickClock() {
    if (clockEl) clockEl.textContent = new Date().toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
}
tickClock();
setInterval(tickClock, 1000);

// ── Telemetry helpers ────────────────────────────────────────────────────────
// Read a nested VSS path from the /api/vss/latest response (the VSS Vehicle tree).
function vssPick(data, path) {
    return path.split('.').reduce((o, k) => (o == null ? undefined : o[k]), data);
}

// ── Circular gauges (SVG arcs, 270° sweep with a gap at the bottom) ───────────
const G = { cx: 130, cy: 130, r: 100, rTick: 128, rTickInner: 85, a0: 225, span: 270 };
function gpolar(deg, r) {
    const a = (deg - 90) * Math.PI / 180;
    return [G.cx + r * Math.cos(a), G.cy + r * Math.sin(a)];
}
function gArc(frac0, frac1, r) {
    const s = G.a0 + G.span * frac0, e = G.a0 + G.span * frac1;
    const [x0, y0] = gpolar(s, r), [x1, y1] = gpolar(e, r);
    const large = (e - s) <= 180 ? 0 : 1;
    return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}
const NEEDLE_LEN = 82;
function initGauge(cfg) {
    const track = document.getElementById(cfg.track);
    if (track) track.setAttribute('d', gArc(0, 1, G.r));
    if (cfg.redline) {
        const rl = document.getElementById(cfg.redline);
        if (rl) rl.setAttribute('d', gArc(0.85, 1, G.r));
    }
    const ticks = document.getElementById(cfg.ticks);
    if (ticks) {
        const majorSet = new Set(cfg.labels.map(v => v / cfg.max));
        let minorHtml = '';
        if (cfg.minorInterval) {
            for (let v = 0; v <= cfg.max; v += cfg.minorInterval) {
                const frac = v / cfg.max;
                if (majorSet.has(frac)) continue;
                const ang = G.a0 + G.span * frac;
                const [ix, iy] = gpolar(ang, G.r - 8);
                const [ox, oy] = gpolar(ang, G.r);
                minorHtml += `<line x1="${ix.toFixed(1)}" y1="${iy.toFixed(1)}" x2="${ox.toFixed(1)}" y2="${oy.toFixed(1)}" class="g-tick-minor"/>`;
            }
        }
        const majorHtml = cfg.labels.map(v => {
            const ang = G.a0 + G.span * (v / cfg.max);
            const [tx, ty] = gpolar(ang, G.rTick);
            const [ix, iy] = gpolar(ang, G.rTickInner);
            const [ox, oy] = gpolar(ang, G.r);
            return `<line x1="${ix.toFixed(1)}" y1="${iy.toFixed(1)}" x2="${ox.toFixed(1)}" y2="${oy.toFixed(1)}"/>` +
                   `<text x="${tx.toFixed(1)}" y="${ty.toFixed(1)}">${v}</text>`;
        }).join('');
        ticks.innerHTML = minorHtml + majorHtml;
    }
    // initialise needle and fill arc at 0
    const needle = document.getElementById(cfg.needle);
    if (needle) {
        const [x2, y2] = gpolar(G.a0, NEEDLE_LEN);
        needle.setAttribute('x2', x2.toFixed(1));
        needle.setAttribute('y2', y2.toFixed(1));
    }
    if (cfg.fill) {
        const fillEl = document.getElementById(cfg.fill);
        if (fillEl) fillEl.setAttribute('d', '');
    }
}
function setNeedle(needleId, fillId, value, max) {
    const frac = Math.max(0, Math.min(1, (value || 0) / max));
    const [x2, y2] = gpolar(G.a0 + G.span * frac, NEEDLE_LEN);
    const el = document.getElementById(needleId);
    if (el) { el.setAttribute('x2', x2.toFixed(1)); el.setAttribute('y2', y2.toFixed(1)); }
    if (fillId) {
        const fillEl = document.getElementById(fillId);
        if (fillEl) fillEl.setAttribute('d', frac > 0.001 ? gArc(0, frac, G.r) : '');
    }
}
const RPM_MAX = 8000, SPD_MAX = 240;
initGauge({ track: 'rpm-track', redline: 'rpm-redline', ticks: 'rpm-ticks', needle: 'rpm-needle', fill: 'rpm-fill', labels: [0,1,2,3,4,5,6,7,8], max: 8 });
initGauge({ track: 'spd-track', ticks: 'spd-ticks', needle: 'spd-needle', fill: 'spd-fill', labels: [0,40,80,120,160,200,240], max: SPD_MAX, minorInterval: 20 });

// ── Tell-tales ────────────────────────────────────────────────────────────────
const TELLTALE_IDS = ['tt-mil', 'tt-abs', 'tt-tpms', 'tt-belt', 'tt-temp', 'tt-batt', 'tt-oil', 'tt-fuel'];
function setTellTale(id, level) {
    const el = document.getElementById(id);
    if (!el) return;
    el.classList.remove('off', 'amber', 'red');
    el.classList.add(level || 'off');
}
function worse(a, b) { const r = { off: 0, amber: 1, red: 2 }; return r[b] > r[a] ? b : a; }

// ── DTC catalog + bottom ticker ────────────────────────────────────────────────
let _dtcCatalog = {};
fetch('/static/dtc_catalog.json')
    .then(r => r.ok ? r.json() : {})
    .then(c => { _dtcCatalog = c || {}; })
    .catch(() => { _dtcCatalog = {}; });

function renderDtcTicker(codes) {
    const el = document.getElementById('dtcTicker');
    if (!el) return;
    if (!codes || !codes.length) {
        el.innerHTML = '<span class="dtc-none">No active fault codes</span>';
        return;
    }
    el.innerHTML = codes.map(code => {
        const desc = _dtcCatalog[code] || 'Unknown fault code';
        const c0 = String(code)[0];
        const chassis = c0 === 'C' || c0 === 'B';
        return `<span class="dtc-chip2 ${chassis ? 'chassis' : ''}" title="${escapeHtml(desc)}">${escapeHtml(code)}</span>`;
    }).join('');
}

// ── Gear selector ──────────────────────────────────────────────────────────────
function setGear(gearNum) {
    let g = null;
    if (gearNum != null) g = gearNum < 0 ? 'R' : gearNum === 0 ? 'N' : 'D';
    document.querySelectorAll('#gearSelect span').forEach(s => s.classList.toggle('active', s.dataset.g === g));
}

// ── Main cockpit update ─────────────────────────────────────────────────────────
function updateCockpit(data) {
    if (!data) return;

    // Gauges
    const rpm = vssPick(data, 'Powertrain.CombustionEngine.Speed');
    setNeedle('rpm-needle', 'rpm-fill', rpm, RPM_MAX);
    const rpmEl = document.getElementById('rpm-num');
    if (rpmEl) rpmEl.textContent = rpm == null ? 'N/A' : (rpm / 1000).toFixed(1);

    const spd = vssPick(data, 'Speed');
    setNeedle('spd-needle', 'spd-fill', spd, SPD_MAX);
    const spdEl = document.getElementById('speed-num');
    if (spdEl) spdEl.textContent = spd == null ? 'N/A' : Math.round(spd);


    // Exterior temperature
    const ext = vssPick(data, 'Exterior.AirTemperature');
    const extEl = document.getElementById('ext-temp');
    if (extEl) extEl.textContent = ext == null ? '--°C' : `${Math.round(ext)}°C`;

    // Bottom metrics
    const range = vssPick(data, 'Powertrain.TractionBattery.Range');
    const rEl = document.getElementById('range-val');
    if (rEl) rEl.textContent = range == null ? '-- km' : `${Math.round(range)} km`;
    const rBar = document.getElementById('range-bar');
    if (rBar) rBar.style.width = Math.max(0, Math.min(100, (range || 0) / 500 * 100)) + '%';

    const econ = vssPick(data, 'Powertrain.FuelSystem.InstantConsumption');
    const eEl = document.getElementById('econ-val');
    if (eEl) eEl.textContent = econ == null ? '--' : `${econ.toFixed(1)} L/100km`;

    const dist = vssPick(data, 'TraveledDistance');
    const dEl = document.getElementById('dist-val');
    if (dEl) dEl.textContent = dist == null ? '--' : `${Number(dist).toLocaleString()} km`;

    const svc = vssPick(data, 'Service.DistanceToService');
    const sEl = document.getElementById('service-val');
    if (sEl) sEl.textContent = svc == null ? '--' : `${Math.round(svc).toLocaleString()} km`;

    setGear(vssPick(data, 'Powertrain.Transmission.CurrentGear'));

    // Diagnostics + tell-tales
    const dg = vssPick(data, 'Diagnostics') || {};
    const codes = Array.isArray(dg.DTCList) ? dg.DTCList : [];

    // Sensor-based tell-tale state (baseline)
    const coolant = vssPick(data, 'Powertrain.CombustionEngine.EngineCoolant.Temperature');
    let temp = coolant == null ? 'off' : coolant > 110 ? 'red' : coolant > 100 ? 'amber' : 'off';
    const fuelPct = vssPick(data, 'Powertrain.FuelSystem.RelativeLevel');
    let fuel = fuelPct == null ? 'off' : fuelPct < 10 ? 'red' : fuelPct < 20 ? 'amber' : 'off';
    const soc = vssPick(data, 'Powertrain.TractionBattery.StateOfCharge.Current');
    let batt = soc == null ? 'off' : soc < 15 ? 'red' : soc < 25 ? 'amber' : 'off';
    let tpms = 'off';
    ['Chassis.Axle.Row1.Wheel.Left.Tire.Pressure', 'Chassis.Axle.Row1.Wheel.Right.Tire.Pressure',
     'Chassis.Axle.Row2.Wheel.Left.Tire.Pressure', 'Chassis.Axle.Row2.Wheel.Right.Tire.Pressure'].forEach(p => {
        const v = vssPick(data, p);
        if (v == null) return;
        tpms = worse(tpms, v < 193 ? 'red' : (v < 207 || v > 241) ? 'amber' : 'off');
    });
    const belted = vssPick(data, 'Cabin.Seat.Row1.DriverSide.IsBelted');
    let belt = belted == null ? 'off' : belted ? 'off' : 'red';
    const oilPsi = vssPick(data, 'Powertrain.CombustionEngine.OilPressure');
    let oil = oilPsi == null ? 'off' : oilPsi < 20 ? 'red' : oilPsi < 35 ? 'amber' : 'off';

    // DTC-based upgrades — custom codes map to specific tell-tales;
    // standard P/U → CHK, standard C → ABS.
    // Custom codes (marked [custom] in dtc_catalog.json, not standard OBD-II):
    //   P1217 → TEMP, P1001 → FUEL, P1002 → BATT, C1001 → TPMS, B1001 → BELT
    // Standard code: P0520 → OIL
    const DTC_TELLTALE = { 'P1217': 'temp', 'P1001': 'fuel', 'P1002': 'batt', 'C1001': 'tpms', 'B1001': 'belt', 'P0520': 'oil' };
    let mil = 'off', abs = 'off';
    codes.forEach(c => {
        const k = String(c)[0];
        const specific = DTC_TELLTALE[c];
        if      (specific === 'temp') temp = worse(temp, 'amber');
        else if (specific === 'fuel') fuel = worse(fuel, 'amber');
        else if (specific === 'batt') batt = worse(batt, 'amber');
        else if (specific === 'tpms') tpms = worse(tpms, 'amber');
        else if (specific === 'belt') belt = worse(belt, 'amber');
        else if (specific === 'oil')  oil  = worse(oil,  'red');
        if (k === 'P' || k === 'U') mil = 'amber';
        else if (k === 'C' && !specific) abs = 'amber';
    });

    setTellTale('tt-mil', mil);
    setTellTale('tt-abs', abs);
    setTellTale('tt-tpms', tpms);
    setTellTale('tt-belt', belt);
    setTellTale('tt-temp', temp);
    setTellTale('tt-batt', batt);
    setTellTale('tt-oil',  oil);
    setTellTale('tt-fuel', fuel);

    // Synthesize DTC codes for tell-tales that fired from sensor thresholds alone
    // (i.e. no fault episode is active but the sensor value crossed the warning band).
    const displayCodes = [...codes];
    const ensure = (active, code) => { if (active !== 'off' && !displayCodes.includes(code)) displayCodes.push(code); };
    ensure(temp, 'P1217');
    ensure(fuel, 'P1001');
    ensure(batt, 'P1002');
    ensure(tpms, 'C1001');
    ensure(belt, 'B1001');
    ensure(oil,  'P0520');
    renderDtcTicker(displayCodes);
}

// ── Availability ────────────────────────────────────────────────────────────────
function markTelemetryUnavailable() {
    setNeedle('rpm-needle', 'rpm-fill', 0, RPM_MAX);
    setNeedle('spd-needle', 'spd-fill', 0, SPD_MAX);
    const rpmEl = document.getElementById('rpm-num'); if (rpmEl) rpmEl.textContent = 'N/A';
    const spdEl = document.getElementById('speed-num'); if (spdEl) spdEl.textContent = 'N/A';
    ['range-val', 'econ-val', 'dist-val', 'service-val'].forEach(id => {
        const e = document.getElementById(id); if (e) e.textContent = 'N/A';
    });
    const rBar = document.getElementById('range-bar'); if (rBar) rBar.style.width = '0%';
    TELLTALE_IDS.forEach(id => setTellTale(id, 'off'));
    setGear(null);
    const t = document.getElementById('dtcTicker');
    if (t) t.innerHTML = '<span class="dtc-none">Data not available</span>';
}

async function fetchTelemetry() {
    try {
        const resp = await fetch(`/api/vss/latest?t=${Date.now()}`);
        if (!resp.ok) { markTelemetryUnavailable(); return; }
        const data = await resp.json();
        if (data.error) { markTelemetryUnavailable(); return; }
        updateCockpit(data);
    } catch (_) {
        markTelemetryUnavailable();
    }
}

fetchTelemetry();
setInterval(fetchTelemetry, 3000);

// ── Network mode toggle (online / offline) ──────────────────────────────────────
let networkMode = 'offline';
const networkToggle = document.getElementById('networkToggle');
const networkLabel  = document.getElementById('networkLabel');
const copilotLive   = document.getElementById('copilotLive');

function setNetworkMode(mode) {
    networkMode = mode;
    const online = mode === 'online';
    networkLabel.textContent = online ? 'ONLINE' : 'OFFLINE';
    networkToggle.classList.toggle('online', online);
    if (copilotLive) {
        copilotLive.textContent = online ? '● LIVE TELEMETRY CONNECTED' : '● OFFLINE';
        copilotLive.classList.toggle('on', online);
    }
    // Orange highlight around the whole screen while offline.
    document.body.classList.toggle('offline-mode', !online);
}

// Apply the initial (offline) state on load until the server reports the mode.
setNetworkMode('offline');

networkToggle.addEventListener('click', () => {
    const next = networkMode === 'offline' ? 'online' : 'offline';
    socket.emit('set_network_mode', { mode: next });
});

socket.on('network_mode_changed', (data) => {
    setNetworkMode(data.mode);
});

// ── Simulator start/stop toggle ─────────────────────────────────────────────────
const simToggle      = document.getElementById('simToggle');
const simToggleIcon  = document.getElementById('simToggleIcon');
const simToggleLabel = document.getElementById('simToggleLabel');
let simRunning = false;

function renderSimState(running) {
    simRunning = running;
    simToggle.classList.toggle('running', running);
    simToggleIcon.textContent  = running ? '⏹' : '▶';
    simToggleLabel.textContent = running ? 'STOP SIMULATION' : 'START SIMULATION';
}

async function refreshSimStatus() {
    try {
        const resp = await fetch('/api/vss/simulator/status');
        const data = await resp.json().catch(() => ({}));
        renderSimState(!!data.running);
    } catch (_) {
        renderSimState(false);
    }
}

simToggle.addEventListener('click', async () => {
    const action = simRunning ? 'stop' : 'start';
    simToggle.disabled = true;
    try {
        await fetch(`/api/vss/simulator/${action}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{}',
        });
    } catch (_) {}
    await refreshSimStatus();
    simToggle.disabled = false;
});

refreshSimStatus();
setInterval(refreshSimStatus, 5000);
