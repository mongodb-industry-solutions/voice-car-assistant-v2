/**
 * Telemetry Dashboard JavaScript
 * Handles real-time telemetry display and AI chat
 */

// Configuration
const TELEMETRY_SERVICE_URL = 'http://localhost:8084';
const SIMULATOR_URL = 'http://localhost:8082';
const CHAT_API_URL = '/api/telemetry/chat';
const REFRESH_INTERVAL = 2000; // 2 seconds

// State
let telemetryData = null;
let refreshTimer = null;
let isLoading = false;
let conversationId = null; // Track conversation for memory
let lastSimulationState = null; // Track simulation state to avoid log spam

// System icons
const SYSTEM_ICONS = {
  engine: '🔧',
  battery: '🔋',
  fuel: '⛽',
  tires: '⭕',
  transmission: '⚙️',
  brakes: '🛑'
};

// Initialize
document.addEventListener('DOMContentLoaded', () => {
  initTelemetry();
  initChat();
});

// === TELEMETRY FUNCTIONS ===

function initTelemetry() {
  // Set up button handlers
  document.getElementById('startButton').addEventListener('click', handleStartSimulation);
  document.getElementById('stopButton').addEventListener('click', handleStopSimulation);
  
  // Check simulation status on load
  checkSimulationStatus();
  
  // Start auto-refresh
  startRefresh();
}

async function startRefresh() {
  await fetchTelemetryData();
  refreshTimer = setInterval(fetchTelemetryData, REFRESH_INTERVAL);
  
  // Also check simulation status periodically to keep buttons in sync
  setInterval(checkSimulationStatus, 5000); // Check every 5 seconds
}

async function checkSimulationStatus() {
  try {
    const response = await fetch(`${SIMULATOR_URL}/simulator/status`);
    if (response.ok) {
      const data = await response.json();
      
      // Only log if state changed
      if (lastSimulationState !== data.running) {
        if (data.running) {
          console.log('✅ Simulation is now running');
        } else {
          console.log('⏸️ Simulation is now stopped');
        }
        lastSimulationState = data.running;
      }
      
      // Show appropriate button based on running state
      if (data.running) {
        document.getElementById('startButton').style.display = 'none';
        document.getElementById('stopButton').style.display = 'flex';
      } else {
        document.getElementById('startButton').style.display = 'flex';
        document.getElementById('stopButton').style.display = 'none';
      }
    }
  } catch (error) {
    console.error('Failed to check simulation status:', error);
    // Default to showing start button
    document.getElementById('startButton').style.display = 'flex';
    document.getElementById('stopButton').style.display = 'none';
  }
}

function stopRefresh() {
  if (refreshTimer) {
    clearInterval(refreshTimer);
    refreshTimer = null;
  }
}

async function fetchTelemetryData() {
  try {
    const response = await fetch(`${TELEMETRY_SERVICE_URL}/telemetry/latest`);
    
    if (response.ok) {
      telemetryData = await response.json();
      updateUI(telemetryData);
      updateConnectionStatus(true);
    } else if (response.status === 404) {
      // No data yet
      updateConnectionStatus(true, 'No data yet');
    } else {
      throw new Error(`HTTP ${response.status}`);
    }
  } catch (error) {
    console.error('Failed to fetch telemetry:', error);
    updateConnectionStatus(false, error.message);
  }
}

function updateUI(data) {
  if (!data) return;
  
  // Update vehicle info
  document.getElementById('vehicleInfo').style.display = 'flex';
  document.getElementById('vehicleId').textContent = data.vehicle_id || '--';
  document.getElementById('drivingMode').textContent = (data.driving_mode || '--').toUpperCase();
  document.getElementById('lastTimestamp').textContent = 
    new Date(data.timestamp).toLocaleTimeString();
  
  // Update last update time
  document.getElementById('lastUpdate').textContent = 
    `• Last: ${new Date().toLocaleTimeString()}`;
  
  // Update anomaly count
  if (data.anomaly_count > 0) {
    document.getElementById('anomalyContainer').style.display = 'flex';
    document.getElementById('anomalyCount').textContent = data.anomaly_count;
  } else {
    document.getElementById('anomalyContainer').style.display = 'none';
  }
  
  // Update systems grid
  updateSystemsGrid(data.telemetry_batch);
}

function updateSystemsGrid(telemetry_batch) {
  const grid = document.getElementById('systemsGrid');
  
  if (!telemetry_batch || Object.keys(telemetry_batch).length === 0) {
    grid.innerHTML = '<div class="no-data"><p>No telemetry data available</p></div>';
    return;
  }
  
  grid.innerHTML = '';
  
  Object.entries(telemetry_batch).forEach(([systemName, systemData]) => {
    const card = createSystemCard(systemName, systemData);
    grid.appendChild(card);
  });
}

