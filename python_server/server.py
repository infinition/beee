import os
import json
import asyncio
from typing import List, Set, Dict, Any
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, UploadFile, File, Form, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware

from persona import PersonaEngine
from gif_converter import process_gif_or_image

app = FastAPI(title="Beee Cyber-Companion Backend")

# Enable CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Initialize engines
persona = PersonaEngine()

# Web Sockets tracking
device_sockets: Set[WebSocket] = set()
client_sockets: Set[WebSocket] = set()

# Lock to avoid concurrent media streaming collisions
streaming_lock = asyncio.Lock()

# Auto-mount static directory if it exists
static_path = os.path.join(os.path.dirname(__file__), "static")
if not os.path.exists(static_path):
    os.makedirs(static_path)

# Broadcast utility
async def broadcast_to_clients(data: Dict[str, Any]):
    """Send state updates to all connected web dashboards"""
    if client_sockets:
        message = json.dumps(data)
        await asyncio.gather(
            *[client.send_text(message) for client in client_sockets],
            return_exceptions=True
        )

async def send_to_device(data: Dict[str, Any]):
    """Send JSON command to ESP32 device"""
    if device_sockets:
        message = json.dumps(data)
        await asyncio.gather(
            *[device.send_text(message) for device in device_sockets],
            return_exceptions=True
        )

async def stream_media_to_device(frames: List[bytes], delay_ms: int):
    """Stream binary RGB565 frames to the ESP32 at a specific FPS, sliced in 10 bands of 11KB"""
    async with streaming_lock:
        if not device_sockets:
            print("[Server] Media streaming error: No device connected")
            return
            
        print(f"[Server] Starting media stream: {len(frames)} frames @ {1000/delay_ms:.1f} FPS")
        
        # Calculate dynamic sleep to account for processing overhead
        sleep_time = delay_ms / 1000.0
        
        for idx, frame in enumerate(frames):
            if not device_sockets:
                break
                
            # Slice frame into 10 bands of 172x32 pixels (11008 bytes each)
            band_size = 11008
            for band_idx in range(10):
                offset = band_idx * band_size
                band_data = frame[offset:offset + band_size]
                
                # Prepend 2-byte band_idx header (big endian uint16)
                packet = bytes([band_idx >> 8, band_idx & 0xFF]) + band_data
                
                tasks = [device.send_bytes(packet) for device in device_sockets]
                await asyncio.gather(*tasks, return_exceptions=True)
                
            await asyncio.sleep(sleep_time)
            
        print("[Server] Media stream completed.")

# API endpoints
@app.get("/api/status")
async def get_status():
    persona.decay_state()
    return persona.get_status()

@app.post("/api/emotion")
async def set_emotion(payload: Dict[str, Any]):
    emotion = payload.get("emotion", "idle")
    text = payload.get("text", "")
    intensity = payload.get("intensity", 1.0)
    eye_x = payload.get("eye_x", 0.0)
    eye_y = payload.get("eye_y", 0.0)
    
    # Register this speak event with the persona
    interaction_res = persona.register_interaction(
        "agent_speak", 
        {"text": text, "sentiment": emotion}
    )
    
    # Send display command to ESP32
    cmd = {
        "type": "emotion",
        "emotion": emotion,
        "text": text,
        "eye_x": eye_x,
        "eye_y": eye_y
    }
    await send_to_device(cmd)
    
    # Broadcast status change to dashboards
    status = persona.get_status()
    status["reaction_text"] = text
    await broadcast_to_clients(status)
    
    return interaction_res

@app.post("/api/trigger")
async def trigger_event(payload: Dict[str, Any]):
    event_type = payload.get("event", "ignored")
    
    interaction_res = persona.register_interaction(event_type)
    
    # Send update command to device
    cmd = {
        "type": "emotion",
        "emotion": interaction_res["mood"],
        "text": interaction_res["text"]
    }
    await send_to_device(cmd)
    
    # Broadcast to dashboards
    status = persona.get_status()
    status["reaction_text"] = interaction_res["text"]
    await broadcast_to_clients(status)
    
    return interaction_res

@app.post("/api/upload")
async def upload_asset(file: UploadFile = File(...)):
    """Upload a GIF or image, convert it, and stream it to the ESP32"""
    try:
        contents = await file.read()
        frames, delay_ms = process_gif_or_image(contents)
        
        # Start background streaming task
        # This will send the frames to the ESP32 in real time
        asyncio.create_task(stream_media_to_device(frames, delay_ms))
        
        return {"status": "success", "frames": len(frames), "delay_ms": delay_ms}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Image processing failed: {str(e)}")

# WebSocket Routes
@app.websocket("/ws/device")
async def websocket_device(websocket: WebSocket):
    await websocket.accept()
    device_sockets.add(websocket)
    print(f"[Device] WebSocket connection accepted. Count: {len(device_sockets)}")
    
    try:
        while True:
            # Receive text data (JSON) from ESP32
            data_str = await websocket.receive_text()
            data = json.loads(data_str)
            
            msg_type = data.get("type")
            if msg_type == "handshake":
                print("[Device] Initial handshake completed.")
                
            elif msg_type == "touch":
                x = data.get("x", 0)
                y = data.get("y", 0)
                print(f"[Device] Touch event received: x={x}, y={y}")
                
                # Update persona based on physical touch coordinate
                res = persona.register_interaction("touch", {"x": x, "y": y})
                
                # Command device to react (expression + text reply)
                cmd = {
                    "type": "emotion",
                    "emotion": res["mood"],
                    "text": res["text"]
                }
                await websocket.send_text(json.dumps(cmd))
                
                # Update dashboard clients
                status = persona.get_status()
                status["reaction_text"] = res["text"]
                await broadcast_to_clients(status)
                
    except WebSocketDisconnect:
        device_sockets.remove(websocket)
        print(f"[Device] WebSocket disconnected. Count: {len(device_sockets)}")
    except Exception as e:
        if websocket in device_sockets:
            device_sockets.remove(websocket)
        print(f"[Device] Error on WebSocket: {e}")

@app.websocket("/ws/client")
async def websocket_client(websocket: WebSocket):
    await websocket.accept()
    client_sockets.add(websocket)
    print(f"[Client] Dashboard connected. Count: {len(client_sockets)}")
    
    # Send initial status
    await websocket.send_text(json.dumps(persona.get_status()))
    
    try:
        while True:
            # Handle incoming messages from dashboard (if any)
            await websocket.receive_text()
    except WebSocketDisconnect:
        client_sockets.remove(websocket)
        print(f"[Client] Dashboard disconnected. Count: {len(client_sockets)}")
    except Exception as e:
        if websocket in client_sockets:
            client_sockets.remove(websocket)
        print(f"[Client] Error on dashboard WebSocket: {e}")

# Mount static files (must be at the end of routes)
app.mount("/", StaticFiles(directory=static_path, html=True), name="static")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=True)
