// Car Manual Voice Assistant - Chat Interface
// Connects to Python backend and displays conversation in chat bubbles

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
const searchStatus = document.getElementById('searchStatus');
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
    'get_vehicle_events':       { icon: '⚠️', label: 'Vehicle Events' },
    'get_driving_history':      { icon: '📈', label: 'Driving History' },
};

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
                        <span>📄 ${escapeHtml(source.source || 'Unknown')}</span>
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
    console.log('ℹ️  Voice assistant NOT initialized - waiting for button click');
    hideStatus();
    searchStatus.style.color = 'var(--accent)';
    micButton.disabled = false;
});

socket.on('disconnect', () => {
    console.log('❌ Disconnected from backend');
    searchStatus.textContent = 'Offline';
    searchStatus.style.color = '#FF4444';
    micButton.disabled = true;
    isListening = false;
    micButton.classList.remove('listening');
    hideStatus();
});

// Status updates
socket.on('status', (data) => {
    console.log('📊 Status:', data);
    
    switch(data.state) {
        case 'listening':
            showStatus('🎤 Listening...');
            micIcon.textContent = '🎤';
            break;
        case 'processing':
            showStatus('🔄 Processing...');
            micIcon.textContent = '⚙️';
            break;
        case 'searching':
            showStatus('🔍 Searching manual...');
            micIcon.textContent = '🔍';
            break;
        case 'generating':
            showStatus('💭 Thinking...');
            micIcon.textContent = '🤖';
            break;
        case 'speaking':
            showStatus('🔊 Speaking...');
            micIcon.textContent = '🔊';
            break;
        case 'ready':
            hideStatus();
            micIcon.textContent = '🎤';
            break;
        case 'error':
            hideStatus();
            micIcon.textContent = '❌';
            isListening = false;
            micButton.classList.remove('listening');
            break;
    }
});

// Statistics
socket.on('stats', (data) => {
    console.log('📈 Stats:', data);
    
    if (data.search_service === 'online') {
        searchStatus.textContent = 'Online';
        searchStatus.style.color = 'var(--accent)';
    } else {
        searchStatus.textContent = 'Offline';
        searchStatus.style.color = '#FF4444';
    }
    
    if (data.chunk_count !== undefined) {
        chunkCount.textContent = data.chunk_count.toLocaleString();
    }
});

// Question received
socket.on('question', (data) => {
    console.log('❓ Question:', data.text);
    addUserMessage(data.text);
});

// Search results (stored for answer)
let currentSources = [];
socket.on('search_results', (data) => {
    console.log('📚 Search results:', data.count, 'chunks');
    currentSources = data.chunks || [];
});

// Answer received
socket.on('answer', (data) => {
    console.log('💬 Answer:', data.text.substring(0, 50) + '...');
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
    addAssistantMessage('⚠️ Sorry, I encountered an error: ' + data.message);
    isListening = false;
    micButton.classList.remove('listening');
    hideStatus();
    sendBtn.disabled = false;
});