function createSystemCard(systemName, systemData) {
  const card = document.createElement('div');
  card.className = 'system-card';
  
  const header = document.createElement('div');
  header.className = 'system-header';
  header.innerHTML = `
    <span class="system-icon">${SYSTEM_ICONS[systemName] || '📊'}</span>
    <span class="system-name">${capitalizeSystemName(systemName)}</span>
  `;
  
  const dataContainer = document.createElement('div');
  dataContainer.className = 'system-data';
  
  // Display sensor data
  Object.entries(systemData).forEach(([sensorName, sensorData]) => {
    // Handle nested objects (like tires with pressure/temp/status)
    if (typeof sensorData === 'object' && !Array.isArray(sensorData)) {
      // Check if it's a sensor reading with value/unit
      if (sensorData.value !== undefined) {
        const item = createSensorItem(sensorName, sensorData.value, sensorData.unit, sensorData.status);
        dataContainer.appendChild(item);
      }
      // Handle nested tire data (front_left has pressure, temp, status)
      else if (sensorData.pressure !== undefined || sensorData.temp !== undefined) {
        // Create a sub-header for tire position
        const subHeader = document.createElement('div');
        subHeader.className = 'sensor-subheader';
        subHeader.textContent = capitalizeName(sensorName);
        dataContainer.appendChild(subHeader);
        
        // Add each property
        Object.entries(sensorData).forEach(([key, value]) => {
          if (key !== 'unit' && key !== 'status') {
            const item = createSensorItem(key, value, sensorData.unit, sensorData.status);
            dataContainer.appendChild(item);
          }
        });
      }
      // Handle other nested objects
      else {
        Object.entries(sensorData).forEach(([key, value]) => {
          const item = createSensorItem(`${sensorName} ${key}`, value);
          dataContainer.appendChild(item);
        });
      }
    } else {
      // Simple value
      const item = createSensorItem(sensorName, sensorData);
      dataContainer.appendChild(item);
    }
  });
  
  card.appendChild(header);
  card.appendChild(dataContainer);
  
  return card;
}

function createSensorItem(name, value, unit = '', status = '') {
  const item = document.createElement('div');
  item.className = 'sensor-item';
  
  // Determine value color based on status or value
  let valueClass = 'sensor-value';
  if (status === 'warning' || status === 'critical') {
    valueClass += ' value-warning';
  } else if (status === 'normal' || status === 'ok') {
    valueClass += ' value-normal';
  }
  
  // Format the value
  let displayValue = value;
  if (typeof value === 'number') {
    displayValue = value.toFixed(2);
  }
  if (unit) {
    displayValue += ` ${unit.split('/')[0]}`; // Take first part of unit (e.g., "psi" from "psi/°C")
  }
  
  item.innerHTML = `
    <span class="sensor-name">${capitalizeName(name)}</span>
    <span class="${valueClass}">${displayValue}</span>
  `;
  
  return item;
}

function capitalizeSystemName(name) {
  // Special handling for common system names
  const specialNames = {
    'engine': 'Engine',
    'battery': 'Battery',
    'fuel': 'Fuel System',
    'tires': 'Tires',
    'transmission': 'Transmission',
    'brakes': 'Brakes'
  };
  
  return specialNames[name] || name.charAt(0).toUpperCase() + name.slice(1);
}

function capitalizeName(name) {
  return name
    .replace(/_/g, ' ')
    .split(' ')
    .map(word => word.charAt(0).toUpperCase() + word.slice(1))
    .join(' ');
}

function updateConnectionStatus(connected, message = '') {
  const dot = document.getElementById('statusDot');
  const text = document.getElementById('connectionText');
  
  if (connected) {
    dot.className = 'status-dot connected';
    text.textContent = message || 'Connected';
  } else {
    dot.className = 'status-dot disconnected';
    text.textContent = message || 'Disconnected';
  }
}

async function handleStartSimulation() {
  const button = document.getElementById('startButton');
  const icon = document.getElementById('startIcon');
  
  button.disabled = true;
  icon.textContent = '⏳';
  
  try {
    const response = await fetch(`${SIMULATOR_URL}/simulator/start`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ interval_seconds: 2 })
    });
    
    const data = await response.json();
    
    if (response.ok) {
      showStatusMessage(data.message || 'Simulation started successfully', 'success');
      button.style.display = 'none';
      document.getElementById('stopButton').style.display = 'flex';
      console.log('✅ Simulation started');
    } else {
      // Handle specific error cases
      if (data.running === true || data.message?.includes('already running')) {
        // Simulation already running - just update UI
        showStatusMessage('Simulation is already running', 'info');
        button.style.display = 'none';
        document.getElementById('stopButton').style.display = 'flex';
        console.log('ℹ️ Simulation already running');
      } else {
        throw new Error(data.message || `Failed to start simulation (${response.status})`);
      }
    }
  } catch (error) {
    showStatusMessage(`Error: ${error.message}`, 'error');
    icon.textContent = '▶️';
    console.error('❌ Start simulation error:', error);
  } finally {
    button.disabled = false;
  }
}

