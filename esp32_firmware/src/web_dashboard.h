#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Beee - Cockpit Autonome</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-dark: #060814;
            --bg-card: rgba(13, 17, 39, 0.65);
            --border-card: rgba(0, 240, 255, 0.15);
            --neon-cyan: #00f0ff;
            --neon-magenta: #ff007f;
            --neon-yellow: #ffdf00;
            --neon-red: #ff3b30;
            --text-main: #f0f4f8;
            --text-muted: #8a9fc2;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background-color: var(--bg-dark);
            color: var(--text-main);
            font-family: 'Outfit', sans-serif;
            overflow-x: hidden;
            min-height: 100vh;
            padding: 20px;
            position: relative;
        }
        .cyber-grid {
            position: absolute;
            top: 0; left: 0; width: 100%; height: 100%;
            background-image: 
                linear-gradient(rgba(0, 240, 255, 0.02) 1px, transparent 1px),
                linear-gradient(90deg, rgba(0, 240, 255, 0.02) 1px, transparent 1px);
            background-size: 30px 30px;
            z-index: -1;
            pointer-events: none;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid rgba(0, 240, 255, 0.2);
            padding-bottom: 15px;
            margin-bottom: 25px;
        }
        .logo-area { display: flex; align-items: center; gap: 15px; }
        h1 { font-size: 1.5rem; font-weight: 800; letter-spacing: 2px; }
        .accent-text {
            color: var(--neon-magenta);
            font-family: 'Share Tech Mono', monospace;
            font-size: 1.1rem;
            font-weight: 400;
        }
        .pulse-dot {
            width: 12px; height: 12px; border-radius: 50%;
            background-color: var(--neon-red);
            box-shadow: 0 0 10px var(--neon-red);
            display: inline-block;
        }
        .pulse-dot.online {
            background-color: var(--neon-cyan);
            box-shadow: 0 0 10px var(--neon-cyan);
            animation: pulse 1.8s infinite;
        }
        @keyframes pulse {
            0% { transform: scale(0.9); opacity: 0.6; }
            50% { transform: scale(1.1); opacity: 1; }
            100% { transform: scale(0.9); opacity: 0.6; }
        }
        .system-status {
            font-family: 'Share Tech Mono', monospace;
            font-size: 0.9rem;
        }
        .status-value { color: var(--neon-red); }
        .status-value.online {
            color: var(--neon-cyan);
            text-shadow: 0 0 8px rgba(0, 240, 255, 0.5);
        }
        .dashboard-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
        }
        @media (max-width: 900px) {
            .dashboard-grid { grid-template-columns: 1fr; }
        }
        .glass-card {
            background: var(--bg-card);
            border: 1px solid var(--border-card);
            border-radius: 16px;
            backdrop-filter: blur(12px);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
            padding: 24px;
            transition: all 0.3s ease;
        }
        .glass-card:hover {
            border-color: rgba(0, 240, 255, 0.3);
        }
        .panel-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            border-left: 3px solid var(--neon-cyan);
            padding-left: 10px;
        }
        .panel-header h2 {
            font-size: 1.1rem;
            font-weight: 600;
            letter-spacing: 1px;
        }
        .tag {
            font-family: 'Share Tech Mono', monospace;
            font-size: 0.75rem;
            padding: 2px 8px;
            border-radius: 4px;
            background: rgba(0, 240, 255, 0.1);
            color: var(--neon-cyan);
            border: 1px solid rgba(0, 240, 255, 0.2);
        }
        .metrics-container {
            display: flex;
            flex-direction: column;
            gap: 18px;
            margin-bottom: 25px;
        }
        .metric-card { display: flex; flex-direction: column; gap: 6px; }
        .metric-info {
            display: flex;
            justify-content: space-between;
            font-size: 0.85rem;
            font-weight: 600;
        }
        .metric-name { color: var(--text-muted); }
        .progress-bar {
            height: 8px;
            background: rgba(255, 255, 255, 0.05);
            border-radius: 4px;
            overflow: hidden;
            border: 1px solid rgba(255,255,255,0.05);
        }
        .progress-fill {
            height: 100%; width: 0%;
            border-radius: 4px;
            transition: width 0.5s cubic-bezier(0.4, 0, 0.2, 1);
            box-shadow: 0 0 8px currentColor;
        }
        .fill-cyan { background-color: var(--neon-cyan); color: var(--neon-cyan); }
        .fill-magenta { background-color: var(--neon-magenta); color: var(--neon-magenta); }
        .fill-yellow { background-color: var(--neon-yellow); color: var(--neon-yellow); }
        .fill-red { background-color: var(--neon-red); color: var(--neon-red); }
        
        .mood-badge-container {
            border-top: 1px solid rgba(255, 255, 255, 0.08);
            padding-top: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
            font-size: 0.9rem;
        }
        .mood-title { color: var(--text-muted); }
        .mood-badge {
            background: var(--neon-magenta);
            color: #fff;
            padding: 3px 10px;
            border-radius: 20px;
            font-weight: 800;
            font-size: 0.8rem;
            letter-spacing: 1px;
            box-shadow: 0 0 10px rgba(255, 0, 127, 0.5);
            text-transform: uppercase;
        }
        .control-section {
            margin-bottom: 24px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.06);
            padding-bottom: 20px;
        }
        .control-section:last-child { border-bottom: none; padding-bottom: 0; }
        .control-section h3 {
            font-size: 0.85rem;
            font-weight: 600;
            color: var(--text-muted);
            letter-spacing: 1px;
            margin-bottom: 12px;
        }
        .chat-input-area { display: flex; gap: 10px; }
        #chat-input {
            flex-grow: 1;
            background: rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.1);
            color: #fff;
            padding: 8px 12px;
            border-radius: 8px;
            outline: none;
            font-family: inherit;
        }
        #chat-input:focus { border-color: var(--neon-cyan); }
        #chat-emotion {
            background: #0f132e;
            border: 1px solid rgba(255, 255, 255, 0.1);
            color: #fff;
            padding: 8px;
            border-radius: 8px;
            outline: none;
            font-family: inherit;
        }
        .btn {
            border: none; border-radius: 8px;
            padding: 8px 16px; font-weight: 600;
            cursor: pointer; transition: all 0.2s ease;
            font-family: inherit;
        }
        .btn-primary { background: var(--neon-cyan); color: #000; }
        .btn-primary:hover { box-shadow: 0 0 12px var(--neon-cyan); }
        .btn-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
        }
        .btn-outline {
            background: transparent;
            border: 1px solid rgba(0, 240, 255, 0.25);
            color: var(--neon-cyan);
        }
        .btn-outline:hover { background: rgba(0, 240, 255, 0.08); border-color: var(--neon-cyan); }
        
        .file-upload-area {
            border: 2px dashed rgba(255, 0, 127, 0.25);
            background: rgba(255, 0, 127, 0.02);
            border-radius: 12px;
            padding: 25px;
            text-align: center;
            cursor: pointer;
            color: var(--text-muted);
            font-size: 0.85rem;
        }
        .file-upload-area:hover, .file-upload-area.highlight {
            border-color: var(--neon-magenta);
            background: rgba(255, 0, 127, 0.05);
            color: #fff;
        }
        .upload-status { margin-top: 10px; font-size: 0.8rem; font-weight: 600; }
        .upload-status.success { color: var(--neon-cyan); }
        .upload-status.error { color: var(--neon-red); }
    </style>
    <!-- Include Gifuct-js for client-side GIF decoding -->
    <script type="module">
        import { parseGIF, decompressFrames } from 'https://cdn.jsdelivr.net/npm/gifuct-js@2.1.2/+esm';
        window.parseGIF = parseGIF;
        window.decompressFrames = decompressFrames;
    </script>
