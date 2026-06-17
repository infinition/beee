// Dashboard websocket client and control logic
let socket = null;
const statusDot = document.getElementById("status-dot");
const statusText = document.getElementById("status-text");
const speechContent = document.getElementById("speech-content");
const virtualFace = document.getElementById("virtual-face");

// Metric DOM mappings
const metrics = {
    energy: { bar: document.getElementById("bar-energy"), val: document.getElementById("val-energy") },
    affection: { bar: document.getElementById("bar-affection"), val: document.getElementById("val-affection") },
    trust: { bar: document.getElementById("bar-trust"), val: document.getElementById("val-trust") },
    boredom: { bar: document.getElementById("bar-boredom"), val: document.getElementById("val-boredom") },
    chaos: { bar: document.getElementById("bar-chaos"), val: document.getElementById("val-chaos") }
};
const moodVal = document.getElementById("mood-val");

// Initialize WebSocket connection
function connectWebSocket() {
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const host = window.location.host || "localhost:8000";
    const wsUrl = `${protocol}//${host}/ws/client`;
    
    console.log(`Connecting to dashboard WebSocket: ${wsUrl}`);
    socket = new WebSocket(wsUrl);
    
    socket.onopen = () => {
        console.log("Dashboard WS Connected.");
        statusDot.classList.add("online");
        statusText.innerText = "SERVEUR CONNECTE";
        statusText.classList.add("online");
    };
    
    socket.onclose = () => {
        console.log("Dashboard WS Disconnected. Retrying...");
        statusDot.classList.remove("online");
        statusText.innerText = "SERVEUR DECONNECTE";
        statusText.classList.remove("online");
        setTimeout(connectWebSocket, 3000); // Reconnect in 3s
    };
    
    socket.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            updateDashboard(data);
        } catch (e) {
            console.error("Error parsing websocket message:", e);
        }
    };
}

// Update Dashboard UI elements from state data
function updateDashboard(state) {
    if (!state) return;
    
    // Update Metrics
    if (state.metrics) {
        for (const [key, value] of Object.entries(state.metrics)) {
            if (metrics[key]) {
                metrics[key].bar.style.width = `${value}%`;
                metrics[key].val.innerText = `${value}%`;
            }
        }
    }
    
    // Update Mood Badge
    if (state.mood) {
        moodVal.innerText = state.mood;
        
        // Update simulated face classes
        virtualFace.className = "face"; // Reset
        virtualFace.classList.add(`exp-${state.mood}`);
    }
    
    // Update Speech Content
    if (state.reaction_text) {
        speechContent.innerText = state.reaction_text;
    } else if (state.mood === "sleeping") {
        speechContent.innerText = "Zzz... Ronfle doucement...";
    } else if (state.mood === "idle") {
        speechContent.innerText = "Prêt à recevoir des ordres.";
    }
}

// API: Send speech bubble commands to backend
async function sendChat() {
    const textInput = document.getElementById("chat-input");
    const emotionSelect = document.getElementById("chat-emotion");
    
    const text = textInput.value.trim();
    const emotion = emotionSelect.value;
    
    if (!text) return;
    
    try {
        const response = await fetch("/api/emotion", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ emotion: emotion, text: text })
        });
        
        if (response.ok) {
            textInput.value = ""; // Clear input
        } else {
            console.error("API response error");
        }
    } catch (e) {
        console.error("Failed to send chat command:", e);
    }
}

// API: Trigger direct custom event reactions
async function triggerEvent(eventName) {
    try {
        await fetch("/api/trigger", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ event: eventName })
        });
    } catch (e) {
        console.error("Failed to trigger event:", e);
    }
}

// API: Upload GIF and stream it to the device
async function uploadMedia(file) {
    const statusDiv = document.getElementById("upload-status");
    statusDiv.className = "upload-status";
    statusDiv.innerText = "Conversion & Stream en cours...";
    
    const formData = new FormData();
    formData.append("file", file);
    
    try {
        const response = await fetch("/api/upload", {
            method: "POST",
            body: formData
        });
        
        if (response.ok) {
            const resData = await response.json();
            statusDiv.className = "upload-status success";
            statusDiv.innerText = `Stream lancé ! (${resData.frames} frames)`;
            
            // Set simulated face to hacker mode during stream
            virtualFace.className = "face exp-panic";
            speechContent.innerText = "Streaming média en direct sur l'écran...";
        } else {
            statusDiv.className = "upload-status error";
            statusDiv.innerText = "Erreur de chargement/traitement.";
        }
    } catch (e) {
        statusDiv.className = "upload-status error";
        statusDiv.innerText = "Connexion serveur perdue.";
        console.error(e);
    }
}

// Drag & Drop Area listeners
const dropZone = document.getElementById("drop-zone");
const fileInput = document.getElementById("file-input");

dropZone.addEventListener("click", () => fileInput.click());

fileInput.addEventListener("change", (e) => {
    if (e.target.files.length > 0) {
        uploadMedia(e.target.files[0]);
    }
});

["dragenter", "dragover"].forEach(eventName => {
    dropZone.addEventListener(eventName, (e) => {
        e.preventDefault();
        dropZone.classList.add("highlight");
    }, false);
});

["dragleave", "drop"].forEach(eventName => {
    dropZone.addEventListener(eventName, (e) => {
        e.preventDefault();
        dropZone.classList.remove("highlight");
    }, false);
});

dropZone.addEventListener("drop", (e) => {
    const dt = e.dataTransfer;
    const files = dt.files;
    if (files.length > 0) {
        uploadMedia(files[0]);
    }
});

// Setup chat submit triggers
document.getElementById("btn-send-chat").addEventListener("click", sendChat);
document.getElementById("chat-input").addEventListener("keypress", (e) => {
    if (e.key === "Enter") sendChat();
});

// Run
connectWebSocket();
