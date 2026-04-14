// Car Manual Voice Assistant - Chat Interface
// Connects to Python backend and displays conversation in chat bubbles

// Initialize Socket.IO connection
const socket = io();

// State management
let isListening = false;

// DOM Elements
const micButton = document.getElementById('micButton');
const micIcon = document.getElementById('micIcon');
const chatMessages = document.getElementById('chatMessages');
const chatStatus = document.getElementById('chatStatus');
const statusText = document.getElementById('statusText');
const searchStatus = document.getElementById('searchStatus');
const chunkCount = document.getElementById('chunkCount');

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

// Helper: Add assistant message to chat
function addAssistantMessage(text, sources = null) {
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
    
    messageDiv.innerHTML = `
        <div class="message-avatar">🤖</div>
        <div class="message-content">
            <div class="message-text">${escapeHtml(text)}${sourcesHtml}</div>
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
    addAssistantMessage(data.text, currentSources);
    currentSources = []; // Clear sources after use
});

// Error handling
socket.on('error', (data) => {
    console.error('❌ Error:', data.message);
    addAssistantMessage('⚠️ Sorry, I encountered an error: ' + data.message);
    isListening = false;
    micButton.classList.remove('listening');
    hideStatus();
});

// Session complete (conversation ended)
socket.on('session_complete', () => {
    console.log('✅ Session complete');
    hideStatus();
    isListening = false;
    micButton.classList.remove('listening');
});

// Microphone button click handler
micButton.addEventListener('click', () => {
    if (isListening) {
        // Stop listening
        console.log('🛑 Stopping conversation...');
        socket.emit('stop_listening');
        isListening = false;
        micButton.classList.remove('listening');
        hideStatus();
    } else {
        // Start listening
        console.log('🎤 Starting voice assistant...');
        console.log('⚙️  Initializing speech recognition, LLM, and TTS...');
        socket.emit('start_listening');
        isListening = true;
        micButton.classList.add('listening');
    }
});

// Initialize
console.log('🚗 Car Dashboard UI initialized');
console.log('🎤 Click microphone to start conversation');