</head>
<body>
    <div class="cyber-grid"></div>
    <div class="container">
        <header>
            <div class="logo-area">
                <span class="pulse-dot" id="status-dot"></span>
                <h1>BEEE <span class="accent-text">// INTERFACE PHYSIQUE</span></h1>
            </div>
            <div class="system-status">
                <span class="status-value" id="status-text">DECONNECTE</span>
            </div>
        </header>

        <div class="dashboard-grid">
            <!-- Left Panel: State Metrics -->
            <div class="panel glass-card">
                <div class="panel-header">
                    <h2>STATUT DE BEEE</h2>
                    <span class="tag">NEURAL</span>
                </div>
                
                <div class="metrics-container">
                    <div class="metric-card">
                        <div class="metric-info">
                            <span class="metric-name">ENERGIE</span>
                            <span class="metric-value" id="val-energy">0%</span>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill fill-cyan" id="bar-energy"></div>
                        </div>
                    </div>
                    
                    <div class="metric-card">
                        <div class="metric-info">
                            <span class="metric-name">AFFECTION</span>
                            <span class="metric-value" id="val-affection">0%</span>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill fill-magenta" id="bar-affection"></div>
                        </div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-info">
                            <span class="metric-name">CONFIANCE</span>
                            <span class="metric-value" id="val-trust">0%</span>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill fill-cyan" id="bar-trust"></div>
                        </div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-info">
                            <span class="metric-name">ENNUI</span>
                            <span class="metric-value" id="val-boredom">0%</span>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill fill-yellow" id="bar-boredom"></div>
                        </div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-info">
                            <span class="metric-name">CHAOS</span>
                            <span class="metric-value" id="val-chaos">0%</span>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill fill-red" id="bar-chaos"></div>
                        </div>
                    </div>
                </div>

                <div class="mood-badge-container">
                    <span class="mood-title">HUMEUR :</span>
                    <span class="mood-badge" id="mood-val">IDLE</span>
                </div>
            </div>

            <!-- Right Panel: Commands and GIF Streaming -->
            <div class="panel glass-card">
                <div class="panel-header">
                    <h2>PANEL DE CONTROLE</h2>
                </div>

                <!-- Speech command -->
                <div class="control-section">
                    <h3>PARLER A BEEE</h3>
                    <div class="chat-input-area">
                        <input type="text" id="chat-input" placeholder="Dire quelque chose à l'IA..." />
                        <select id="chat-emotion">
                            <option value="idle">Neutre</option>
                            <option value="happy">Joyeux</option>
                            <option value="sad">Triste</option>
                            <option value="angry">Fâché</option>
                            <option value="sarcastic">Sarcastique</option>
                            <option value="thinking">Pensif</option>
                        </select>
                        <button id="btn-send-chat" class="btn btn-primary">ENVOYER</button>
                    </div>
                </div>

                <!-- Simulation Events -->
                <div class="control-section">
                    <h3>EVENEMENTS SIMULES</h3>
                    <div class="btn-grid">
                        <button class="btn btn-outline" onclick="triggerEvent('build_success')">Compilation OK</button>
                        <button class="btn btn-outline" onclick="triggerEvent('build_failed')">Compilation Echec</button>
                        <button class="btn btn-outline" onclick="triggerEvent('touch')">Caresser</button>
                        <button class="btn btn-outline" onclick="triggerEvent('ignored')">Laisser Seul</button>
                    </div>
                </div>

                <!-- Drag Drop Streamer -->
                <div class="control-section">
                    <h3>DIFFUSER UN MEME / GIF (172x320)</h3>
                    <div class="file-upload-area" id="drop-zone">
                        <p>Déposez un GIF ou une Image ici pour le diffuser en direct</p>
                        <input type="file" id="file-input" accept="image/gif, image/png, image/jpeg" style="display: none;" />
                        <div id="upload-status" class="upload-status"></div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        let ws = null;
        const statusDot = document.getElementById("status-dot");
        const statusText = document.getElementById("status-text");
        const moodVal = document.getElementById("mood-val");
        
        const metrics = {
            energy: { bar: document.getElementById("bar-energy"), val: document.getElementById("val-energy") },
            affection: { bar: document.getElementById("bar-affection"), val: document.getElementById("val-affection") },
            trust: { bar: document.getElementById("bar-trust"), val: document.getElementById("val-trust") },
            boredom: { bar: document.getElementById("bar-boredom"), val: document.getElementById("val-boredom") },
            chaos: { bar: document.getElementById("bar-chaos"), val: document.getElementById("val-chaos") }
        };

        // Open WS connection directly to the ESP32
        function connect() {
            // Port 81 is where our ESP32 WebSocket server is listening
            const wsUrl = `ws://${window.location.hostname}:81/`;
            console.log(`Connecting to ESP32 WebSocket: ${wsUrl}`);
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = () => {
                console.log("WebSocket connected.");
                statusDot.classList.add("online");
                statusText.innerText = "SERVEUR APPAREIL ONLINE";
                statusText.className = "status-value online";
                
                // Fetch initial status via REST endpoint
                fetchStatus();
            };
            
            ws.onclose = () => {
                console.log("WebSocket disconnected. Reconnecting...");
                statusDot.classList.remove("online");
                statusText.innerText = "DECONNECTE";
                statusText.className = "status-value";
                setTimeout(connect, 3000);
            };
            
            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data.type === "status") {
                        updateDashboard(data);
                    }
                } catch(e) {
                    console.log("Error reading WS message:", e);
                }
            };
        }

        async function fetchStatus() {
            try {
                const res = await fetch('/api/status');
                if (res.ok) {
                    const data = await res.json();
                    updateDashboard({ metrics: data.metrics, mood: data.mood });
                }
            } catch(e) {
                console.error("Failed to fetch initial status", e);
            }
        }

        function updateDashboard(data) {
            if (data.metrics) {
                for (const [key, value] of Object.entries(data.metrics)) {
                    if (metrics[key]) {
                        metrics[key].bar.style.width = `${value}%`;
                        metrics[key].val.innerText = `${value}%`;
                    }
                }
            }
            if (data.mood) {
                moodVal.innerText = data.mood;
            }
        }

        // Send chat text command directly to ESP32 REST API
        async function sendChat() {
            const chatInput = document.getElementById("chat-input");
            const chatEmotion = document.getElementById("chat-emotion");
            const text = chatInput.value.trim();
            const emotion = chatEmotion.value;

            if (!text) return;

            try {
                const response = await fetch("/api/emotion", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ emotion: emotion, text: text })
                });
                if (response.ok) {
                    chatInput.value = "";
                    fetchStatus();
                }
            } catch (e) {
                console.error(e);
            }
        }

        // Trigger simulation event directly to ESP32 REST API
        async function triggerEvent(eventName) {
            try {
                const response = await fetch("/api/trigger", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ event: eventName })
                });
                if (response.ok) {
                    fetchStatus();
                }
            } catch (e) {
                console.error(e);
            }
        }

        // Decode uploaded GIF client-side and stream bands over WS
        async function uploadMedia(file) {
            const statusDiv = document.getElementById("upload-status");
            statusDiv.className = "upload-status";
            statusDiv.innerText = "Traitement du fichier...";

            if (file.type === "image/gif") {
                const reader = new FileReader();
                reader.onload = async function(e) {
                    try {
                        const buffer = e.target.result;
                        const gif = window.parseGIF(buffer);
                        const frames = window.decompressFrames(gif, true);
                        
                        statusDiv.className = "upload-status success";
                        statusDiv.innerText = `Décodage réussi ! Diffusion de ${frames.length} frames...`;
                        
                        await streamGifFrames(frames);
                        statusDiv.innerText = "Diffusion terminée.";
                    } catch(err) {
                        statusDiv.className = "upload-status error";
                        statusDiv.innerText = "Erreur de décodage du GIF.";
                        console.error(err);
                    }
                };
                reader.readAsArrayBuffer(file);
            } else {
                // Static image support
                const img = new Image();
                img.onload = async function() {
                    const canvas = document.createElement("canvas");
                    canvas.width = 172;
                    canvas.height = 160;
                    const ctx = canvas.getContext("2d");
                    ctx.drawImage(img, 0, 0, 172, 160);
                    
                    const pixels = ctx.getImageData(0, 0, 172, 160).data;
                    statusDiv.className = "upload-status success";
                    statusDiv.innerText = "Image convertie. Envoi...";
                    
                    await sendSingleRGB565Frame(pixels);
                    statusDiv.innerText = "Image affichée !";
                };
                img.src = URL.createObjectURL(file);
            }
        }

        async function sendSingleRGB565Frame(pixels) {
            const bandSize = 11008; // 172 * 32 * 2 bytes
            for (let b = 0; b < 5; b++) {
                const packet = new Uint8Array(2 + bandSize);
                packet[0] = b >> 8;
                packet[1] = b & 0xFF;

                let pOffset = 2;
                let startPixel = b * 172 * 32;
                for (let p = 0; p < 172 * 32; p++) {
                    let idx = (startPixel + p) * 4;
                    let r = pixels[idx];
                    let g = pixels[idx + 1];
                    let bl = pixels[idx + 2];

                    let r5 = (r >> 3) & 0x1F;
                    let g6 = (g >> 2) & 0x3F;
                    let b5 = (bl >> 3) & 0x1F;

                    let val = (r5 << 11) | (g6 << 5) | b5;
                    packet[pOffset++] = val & 0xFF;
                    packet[pOffset++] = (val >> 8) & 0xFF;
                }
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(packet);
                    // Small delay to prevent network buffer overflow on ESP32
                    await new Promise(r => setTimeout(r, 4));
                }
            }
        }

        async function streamGifFrames(frames) {
            const canvas = document.createElement("canvas");
            canvas.width = 172;
            canvas.height = 160;
            const ctx = canvas.getContext("2d");

            for (let f = 0; f < frames.length; f++) {
                const frame = frames[f];
                
                // Create frame canvas
                const frameCanvas = document.createElement("canvas");
                frameCanvas.width = frame.dims.width;
                frameCanvas.height = frame.dims.height;
                const frameCtx = frameCanvas.getContext("2d");
                
                const imgData = frameCtx.createImageData(frame.dims.width, frame.dims.height);
                imgData.data.set(frame.patch);
                frameCtx.putImageData(imgData, 0, 0);

                // Draw center-cropped & resized
                ctx.fillStyle = "#000000";
                ctx.fillRect(0, 0, 172, 160);

                let targetW = 172, targetH = 160;
                let targetAspect = targetW / targetH;
                let origW = frame.dims.width, origH = frame.dims.height;
                let origAspect = origW / origH;

                let drawW, drawH, drawX, drawY;
                if (origAspect > targetAspect) {
                    drawH = targetH;
                    drawW = origW * (targetH / origH);
                    drawX = (targetW - drawW) / 2;
                    drawY = 0;
                } else {
                    drawW = targetW;
                    drawH = origH * (targetW / origW);
                    drawX = 0;
                    drawY = (targetH - drawH) / 2;
                }

                ctx.drawImage(frameCanvas, drawX, drawY, drawW, drawH);
                const pixels = ctx.getImageData(0, 0, 172, 160).data;

                // Compensation de timing : on retranche le temps deja passe a envoyer
                // la frame (bandes + dessin) du delai du GIF -> lecture a la vraie
                // vitesse au lieu d'un ralenti cumulatif.
                const tStart = performance.now();
                await sendSingleRGB565Frame(pixels);
                const spent = performance.now() - tStart;
                const wait = Math.max(0, (frame.delay || 100) - spent);
                await new Promise(r => setTimeout(r, wait));
            }
        }

        // Drag Drop triggers
        const dropZone = document.getElementById("drop-zone");
        const fileInput = document.getElementById("file-input");

        dropZone.addEventListener("click", () => fileInput.click());
        fileInput.addEventListener("change", (e) => {
            if (e.target.files.length > 0) uploadMedia(e.target.files[0]);
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
            if (dt.files.length > 0) uploadMedia(dt.files[0]);
        });

        document.getElementById("btn-send-chat").addEventListener("click", sendChat);
        document.getElementById("chat-input").addEventListener("keypress", (e) => {
            if (e.key === "Enter") sendChat();
        });

        connect();
    </script>
</body>
</html>
)rawhtml";

#endif // WEB_DASHBOARD_H
