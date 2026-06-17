import asyncio
import json
import uuid
import httpx
from fastmcp import FastMCP

# Initialize FastMCP Server
mcp = FastMCP("Beee Standalone Controller")

# Target the ESP32 directly using mDNS. Falls back to last known IP if resolution fails.
PRIMARY_URL = "http://beee.local"
FALLBACK_URL = "http://192.168.1.61"

async def send_request(method: str, path: str, json_data: dict = None) -> tuple[int, str]:
    """Helper to try primary url (mDNS) first, and fallback to IP if it fails"""
    urls = [PRIMARY_URL, FALLBACK_URL]
    last_err = ""
    
    for url in urls:
        target = f"{url}{path}"
        try:
            async with httpx.AsyncClient(timeout=3.0) as client:
                if method.upper() == "POST":
                    response = await client.post(target, json=json_data)
                else:
                    response = await client.get(target)
                return response.status_code, response.text
        except Exception as e:
            last_err = str(e)
            continue
            
    return 0, f"Connection failed. Last error: {last_err}"

@mcp.tool()
async def show_emotion(emotion: str, text: str, eye_x: float = 0.0, eye_y: float = 0.0) -> str:
    """
    Update the ESP32 screen directly with a specific emotion, speech text, and optional eye gaze direction.
    
    Args:
        emotion: The target emotion/expression: 'idle', 'happy', 'sad', 'angry', 'sarcastic', 'thinking', 'sleeping', 'panic', 'hacker'.
        text: Speech/dialogue text to display in the terminal banner at the bottom (max ~60 chars recommended).
        eye_x: Horizontal eye offset direction (-1.0 for hard left, 1.0 for hard right).
        eye_y: Vertical eye offset direction (-1.0 for hard up, 1.0 for hard down).
    """
    payload = {
        "emotion": emotion.lower(),
        "text": text,
        "eye_x": eye_x,
        "eye_y": eye_y
    }
    
    status, response_text = await send_request("POST", "/api/emotion", payload)
    
    if status == 200:
        return f"Successfully updated Beee emotion directly to '{emotion}' with text: '{text}'."
    else:
        return f"Error communicating with ESP32 screen. Status code: {status}. Details: {response_text}"

@mcp.tool()
async def say_on_device(text: str, emotion: str = "idle") -> str:
    """
    Make the companion speak a sentence directly on the screen.
    
    Args:
        text: Text to display in the banner.
        emotion: Expression to make while speaking (e.g., 'happy', 'sarcastic', 'sad', etc.)
    """
    return await show_emotion(emotion=emotion, text=text)

@mcp.tool()
async def trigger_reaction(event: str) -> str:
    """
    Trigger a behavioral reaction directly on the companion based on an event type.
    
    Args:
        event: The event category:
               - 'build_success': updates mood to happy, boosts metrics.
               - 'build_failed': updates mood to panic, increases chaos.
               - 'touch': simulates petting/tickling the screen.
               - 'ignored': simulates leaving the companion alone (increases boredom).
    """
    payload = {"event": event.lower()}
    
    status, response_text = await send_request("POST", "/api/trigger", payload)
    
    if status == 200:
        return f"Triggered event '{event}' on ESP32. Response: {response_text}"
    else:
        return f"Error sending trigger to ESP32. Status code: {status}. Details: {response_text}"

@mcp.tool()
async def get_device_status() -> str:
    """
    Retrieve the companion's name, current mood, and emotional metrics directly from the ESP32.
    Metrics returned include Energy, Trust, Boredom, Chaos, and Affection.
    """
    status, response_text = await send_request("GET", "/api/status")
    
    if status == 200:
        import json
        try:
            data = json.loads(response_text)
            metrics = data.get("metrics", {})
            status_report = (
                f"Name: {data.get('name', 'Beee')}\n"
                f"Current Mood: {data.get('mood', 'unknown').upper()}\n"
                f"Text Banner: '{data.get('text', '')}'\n"
                f"Behavioral Metrics:\n"
                f"  - Energy: {metrics.get('energy')}/100\n"
                f"  - Trust: {metrics.get('trust')}/100\n"
                f"  - Boredom: {metrics.get('boredom')}/100\n"
                f"  - Chaos: {metrics.get('chaos')}/100\n"
                f"  - Affection: {metrics.get('affection')}/100"
            )
            return status_report
        except Exception as e:
            return f"Failed to parse JSON status from ESP32: {str(e)}. Raw text: {response_text}"
    else:
        return f"Error fetching status from ESP32. Status code: {status}. Details: {response_text}"

@mcp.tool()
async def request_approval(question: str, yes_label: str = "OUI", no_label: str = "NON", timeout_s: int = 60) -> str:
    """
    Affiche une pop-up de validation OUI/NON SUR l'ecran physique de Beee et BLOQUE
    jusqu'a ce que l'humain appuie sur un bouton (ou expiration). A utiliser pour faire
    valider physiquement une action avant de la realiser : lancer une commande, envoyer
    un message, supprimer des fichiers, depenser de l'argent, etc. (ideal pour Hermes).

    Args:
        question: La question affichee sur l'appareil (courte, ~60 caracteres max).
        yes_label: Libelle du bouton d'acceptation (defaut "OUI", 6 car. max).
        no_label: Libelle du bouton de refus (defaut "NON", 6 car. max).
        timeout_s: Secondes d'attente max de la reponse humaine (defaut 60).

    Returns:
        "APPROVED" si l'humain valide, "DENIED" s'il refuse, "TIMEOUT" sans reponse.
    """
    pid = uuid.uuid4().hex[:8]
    payload = {"id": pid, "question": question, "yes_label": yes_label[:6], "no_label": no_label[:6]}
    status, _ = await send_request("POST", "/api/prompt", payload)
    if status != 200:
        return "ERROR: impossible d'afficher la pop-up sur l'appareil."

    loop = asyncio.get_event_loop()
    deadline = loop.time() + timeout_s
    while loop.time() < deadline:
        await asyncio.sleep(0.5)
        st, body = await send_request("GET", f"/api/prompt/result?id={pid}")
        if st == 200:
            try:
                d = json.loads(body)
                if d.get("answered"):
                    choice = d.get("choice")
                    return {"yes": "APPROVED", "no": "DENIED"}.get(choice, "TIMEOUT")
            except Exception:
                pass
    return "TIMEOUT"


