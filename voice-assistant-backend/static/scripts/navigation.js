// Navigation module — Leaflet map + route panel (shown inline when agent returns a route)

let map = null;
let userMarker = null;
let destMarker = null;
let routeLayer = null;
let userCoords = null;
let mapInitialized = false;

// ── Icons ─────────────────────────────────────────────────────────────────────

function makeIcon(html) {
    return L.divIcon({ className: '', html, iconSize: [32, 32], iconAnchor: [16, 28] });
}

const USER_ICON = makeIcon('<div style="font-size:2rem;line-height:1;filter:drop-shadow(0 2px 4px #000)">📍</div>');
const DEST_ICON = makeIcon('<div style="font-size:2rem;line-height:1;filter:drop-shadow(0 2px 4px #000)">🏁</div>');

// ── Map init ──────────────────────────────────────────────────────────────────

function initMap() {
    if (mapInitialized) return;
    mapInitialized = true;

    map = L.map('mapContainer', { zoomControl: true, attributionControl: true });

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>',
        maxZoom: 19,
    }).addTo(map);

    if (userCoords) {
        setUserMarker(userCoords.lat, userCoords.lon);
    } else {
        map.setView([48.8566, 2.3522], 13);
    }
}

function setUserMarker(lat, lon) {
    if (!map) return;
    if (userMarker) {
        userMarker.setLatLng([lat, lon]);
    } else {
        userMarker = L.marker([lat, lon], { icon: USER_ICON })
            .addTo(map)
            .bindPopup('Your location');
        map.setView([lat, lon], 15);
    }
}

// ── Geolocation ───────────────────────────────────────────────────────────────

function startGeolocation() {
    if (!navigator.geolocation) {
        console.warn('Geolocation not supported');
        return;
    }
    navigator.geolocation.watchPosition(
        (pos) => {
            userCoords = { lat: pos.coords.latitude, lon: pos.coords.longitude };
            socket.emit('update_location', userCoords);
            setUserMarker(userCoords.lat, userCoords.lon);
        },
        (err) => console.warn('Geolocation error:', err.message),
        { enableHighAccuracy: true, maximumAge: 30000, timeout: 10000 }
    );
}

// ── Nav panel ─────────────────────────────────────────────────────────────────

function showNavPanel() {
    const panel = document.getElementById('navPanel');
    if (!panel) return;
    panel.classList.remove('hidden');
    initMap();
    setTimeout(() => { if (map) map.invalidateSize(); }, 300);
}

function closeNavPanel() {
    const panel = document.getElementById('navPanel');
    if (panel) panel.classList.add('hidden');
}

// ── Route display ─────────────────────────────────────────────────────────────

function displayRoute(navData) {
    if (!map) return;

    const { destination, route } = navData;

    if (destMarker) { map.removeLayer(destMarker); destMarker = null; }
    if (routeLayer) { map.removeLayer(routeLayer); routeLayer = null; }

    const title = document.getElementById('navPanelTitle');
    if (title) title.textContent = destination.name;

    destMarker = L.marker([destination.lat, destination.lon], { icon: DEST_ICON })
        .addTo(map)
        .bindPopup(`<b>${destination.name}</b>${destination.address ? '<br>' + destination.address : ''}`)
        .openPopup();

    routeLayer = L.geoJSON(route.geometry, {
        style: { color: '#00ED64', weight: 5, opacity: 0.85, lineCap: 'round', lineJoin: 'round' },
    }).addTo(map);

    map.fitBounds(routeLayer.getBounds(), { padding: [40, 40] });
}

function renderSteps(navData) {
    const { destination, route } = navData;

    const title = document.getElementById('navPanelTitle');
    if (title) title.textContent = destination.name;

    const panel = document.getElementById('navSteps');
    if (!panel) return;

    const stepsHtml = route.steps.map((s, i) => `
        <div class="nav-step">
            <span class="step-num">${i + 1}</span>
            <span class="step-text">${escapeHtml(s.instruction)}</span>
            <span class="step-dist">${s.distance_text}</span>
        </div>
    `).join('');

    panel.innerHTML = `
        <div class="nav-summary">
            <div class="nav-dest-row">
                <span class="nav-dest-icon">📍</span>
                <span class="nav-dest-name">${escapeHtml(destination.name)}</span>
            </div>
            <div class="nav-meta">
                <span>${route.distance_text}</span>
                <span class="nav-meta-sep">·</span>
                <span>${route.duration_text}</span>
                ${destination.address ? `<span class="nav-meta-sep">·</span><span>${escapeHtml(destination.address.split(',').slice(0, 2).join(','))}</span>` : ''}
            </div>
        </div>
        <div class="nav-steps-list">${stepsHtml}</div>
    `;
}

function renderNearbyOptions(navData) {
    const container = document.getElementById('nearbyOptions');
    if (!container || !navData.nearby_options || navData.nearby_options.length <= 1) {
        if (container) container.innerHTML = '';
        return;
    }

    const btns = navData.nearby_options.slice(1, 4).map((poi, i) => `
        <button class="nearby-btn" onclick="navigateToPoi(${i + 1})" data-idx="${i + 1}">
            <span class="nearby-name">${escapeHtml(poi.name)}</span>
            <span class="nearby-dist">${Math.round(poi.distance_m)} m</span>
        </button>
    `).join('');

    container.innerHTML = `<div class="nearby-label">Other nearby options</div>${btns}`;
    container._navData = navData;
}

function navigateToPoi(idx) {
    const container = document.getElementById('nearbyOptions');
    const navData = container && container._navData;
    if (!navData || !navData.nearby_options[idx]) return;

    const poi = navData.nearby_options[idx];
    if (!userCoords) return;

    fetch('/api/navigate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ query: poi.name, lat: userCoords.lat, lon: userCoords.lon }),
    })
        .then(r => r.json())
        .then(data => { if (data.route) displayRoute(data); })
        .catch(err => console.error('Navigation error:', err));
}

// ── SocketIO — navigation result from agent ───────────────────────────────────

socket.on('navigation_result', (navData) => {
    showNavPanel();
    setTimeout(() => {
        if (map) {
            map.invalidateSize();
            displayRoute(navData);
        }
    }, 350);
});

// ── Init ──────────────────────────────────────────────────────────────────────

startGeolocation();