socket.on('session_complete', () => {
    hideStatus();
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

// Initialize
console.log('🚗 Car Dashboard UI initialized');
console.log('🎤 Click microphone or type to start conversation');

// ── Telemetry Dashboard (VSS model) ──────────────────────────────────────────

const ARC_C_MAIN = 408.41;  // circumference at r=65
const ARC_C_MINI = 238.76;  // circumference at r=38

function updateArc(id, value, max, C) {
    const arcLen = C * (240 / 360);
    const filled = arcLen * Math.max(0, Math.min(1, value / max));
    const el = document.getElementById(id);
    if (el) el.style.strokeDasharray = `${filled.toFixed(2)} ${(C - filled).toFixed(2)}`;
}

function updateBar(id, pct) {
    const el = document.getElementById(id);
    if (el) el.setAttribute('width', Math.max(0, Math.min(100, pct)).toFixed(1));
}

function applyColor(id, color) {
    const el = document.getElementById(id);
    if (!el) return;
    const tag = (el.tagName || '').toLowerCase();
    if (tag === 'circle') {
        el.style.stroke = color;
    } else if (tag === 'rect') {
        el.style.fill = color;
    } else {
        el.style.fill  = color;
        el.style.color = color;
    }
}

function statusColor(status) {
    return { normal: '#00ED64', warning: '#F5A623', critical: '#FF4444' }[status] || '#00ED64';
}

// Compute status from raw VSS values
function coolantStatus(c)   { return c > 110 ? 'critical' : c > 95  ? 'warning' : 'normal'; }
function battSocStatus(s)   { return s < 15  ? 'critical' : s < 25  ? 'warning' : 'normal'; }
function battHealthStatus(h){ return h < 70  ? 'critical' : h < 80  ? 'warning' : 'normal'; }
// Tire thresholds in kPa: normal 193–241 kPa (~28–35 psi)
function tireKpaStatus(p)   { return p < 193 ? 'critical' : (p < 207 || p > 241) ? 'warning' : 'normal'; }

function updateDashboard(data) {
    if (!data) return;
    const pt  = data.powertrain || {};
    const bat = data.battery    || {};
    const ch  = data.chassis    || {};

    // RPM gauge
    if (pt.engineRpm != null) {
        updateArc('rpm-arc', pt.engineRpm, 7000, ARC_C_MAIN);
        const el = document.getElementById('rpm-num');
        if (el) el.textContent = Math.round(pt.engineRpm).toLocaleString();
        applyColor('rpm-arc', '#00ED64');
    }

    // Engine ON/OFF badge — field: ignitionOn
    if (pt.ignitionOn != null) {
        const el = document.getElementById('engine-on-badge');
        if (el) {
            el.textContent = pt.ignitionOn ? 'ENGINE ON' : 'ENGINE OFF';
            el.style.color = pt.ignitionOn ? '#00ED64' : '#888';
        }
    }

    // Coolant
    if (pt.coolantTempC != null) {
        const st = coolantStatus(pt.coolantTempC);
        const valEl = document.getElementById('coolant-val');
        const barEl = document.getElementById('coolant-bar');
        if (valEl) { valEl.textContent = `${pt.coolantTempC.toFixed(0)}°C`; applyColor('coolant-val', statusColor(st)); }
        if (barEl) { updateBar('coolant-bar', pt.coolantTempC / 115 * 100); applyColor('coolant-bar', statusColor(st)); }
    }

    // Throttle
    if (pt.throttlePct != null) {
        const valEl = document.getElementById('throttle-val');
        const barEl = document.getElementById('throttle-bar');
        if (valEl) valEl.textContent = `${pt.throttlePct.toFixed(0)}%`;
        if (barEl) updateBar('throttle-bar', pt.throttlePct);
    }

    // Odometer
    if (pt.odometerKm != null) {
        const el = document.getElementById('odometer-val');
        if (el) el.textContent = `${Math.round(pt.odometerKm).toLocaleString()} km`;
    }

    // Battery SoC arc
    if (bat.socPct != null) {
        const st = battSocStatus(bat.socPct);
        updateArc('batt-arc', bat.socPct, 100, ARC_C_MINI);
        const el = document.getElementById('batt-pct');
        if (el) el.textContent = bat.socPct.toFixed(0);
        applyColor('batt-arc', statusColor(st));
    }
    if (bat.voltageV != null) {
        const el = document.getElementById('batt-v');
        if (el) el.textContent = `${bat.voltageV.toFixed(1)}V`;
    }
    if (bat.sohPct != null) {
        const st = battHealthStatus(bat.sohPct);
        const el = document.getElementById('batt-health');
        if (el) { el.textContent = `${bat.sohPct.toFixed(0)}%`; applyColor('batt-health', statusColor(st)); }
    }
    if (bat.estimatedRangeKm != null) {
        const el = document.getElementById('batt-range');
        if (el) el.textContent = `${Math.round(bat.estimatedRangeKm)} km`;
    }
    // chargingState is a string: "charging" | "not_charging" | etc.
    if (bat.chargingState != null) {
        const el = document.getElementById('batt-charging');
        if (el) {
            const charging = bat.chargingState === 'charging';
            el.textContent = charging ? 'YES' : 'NO';
            el.style.color = charging ? '#00ED64' : '#888';
        }
    }

    // Fuel gauge
    if (pt.fuelLevelPct != null) {
        updateArc('fuel-arc', pt.fuelLevelPct, 100, ARC_C_MAIN);
        const el = document.getElementById('fuel-num');
        if (el) el.textContent = pt.fuelLevelPct.toFixed(0);
        const st = pt.fuelLevelPct < 10 ? 'critical' : pt.fuelLevelPct < 20 ? 'warning' : 'normal';
        applyColor('fuel-arc', statusColor(st));
    }

    // Gear — field: gear (number)
    if (pt.gear != null) {
        const el = document.getElementById('current-gear');
        if (el) el.textContent = pt.gear;
    }

    // Speed — field: speedKph
    if (pt.speedKph != null) {
        const el = document.getElementById('speed-val');
        if (el) el.textContent = `${pt.speedKph.toFixed(0)} km/h`;
    }

    // Tires — fields: tirePressureFlKpa / FrKpa / RlKpa / RrKpa (kPa)
    const tireMap = {
        tirePressureFlKpa: 'fl',
        tirePressureFrKpa: 'fr',
        tirePressureRlKpa: 'rl',
        tirePressureRrKpa: 'rr',
    };
    Object.entries(tireMap).forEach(([field, abbr]) => {
        const kpa = ch[field];
        if (kpa == null) return;
        const psi = kpa * 0.14504;  // display in PSI for readability
        const c = statusColor(tireKpaStatus(kpa));
        const valEl  = document.getElementById(`tire-${abbr}-val`);
        const rectEl = document.getElementById(`tire-${abbr}-rect`);
        if (valEl)  { valEl.textContent = psi.toFixed(1); valEl.style.fill = c; }
        if (rectEl) { rectEl.style.stroke = c; }
    });

    // Brake pedal — field: brakePedalPct
    if (ch.brakePedalPct != null) {
        const valEl = document.getElementById('brake-pedal-val');
        const barEl = document.getElementById('brake-pedal-bar');
        if (valEl) valEl.textContent = `${ch.brakePedalPct.toFixed(0)}%`;
        if (barEl) updateBar('brake-pedal-bar', ch.brakePedalPct);
    }

    // ABS — field: absActive
    if (ch.absActive != null) {
        const el = document.getElementById('abs-val');
        if (el) { el.textContent = ch.absActive ? 'ACTIVE' : 'OFF'; el.style.color = ch.absActive ? '#F5A623' : '#00ED64'; }
    }
    // Traction control — field: tractionControlActive (replaces escActive)
    if (ch.tractionControlActive != null) {
        const el = document.getElementById('esc-val');
        if (el) { el.textContent = ch.tractionControlActive ? 'ACTIVE' : 'OFF'; el.style.color = ch.tractionControlActive ? '#F5A623' : '#00ED64'; }
    }

    // ADAS — fields: cruiseEnabled, cruiseSetSpeedKph, laneKeepAssistOn, collisionWarningActive
    const adas = data.adas || {};
    if (adas.cruiseEnabled != null) {
        const el = document.getElementById('adas-cruise');
        if (el) {
            el.textContent = adas.cruiseEnabled
                ? `ON${adas.cruiseSetSpeedKph ? ' ' + adas.cruiseSetSpeedKph.toFixed(0) + ' km/h' : ''}`
                : 'OFF';
            el.style.color = adas.cruiseEnabled ? '#00ED64' : '#888';
        }
    }
    if (adas.laneKeepAssistOn != null) {
        const el = document.getElementById('adas-lka');
        if (el) { el.textContent = adas.laneKeepAssistOn ? 'ON' : 'OFF'; el.style.color = adas.laneKeepAssistOn ? '#00ED64' : '#888'; }
    }
    if (adas.collisionWarningActive != null) {
        const collEl  = document.getElementById('adas-collision');
        const badgeEl = document.getElementById('collision-badge');
        const warn = adas.collisionWarningActive;
        if (collEl)  { collEl.textContent = warn ? '⚠ ALERT' : 'CLEAR'; collEl.style.color = warn ? '#FF4444' : '#00ED64'; }
        if (badgeEl) { badgeEl.textContent = warn ? '⚠ COLLISION' : '✓ OK'; badgeEl.className = `alert-pill${warn ? ' critical' : ''}`; }
    }
}

async function fetchTelemetry() {
    try {
        const resp = await fetch(`/api/vss/latest?t=${Date.now()}`);
        if (!resp.ok) return;
        const data = await resp.json();
        if (data.error) return;
        updateDashboard(data);
    } catch (_) {}
}

fetchTelemetry();
setInterval(fetchTelemetry, 3000);

// ── Network mode toggle ────────────────────────────────────────────────────────

let networkMode = 'offline';
const networkToggle = document.getElementById('networkToggle');
const networkLabel  = document.getElementById('networkLabel');

function setNetworkMode(mode) {
    networkMode = mode;
    if (mode === 'online') {
        networkLabel.textContent = 'ONLINE';
        networkToggle.classList.add('online');
    } else {
        networkLabel.textContent = 'OFFLINE';
        networkToggle.classList.remove('online');
    }
}

networkToggle.addEventListener('click', () => {
    const next = networkMode === 'offline' ? 'online' : 'offline';
    socket.emit('set_network_mode', { mode: next });
});

socket.on('network_mode_changed', (data) => {
    setNetworkMode(data.mode);
});
