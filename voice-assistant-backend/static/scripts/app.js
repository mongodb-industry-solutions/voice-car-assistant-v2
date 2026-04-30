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
    'search_car_manual':      { icon: '📖', label: 'Car Manual' },
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