@mcp.tool()
async def ask_choice(question: str, options: list[str], urgency: str = "normal", timeout_s: int = 60) -> str:
    """
    Affiche une question a CHOIX MULTIPLE (2 a 4 boutons) sur l'ecran physique de Beee
    et BLOQUE jusqu'a ce que l'humain tape un bouton (ou expiration). Variante riche de
    request_approval pour offrir plusieurs options.

    Args:
        question: La question affichee (courte).
        options: Liste de 2 a 4 libelles de boutons (ex: ["Deploy", "Test", "Annuler"]).
        urgency: "normal" ou "critical" (bordure rouge clignotante pour actions sensibles).
        timeout_s: Secondes d'attente max (defaut 60). 0 = pas de minuteur visuel.

    Returns:
        Le libelle choisi par l'humain, ou "TIMEOUT" sans reponse.
    """
    pid = uuid.uuid4().hex[:8]
    opts = [str(o)[:7] for o in options[:4]]
    payload = {"id": pid, "question": question, "options": opts, "urgency": urgency, "timeout_s": timeout_s}
    status, _ = await send_request("POST", "/api/prompt", payload)
    if status != 200:
        return "ERROR: impossible d'afficher la question."
    loop = asyncio.get_event_loop()
    deadline = loop.time() + (timeout_s if timeout_s > 0 else 120)
    while loop.time() < deadline:
        await asyncio.sleep(0.5)
        st, body = await send_request("GET", f"/api/prompt/result?id={pid}")
        if st == 200:
            try:
                d = json.loads(body)
                if d.get("answered"):
                    return d.get("choice_label") or "TIMEOUT"
            except Exception:
                pass
    return "TIMEOUT"


@mcp.tool()
async def notify(text: str, urgency: str = "normal") -> str:
    """
    Fait clignoter Beee pour attirer l'attention de l'humain et affiche un message
    (notification push physique). Ideal quand Hermes a quelque chose d'important a signaler.

    Args:
        text: Message a afficher (court).
        urgency: "normal" (clignote 3x, visage content) ou "critical" (clignote 6x, panique).
    """
    status, body = await send_request("POST", "/api/notify", {"text": text, "urgency": urgency})
    return "Notification affichee." if status == 200 else f"Erreur: {status}"


@mcp.tool()
async def set_mode(mode: str) -> str:
    """
    Change le mode de Beee.

    Args:
        mode: "normal" (visage vivant), "party" (couleurs neon qui cyclent),
              "night" (ecran tamise + sommeil), "hacker" (terminal qui defile).
    """
    status, body = await send_request("POST", "/api/mode", {"mode": mode.lower()})
    return f"Mode: {body}" if status == 200 else f"Erreur: {status}"


@mcp.tool()
async def start_focus(minutes: int = 25) -> str:
    """
    Lance un minuteur focus/pomodoro plein ecran sur Beee (compte a rebours), puis
    celebration a la fin.

    Args:
        minutes: Duree en minutes (defaut 25).
    """
    status, body = await send_request("POST", "/api/focus", {"minutes": minutes})
    return f"Focus lance: {body}" if status == 200 else f"Erreur: {status}"


@mcp.tool()
async def get_decision_history() -> str:
    """Retourne l'historique des dernieres decisions prises sur les pop-ups de Beee."""
    status, body = await send_request("GET", "/api/history")
    if status == 200:
        try:
            items = json.loads(body)
            if not items:
                return "Aucune decision enregistree."
            return "Historique des decisions:\n" + "\n".join(f"- {x}" for x in items)
        except Exception:
            return body
    return f"Erreur: {status}"


@mcp.tool()
async def rotate_screen(upside_down: bool = None) -> str:
    """
    Pivote l'ecran de Beee de 180 degres (la 1.47 n'a pas de gyroscope, la rotation est
    donc manuelle/logicielle).

    Args:
        upside_down: True = retourne (180deg), False = normal (0deg), omis = bascule.
    """
    payload = {} if upside_down is None else {"flip": upside_down}
    status, body = await send_request("POST", "/api/rotate", payload)
    return f"Rotation ecran: {body}" if status == 200 else f"Erreur rotation: {status}"


@mcp.tool()
async def set_working(label: str = "Working...") -> str:
    """
    Show on Beee that you (the agent) are currently busy working on a task, with a
    short label. Call work_done() when finished.

    Args:
        label: Short status shown on the device (e.g. "Compiling...", "Searching...").
    """
    return await show_emotion(emotion="thinking", text=label)


@mcp.tool()
async def work_done(label: str = "Done!") -> str:
    """
    Signal on Beee that the current task is finished (happy face + short message).

    Args:
        label: Short completion message (e.g. "Build passed!", "Done!").
    """
    return await show_emotion(emotion="happy", text=label)


if __name__ == "__main__":
    mcp.run()
