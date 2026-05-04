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
    'search_car_manual_atlas':     { icon: '🍃', label: 'MongoDB Atlas Search' },
    'navigate_to':            { icon: '🗺️', label: 'Navigation' },
    'get_latest_telemetry':   { icon: '📡', label: 'Telemetry' },
    'check_system_status':    { icon: '🔧', label: 'System Check' },
    'get_tire_pressure':      { icon: '🔧', label: 'Tire Pressure' },
    'get_anomalies':          { icon: '⚠️', label: 'Anomaly Scan' },
    'query_telemetry_history':{ icon: '📈', label: 'History' },
    'get_telemetry_stats':    { icon: '📊', label: 'Stats' },
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

// ── Telemetry Dashboard ───────────────────────────────────────────────────────

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
    if (el) el.style.width = `${Math.max(0, Math.min(100, pct)).toFixed(1)}`;
}

function applyStatus(id, status) {
    const c = { normal: '#00ED64', warning: '#F5A623', critical: '#FF4444' }[status] || '#00ED64';
    const el = document.getElementById(id);
    if (!el) return;
    const tag = (el.tagName || '').toLowerCase();
    if (tag === 'circle') {
        el.style.stroke = c;          // arc rings: stroke only, never fill
    } else if (tag === 'rect') {
        el.style.fill = c;            // bar rects: fill only
    } else {
        el.style.fill  = c;           // text / html elements
        el.style.color = c;
    }
}

function setMetric(valId, barId, sensor, fmtFn, pctFn) {
    if (!sensor) return;
    const valEl = document.getElementById(valId);
    const barEl = document.getElementById(barId);
    if (valEl) { valEl.textContent = fmtFn(sensor.value); applyStatus(valId, sensor.status); }
    if (barEl) { updateBar(barId, pctFn(sensor.value));   applyStatus(barId, sensor.status); }
}

function updateDashboard(data) {
    if (!data || !data.telemetry_batch) return;
    const eng  = data.telemetry_batch.engine       || {};
    const fuel = data.telemetry_batch.fuel         || {};
    const bat  = data.telemetry_batch.battery      || {};
    const tires = data.telemetry_batch.tires       || {};
    const trans = data.telemetry_batch.transmission || {};
    const brk  = data.telemetry_batch.brakes       || {};

    // RPM
    if (eng.rpm) {
        updateArc('rpm-arc', eng.rpm.value, 7000, ARC_C_MAIN);
        const el = document.getElementById('rpm-num');
        if (el) el.textContent = Math.round(eng.rpm.value).toLocaleString();
        applyStatus('rpm-arc', eng.rpm.status);
    }

    // Driving mode
    const modeEl = document.getElementById('driving-mode');
    if (modeEl && data.driving_mode) modeEl.textContent = data.driving_mode.toUpperCase();

    // Anomaly badge
    const badgeEl = document.getElementById('anomaly-badge');
    if (badgeEl) {
        const n = data.anomaly_count || 0;
        badgeEl.textContent = n > 0 ? `⚠ ${n}` : '✓ OK';
        badgeEl.className = `alert-pill${n > 3 ? ' critical' : n > 0 ? ' warning' : ''}`;
    }

    // Engine metrics
    setMetric('coolant-val', 'coolant-bar', eng.coolant_temp,  v => `${v.toFixed(0)}°C`,  v => v / 115 * 100);
    setMetric('oilp-val',    'oilp-bar',    eng.oil_pressure,  v => `${v.toFixed(0)} psi`, v => v / 80  * 100);
    setMetric('oill-val',    'oill-bar',    eng.oil_level,     v => `${v.toFixed(0)}%`,    v => v);

    // Battery SoC
    if (bat.state_of_charge) {
        updateArc('batt-arc', bat.state_of_charge.value, 100, ARC_C_MINI);
        const el = document.getElementById('batt-pct');
        if (el) el.textContent = bat.state_of_charge.value.toFixed(0);
        applyStatus('batt-arc', bat.state_of_charge.status);
    }
    if (bat.voltage) {
        const el = document.getElementById('batt-v');
        if (el) el.textContent = `${bat.voltage.value.toFixed(1)}V`;
    }
    if (bat.health) {
        const el = document.getElementById('batt-health');
        if (el) { el.textContent = `${bat.health.value.toFixed(0)}%`; applyStatus('batt-health', bat.health.status); }
    }

    // Fuel
    if (fuel.level) {
        updateArc('fuel-arc', fuel.level.value, 100, ARC_C_MAIN);
        const el = document.getElementById('fuel-num');
        if (el) el.textContent = fuel.level.value.toFixed(0);
        applyStatus('fuel-arc', fuel.level.status);
    }

    // Gear + transmission temp
    if (trans.gear) {
        const el = document.getElementById('current-gear');
        if (el) el.textContent = trans.gear.value;
    }
    if (trans.oil_temp) {
        const el = document.getElementById('trans-temp');
        if (el) { el.textContent = `${trans.oil_temp.value.toFixed(0)}°C`; applyStatus('trans-temp', trans.oil_temp.status); }
    }

    // Tires
    const tireMap = { front_left: 'fl', front_right: 'fr', rear_left: 'rl', rear_right: 'rr' };
    Object.entries(tireMap).forEach(([key, abbr]) => {
        const tire = tires[key];
        if (!tire) return;
        const valEl  = document.getElementById(`tire-${abbr}-val`);
        const rectEl = document.getElementById(`tire-${abbr}-rect`);
        const c = { normal: '#00ED64', warning: '#F5A623', critical: '#FF4444' }[tire.status] || '#00ED64';
        if (valEl)  { valEl.textContent = tire.pressure.toFixed(1); valEl.style.fill = c; }
        if (rectEl) { rectEl.style.stroke = c; }
    });

    // Brakes
    setMetric('brake-fluid-val', 'brake-fluid-bar', brk.fluid_level,    v => `${v.toFixed(0)}%`, v => v);
    setMetric('brake-f-val',     'brake-f-bar',     brk.pad_wear_front, v => `${v.toFixed(0)}%`, v => v);
    setMetric('brake-r-val',     'brake-r-bar',     brk.pad_wear_rear,  v => `${v.toFixed(0)}%`, v => v);
}

async function fetchTelemetry() {
    try {
        const resp = await fetch(`/api/telemetry/latest?t=${Date.now()}`);
        if (!resp.ok) return;
        updateDashboard(await resp.json());
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