async function handleStopSimulation() {
  const button = document.getElementById('stopButton');
  const icon = document.getElementById('stopIcon');
  
  button.disabled = true;
  icon.textContent = '⏳';
  
  try {
    const response = await fetch(`${SIMULATOR_URL}/simulator/stop`, {
      method: 'POST'
    });
    
    const data = await response.json();
    
    if (response.ok) {
      showStatusMessage(data.message || 'Simulation stopped successfully', 'success');
      button.style.display = 'none';
      document.getElementById('startButton').style.display = 'flex';
      console.log('✅ Simulation stopped:', data.snapshots_generated, 'snapshots generated');
    } else {
      throw new Error(data.message || `Failed to stop simulation (${response.status})`);
    }
  } catch (error) {
    showStatusMessage(`Error: ${error.message}`, 'error');
    icon.textContent = '⏹️';
    console.error('❌ Stop simulation error:', error);
  } finally {
    button.disabled = false;
  }
}

function showStatusMessage(message, type) {
  const statusEl = document.getElementById('statusMessage');
  statusEl.textContent = message;
  statusEl.style.display = 'block';
  
  setTimeout(() => {
    statusEl.style.display = 'none';
  }, 3000);
}

// === CHAT FUNCTIONS ===

function initChat() {
  const form = document.getElementById('chatForm');
  const clearBtn = document.getElementById('clearButton');
  const exampleButtons = document.querySelectorAll('.example-button');
  
  form.addEventListener('submit', handleChatSubmit);
  clearBtn.addEventListener('click', handleClearChat);
  
  exampleButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      const question = btn.getAttribute('data-question');
      document.getElementById('chatInput').value = question;
    });
  });
}

async function handleChatSubmit(e) {
  e.preventDefault();
  
  const input = document.getElementById('chatInput');
  const question = input.value.trim();
  
  if (!question) return;
  
  // Add user message
  addChatMessage(question, 'user');
  input.value = '';
  
  // Show loading
  addLoadingMessage();
  
  try {
    const response = await fetch(CHAT_API_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ 
        question,
        conversation_id: conversationId // Include conversation_id for memory
      })
    });
    
    removeLoadingMessage();
    
    if (!response.ok) {
      throw new Error(`Server error: ${response.status}`);
    }
    
    const data = await response.json();
    
    // Store conversation_id for next message
    if (data.conversation_id) {
      conversationId = data.conversation_id;
      console.log('💾 Conversation ID:', conversationId);
    }
    
    addChatMessage(data.answer || 'Sorry, I couldn\'t process that question.', 'assistant', data.tools_used);
    
  } catch (error) {
    removeLoadingMessage();
    addChatMessage(`⚠️ Error: ${error.message}. Please make sure the telemetry services are running.`, 'assistant');
  }
}

function addChatMessage(content, role, toolsUsed = []) {
  const container = document.getElementById('messagesContainer');
  const message = document.createElement('div');
  message.className = `message ${role}`;
  
  const now = new Date();
  const timeStr = now.toLocaleTimeString();
  
  let toolsHTML = '';
  if (toolsUsed && toolsUsed.length > 0) {
    const badges = toolsUsed.map(tool => 
      `<span class="tool-badge">${tool}</span>`
    ).join('');
    toolsHTML = `
      <div class="tools-used">
        <span class="tools-label">🔧 Tools used:</span>
        ${badges}
      </div>
    `;
  }
  
  message.innerHTML = `
    <div class="message-header">
      <span class="message-role">${role === 'user' ? '👤 You' : '🤖 AI'}</span>
      <span class="message-time">${timeStr}</span>
    </div>
    <div class="message-content">${content}</div>
    ${toolsHTML}
  `;
  
  container.appendChild(message);
  container.scrollTop = container.scrollHeight;
}

function addLoadingMessage() {
  const container = document.getElementById('messagesContainer');
  const message = document.createElement('div');
  message.className = 'message assistant loading-message';
  message.id = 'loadingMessage';
  
  message.innerHTML = `
    <div class="message-content">
      <div class="loading-dots">
        <span>●</span>
        <span>●</span>
        <span>●</span>
      </div>
    </div>
  `;
  
  container.appendChild(message);
  container.scrollTop = container.scrollHeight;
}

function removeLoadingMessage() {
  const loading = document.getElementById('loadingMessage');
  if (loading) {
    loading.remove();
  }
}

function handleClearChat() {
  // Reset conversation memory
  conversationId = null;
  console.log('🔄 Conversation reset - new conversation will start');
  
  const container = document.getElementById('messagesContainer');
  container.innerHTML = `
    <div class="message assistant">
      <div class="message-header">
        <span class="message-role">🤖 AI</span>
        <span class="message-time">${new Date().toLocaleTimeString()}</span>
      </div>
      <div class="message-content">
        👋 Chat cleared! Ask me anything about vehicle telemetry.
      </div>
    </div>
  `;
}

// === STATS UPDATE ===

async function updateStats() {
  try {
    const response = await fetch(`${TELEMETRY_SERVICE_URL}/health`);
    if (response.ok) {
      const data = await response.json();
      document.getElementById('statsContainer').style.display = 'flex';
      document.getElementById('totalSnapshots').textContent = data.snapshot_count || 0;
    }
  } catch (error) {
    console.error('Failed to fetch stats:', error);
  }
}

// Update stats every 5 seconds
setInterval(updateStats, 5000);
updateStats();
