#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <ESPmDNS.h>

#include "config.h"
#include "i18n.h"
#include "face_renderer.h"
#include "touch_driver.h"
#include "imu_driver.h"
#include "web_dashboard.h"

// Screen hardware setup
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, LCD_RST, SCREEN_ROTATION, false /* IPS */,
    SCREEN_WIDTH, SCREEN_HEIGHT,
    34 /* col_offset1 */, 0 /* row_offset1 */,
    34 /* col_offset2 */, 0 /* row_offset2 */
);

FaceRenderer face(gfx);
TouchDriver touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
ImuDriver imu;

// Auto-rotation via gyroscope/accelerometre QMI8658
bool screenFlipped = false;             // false = 0deg (normal), true = 180deg
unsigned long lastOrientationCheck = 0;
unsigned long flipCandidateSince = 0;   // pour l'hysteresis (anti-clignotement)
bool flipCandidate = false;

// Web Servers on ESP32
WebServer server(80);
WebSocketsServer webSocket(81);

// Standalone C++ Persona Engine metrics
int energy = 80;
int trust = 50;
int boredom = 10;
int chaos = 20;
int affection = 60;
String currentMood = "idle";
String currentText = TXT_BOOT_CORE;

unsigned long lastTouchTime = 0;
unsigned long lastDecayTime = 0;
unsigned long mediaStreamTimeout = 0;
bool isStreamingMedia = false;

// Retour automatique a l'etat idle (rend la "boucle de vie" + clins d'oeil visibles)
unsigned long lastActivityTime = 0;

// --- KILLER FEATURE : pop-up d'approbation/choix pilotee par un agent LLM (via MCP) ---
bool promptActive = false;
String promptId = "";
String promptQuestion = "";
String promptOptions[4] = {TXT_YES, TXT_NO, "", ""};
int    promptOptionCount = 2;
String promptUrgency = "normal";   // "normal" / "critical" (bordure rouge clignotante)
long   promptTimeoutMs = 0;        // 0 = pas de minuteur visuel
bool   promptAnswered = false;
int    promptChoiceIdx = -1;
String promptChoiceLabel = "";
String promptChoice = "";          // compat OUI/NON : "yes" / "no" / "timeout"
unsigned long promptShownAt = 0;
unsigned long lastPromptRedraw = 0;

// --- Modes & vie ---
String currentMode = "normal";     // normal / party / night / focus
bool   focusActive = false;
unsigned long focusEndTime = 0;
unsigned long lastQuoteTime = 0;

// --- Barre de progression (reactions PC temps reel : compile/build...) ---
bool   progressActive = false;
int    progressPct = -1;           // -1 = indetermine (balayage)
String progressText = "Working...";

// Backlight PWM (LEDC) pour le mode nuit
#define BL_CHANNEL 0

// Historique des decisions (ring buffer)
String history[10];
int    historyCount = 0;
int    historyHead = 0;
void pushHistory(const String &line) {
    history[historyHead] = line;
    historyHead = (historyHead + 1) % 10;
    if (historyCount < 10) historyCount++;
}

// Citations aleatoires lachees en idle (un peu de vie/personnalite) - voir i18n.h
const char* QUOTES[] = QUOTE_LIST;
const int QUOTE_COUNT = 10;

// Register LCD commands (Required for JD9853 initialization)
void initLCDRegisters() {
    static const uint8_t init_operations[] = {
        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x11,  // Exit sleep
        END_WRITE,
        DELAY, 120,

        BEGIN_WRITE,
        WRITE_C8_D16, 0xDF, 0x98, 0x53, // Unlock commands
        WRITE_C8_D8, 0xB2, 0x23, 

        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 4,
        0x00, 0x47, 0x00, 0x6F,

        WRITE_COMMAND_8, 0xBB,
        WRITE_BYTES, 6,
        0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,

        WRITE_C8_D16, 0xC0, 0x44, 0xA4,
        WRITE_C8_D8, 0xC1, 0x16, 

        WRITE_COMMAND_8, 0xC3,
        WRITE_BYTES, 8,
        0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,

        WRITE_COMMAND_8, 0xC4,
        WRITE_BYTES, 12,
        0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,

        WRITE_COMMAND_8, 0xC8,
        WRITE_BYTES, 32,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00, 
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,

        WRITE_COMMAND_8, 0xD0,
        WRITE_BYTES, 5,
        0x04, 0x06, 0x6B, 0x0F, 0x00,

        WRITE_C8_D16, 0xD7, 0x00, 0x30,
        WRITE_C8_D8, 0xE6, 0x14, 
        WRITE_C8_D8, 0xDE, 0x01, 

        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 5,
        0x03, 0x13, 0xEF, 0x35, 0x35,

        WRITE_COMMAND_8, 0xC1,
        WRITE_BYTES, 3,
        0x14, 0x15, 0xC0,

        WRITE_C8_D16, 0xC2, 0x06, 0x3A,
        WRITE_C8_D16, 0xC4, 0x72, 0x12,
        WRITE_C8_D8, 0xBE, 0x00, 
        WRITE_C8_D8, 0xDE, 0x02, 

        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3,
        0x00, 0x02, 0x00,

        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3,
        0x01, 0x02, 0x00,

        WRITE_C8_D8, 0xDE, 0x00, 
        WRITE_C8_D8, 0x35, 0x00, 
        WRITE_C8_D8, 0x3A, 0x05, 

        WRITE_COMMAND_8, 0x2A,
        WRITE_BYTES, 4,
        0x00, 0x22, 0x00, 0xCD,

        WRITE_COMMAND_8, 0x2B,
        WRITE_BYTES, 4,
        0x00, 0x00, 0x01, 0x3F,

        WRITE_C8_D8, 0xDE, 0x02, 

        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3,
        0x00, 0x02, 0x00,
        
        WRITE_C8_D8, 0xDE, 0x00, 
        WRITE_C8_D8, 0x36, 0x00, // MADCTL: MX=0/MY=0 (pas de miroir), BGR=0 (test ordre couleur RGB)
        WRITE_COMMAND_8, 0x21, // Inversion on
        END_WRITE,
        
        DELAY, 10,

        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x29,  // Display output ON
        END_WRITE
    };
    bus->batchOperation(init_operations, sizeof(init_operations));
}

// Applique l'orientation de l'ecran (0deg / 180deg) : reecrit le MADCTL (0x36) et
// reoriente le tactile. La fenetre colonne (34..205) etant symetrique, le flip
// MX|MY garde l'image bien centree.
void applyOrientation(bool flipped) {
    uint8_t madctl = flipped ? 0xC0 : 0x00; // 0xC0 = MX|MY (180deg), BGR=0
    uint8_t op[] = { BEGIN_WRITE, WRITE_C8_D8, 0x36, madctl, END_WRITE };
    bus->batchOperation(op, sizeof(op));

    touch.setRotation(flipped ? 2 : 0); // 2 = portrait inverse (x=rx, y=h-1-ry)
    gfx->fillScreen(RGB565_BLACK);
    face.forceRedraw(); // redessine le texte (le visage est redessine a chaque frame)
}

// Geometrie des boutons selon le nombre d'options (2 = cote a cote, 3-4 = grille 2x2).
void promptButtonRect(int i, int count, int &bx, int &by, int &bw, int &bh) {
    if (count <= 2) {
        bw = 74; bh = 62; by = 250;
        bx = (i == 0) ? 6 : 92;
    } else {
        bw = 78; bh = 54;
        int col = i % 2, row = i / 2;
        bx = 6 + col * 84;     // 6 / 90
        by = 196 + row * 60;   // 196 / 256
    }
}

// Determine l'index du bouton touche (-1 si hors zone).
int promptHitTest(int tx, int ty, int count) {
    for (int i = 0; i < count; i++) {
        int bx, by, bw, bh;
        promptButtonRect(i, count, bx, by, bw, bh);
        if (tx >= bx && tx < bx + bw && ty >= by && ty < by + bh) return i;
    }
    return -1;
}

// Palette de boutons (couleurs distinctes pour le multi-choix)
const uint16_t BTN_FILL[4] = {0x0320, 0x5000, 0x3208, 0x6300}; // vert, rouge, violet, orange
const uint16_t BTN_EDGE[4] = {0x07E0, 0xF800, 0x781F, 0xFD20};

// Dessine la pop-up : bandeau, question (wrap), boutons, minuteur, bordure d'urgence.
void drawPrompt() {
    uint16_t bg = (promptUrgency == "critical") ? 0x1000 : 0x0008;
    gfx->fillScreen(bg);

    uint16_t bannerCol = (promptUrgency == "critical") ? 0xF800 : 0xF81F;
    gfx->fillRect(0, 0, SCREEN_WIDTH, 26, bannerCol);
    gfx->setTextColor(0x0000);
    gfx->setTextSize(1);
    gfx->setCursor(8, 9);
    gfx->print(promptUrgency == "critical" ? TXT_PROMPT_CRITICAL : TXT_PROMPT_BANNER);

    // Question
    int yMax = (promptOptionCount > 2) ? 180 : 210;
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(2);
    const int x0 = 6, lineH = 18, maxChars = 13;
    int y = 44;
    String word = "", line = "";
    for (size_t i = 0; i <= promptQuestion.length(); i++) {
        char c = (i < promptQuestion.length()) ? promptQuestion[i] : ' ';
        if (c == ' ' || c == '\n') {
            if (line.length() + word.length() + 1 > (unsigned)maxChars && line.length() > 0) {
                gfx->setCursor(x0, y); gfx->print(line); y += lineH; line = "";
                if (y > yMax) break;
            }
            if (line.length() > 0) line += " ";
            line += word; word = "";
        } else word += c;
    }
    if (line.length() > 0 && y <= yMax) { gfx->setCursor(x0, y); gfx->print(line); }

    // Boutons
    gfx->setTextSize(2);
    for (int i = 0; i < promptOptionCount; i++) {
        int bx, by, bw, bh;
        promptButtonRect(i, promptOptionCount, bx, by, bw, bh);
        gfx->fillRoundRect(bx, by, bw, bh, 8, BTN_FILL[i]);
        gfx->drawRoundRect(bx, by, bw, bh, 8, BTN_EDGE[i]);
        gfx->setTextColor(BTN_EDGE[i]);
        String lab = promptOptions[i];
        if (lab.length() > 7) lab = lab.substring(0, 7);
        int tw = lab.length() * 12;
        gfx->setCursor(bx + (bw - tw) / 2, by + bh / 2 - 8);
        gfx->print(lab);
    }
}

// Barre de minuteur + clignotement d'urgence, redessines periodiquement (sans tout refaire).
void drawPromptOverlay() {
    unsigned long now = millis();
    // Minuteur (barre verte qui se vide) juste sous le bandeau
    if (promptTimeoutMs > 0) {
        long remain = (long)promptTimeoutMs - (long)(now - promptShownAt);
        if (remain < 0) remain = 0;
        int w = (int)((long)(SCREEN_WIDTH) * remain / promptTimeoutMs);
        gfx->fillRect(0, 27, SCREEN_WIDTH, 4, 0x0008);
        uint16_t c = (remain < promptTimeoutMs / 4) ? 0xF800 : 0x07E0;
        gfx->fillRect(0, 27, w, 4, c);
    }
    // Bordure clignotante si critique
    if (promptUrgency == "critical") {
        bool on = ((now / 350) % 2) == 0;
        uint16_t c = on ? 0xF800 : 0x0008;
        gfx->drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, c);
        gfx->drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, c);
    }
}

// Ecran de progression : label + barre (determinee ou indeterminee qui balaye).
void drawProgressFrame() {
    gfx->fillScreen(0x0008);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 26, 0x07FF); // bandeau cyan
    gfx->setTextColor(0x0000); gfx->setTextSize(1);
    gfx->setCursor(8, 9); gfx->print(">> BEEE WORKS <<");
    // Yeux concentres stylises (deux barres) facon "thinking"
    gfx->fillRoundRect(40, 90, 30, 30, 8, 0x07FF);
    gfx->fillRoundRect(102, 90, 30, 30, 8, 0x07FF);
    gfx->fillCircle(58, 108, 5, 0x0008);
    gfx->fillCircle(120, 108, 5, 0x0008);
    gfx->setTextColor(0xFFFF); gfx->setTextSize(1);
    gfx->setCursor(8, 150); gfx->print(progressText);
    gfx->drawRoundRect(16, 170, 140, 18, 4, 0x528A);
}

void drawProgressBar() {
    gfx->fillRect(18, 172, 136, 14, 0x0008); // efface l'interieur
    if (progressPct < 0) {
        int seg = 42, travel = 136 - seg;
        if (travel < 1) travel = 1;
        int t = (millis() / 6) % (2 * travel);
        int off = (t < travel) ? t : (2 * travel - t);
        gfx->fillRoundRect(18 + off, 172, seg, 14, 3, 0x07FF);
    } else {
        int p = progressPct; if (p < 0) p = 0; if (p > 100) p = 100;
        int w = 136 * p / 100; if (w < 2) w = 2;
        gfx->fillRoundRect(18, 172, w, 14, 3, 0x07FF);
        gfx->setTextColor(0xFFFF); gfx->setTextSize(1);
        char b[6]; snprintf(b, sizeof(b), "%d%%", p);
        gfx->setCursor(78, 194); gfx->print(b);
    }
}

// Ecran focus/pomodoro : gros compte a rebours MM:SS.
void drawFocus() {
    long remain = (long)focusEndTime - (long)millis();
    if (remain < 0) remain = 0;
    int total = remain / 1000;
    int mm = total / 60, ss = total % 60;

    gfx->fillScreen(0x0008);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 26, 0x07FF);
    gfx->setTextColor(0x0000); gfx->setTextSize(1);
    gfx->setCursor(40, 9); gfx->print(TXT_FOCUS_TITLE);

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
    gfx->setTextColor(0x07FF); gfx->setTextSize(4);
    gfx->setCursor(14, 130); gfx->print(buf);

    // petite barre de progression
    gfx->drawRect(16, 200, 140, 10, 0x528A);
}

// Splash d'identite au demarrage : le nom "Beee" + tagline.
void drawBootSplash() {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(0x07FF); // cyan
    gfx->setTextSize(5);
    gfx->setCursor(26, 104);
    gfx->print("Beee");
    gfx->setTextColor(0xF81F); // magenta
    gfx->setTextSize(1);
    gfx->setCursor(20, 168);
    gfx->print("AI agent companion");
    gfx->fillRect(36, 196, 100, 3, 0x528A);
}

Expression parseExpression(const String &expStr) {
    if (expStr == "idle") return EXP_IDLE;
    if (expStr == "happy") return EXP_HAPPY;
    if (expStr == "sad") return EXP_SAD;
    if (expStr == "angry") return EXP_ANGRY;
    if (expStr == "sarcastic" || expStr == "side_eye") return EXP_SARCASMATIC;
    if (expStr == "thinking") return EXP_THINKING;
    if (expStr == "sleeping") return EXP_SLEEPING;
    if (expStr == "panic") return EXP_PANIC;
    if (expStr == "hacker") return EXP_HACKER;
    return EXP_IDLE;
}

// Broadcast metrics & mood state to all connected dashboard websockets
void broadcastStateToClients() {
    JsonDocument doc;
    doc["type"] = "status";
    doc["mood"] = currentMood;
    doc["reaction_text"] = currentText;
    
    JsonObject metricsObj = doc.createNestedObject("metrics");
    metricsObj["energy"] = energy;
    metricsObj["trust"] = trust;
    metricsObj["boredom"] = boredom;
    metricsObj["chaos"] = chaos;
    metricsObj["affection"] = affection;

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

// C++ Persona Engine Interaction Logic
void registerInteraction(const String &type, int detailsX = 0, int detailsY = 0) {
    // Toute interaction/parole sort du mode media/progression et redonne la main au visage.
    if (isStreamingMedia || progressActive) {
        isStreamingMedia = false;
        progressActive = false;
        gfx->fillScreen(RGB565_BLACK);
        face.forceRedraw();
    }
    lastActivityTime = millis(); // rearme le minuteur de retour a idle
    boredom = max(0, boredom - 20);
    
    if (type == "touch") {
        if (detailsY > 240) { // Bottom tickle
            affection = min(100, affection + 10);
            trust = min(100, trust + 5);
            energy = min(100, energy + 5);
            currentMood = (affection > 80) ? "happy" : "idle";
            currentText = TXT_TICKLE;
        } else if (detailsX < 50) { // Left eye poke
            chaos = min(100, chaos + 15);
            currentMood = "side_eye";
            currentText = TXT_LEFT_EYE;
        } else if (detailsX > 120) { // Right eye poke
            chaos = min(100, chaos + 15);
            currentMood = "side_eye";
            currentText = TXT_RIGHT_EYE;
        } else { // Center face pet
            affection = min(100, affection + 5);
            energy = min(100, energy + 10);
            currentMood = "happy";
            currentText = TXT_PET;
        }
    } else if (type == "build_success") {
        energy = min(100, energy + 20);
        trust = min(100, trust + 10);
        currentMood = "happy";
        currentText = TXT_BUILD_OK;
    } else if (type == "build_failed") {
        energy = max(0, energy - 15);
        chaos = min(100, chaos + 20);
        currentMood = "panic";
        currentText = TXT_BUILD_FAIL;
    } else if (type == "ignored") {
        boredom = min(100, boredom + 30);
        if (boredom > 60) {
            currentMood = "sleeping";
            currentText = TXT_SLEEP;
        } else {
            currentMood = "side_eye";
            currentText = TXT_IGNORED;
        }
    } else if (type == "mail") {
        currentMood = "thinking";
        currentText = TXT_MAIL;
    } else if (type == "alert") {
        chaos = min(100, chaos + 25);
        currentMood = "panic";
        currentText = TXT_ALERT;
    } else if (type == "task_done") {
        energy = min(100, energy + 15);
        affection = min(100, affection + 5);
        currentMood = "happy";
        currentText = TXT_TASK_DONE;
    } else if (type == "agent_speak") {
        // Updated via API / MCP tool
        boredom = max(0, boredom - 25);
    }
    
    face.setExpression(parseExpression(currentMood));
    face.setText(currentText);
    
    // Notify browser clients
    broadcastStateToClients();
}

// Decay metrics over time (run every 30s)
void decayPersona() {
    energy = max(0, energy - 1);
    boredom = min(100, boredom + 2);
    
    if (boredom > 70) {
        currentMood = (energy < 20) ? "sleeping" : "idle";
    }
    
    broadcastStateToClients();
}

// WebSocket Event Handler (port 81)
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%d] Client disconnected\n", num);
            break;
        case WStype_CONNECTED: {
            Serial.printf("[%d] Client connected\n", num);
            // Send initial state upon connect
            JsonDocument doc;
            doc["type"] = "status";
            doc["mood"] = currentMood;
            doc["reaction_text"] = currentText;
            
            JsonObject metricsObj = doc.createNestedObject("metrics");
            metricsObj["energy"] = energy;
            metricsObj["trust"] = trust;
            metricsObj["boredom"] = boredom;
            metricsObj["chaos"] = chaos;
            metricsObj["affection"] = affection;

            String jsonString;
            serializeJson(doc, jsonString);
            webSocket.sendTXT(num, jsonString);
            break;
        }
        case WStype_TEXT:
            // Text payloads currently not used since browser communicates via REST API
            break;
            
        case WStype_BIN: {
            // Media envoye par le dashboard (JS) en 5 bandes de 172x32 px (11008 octets),
            // affichees dans la moitie basse de l'ecran (y=160..319).
            // Format paquet : [2 octets band_idx big-endian][11008 octets RGB565]
            if (length == 11010) {
                isStreamingMedia = true;
                mediaStreamTimeout = millis();

                uint16_t bandIdx = (payload[0] << 8) | payload[1];
                if (bandIdx < 5) {
                    gfx->draw16bitRGBBitmap(0, 160 + bandIdx * 32, (uint16_t *)(payload + 2), SCREEN_WIDTH, 32);
                }
            }
            break;
        }
        default:
            break;
    }
}

// Web Server REST Routing (port 80)
void setupWebRoutes() {
    // Serve HTML Dashboard
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "0");
        server.send_P(200, "text/html", DASHBOARD_HTML);
    });

    // GET Status Endpoint
    server.on("/api/status", HTTP_GET, []() {
        JsonDocument doc;
        doc["name"] = "Beee";
        doc["mood"] = currentMood;
        doc["text"] = currentText;
        
        JsonObject metricsObj = doc.createNestedObject("metrics");
        metricsObj["energy"] = energy;
        metricsObj["trust"] = trust;
        metricsObj["boredom"] = boredom;
        metricsObj["chaos"] = chaos;
        metricsObj["affection"] = affection;

        String jsonString;
        serializeJson(doc, jsonString);
        server.send(200, "application/json", jsonString);
    });

    // POST Emotion Endpoint
    server.on("/api/emotion", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");
            JsonDocument doc;
            deserializeJson(doc, body);
            
            String exp = doc["emotion"];
            if (exp == "") exp = (const char*)doc["expression"];
            String speechText = doc["text"];
            
            if (exp != "") {
                currentMood = exp;
                face.setExpression(parseExpression(currentMood));
            }
            if (speechText != "") {
                currentText = speechText;
                face.setText(currentText);
            }
            
            if (doc.containsKey("eye_x") && doc.containsKey("eye_y")) {
                face.setEyeOffset(doc["eye_x"], doc["eye_y"]);
            }
            
            registerInteraction("agent_speak");
            
            JsonDocument responseDoc;
            responseDoc["status"] = "success";
            responseDoc["mood"] = currentMood;
            String responseStr;
            serializeJson(responseDoc, responseStr);
            server.send(200, "application/json", responseStr);
        } else {
            server.send(400, "application/json", "{\"error\":\"Missing body\"}");
        }
    });

    // POST Trigger Endpoint
    server.on("/api/trigger", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");
            JsonDocument doc;
            deserializeJson(doc, body);
            
            String event = doc["event"];
            if (event != "") {
                registerInteraction(event);
                
                JsonDocument responseDoc;
                responseDoc["status"] = "success";
                responseDoc["mood"] = currentMood;
                responseDoc["text"] = currentText;
                String responseStr;
                serializeJson(responseDoc, responseStr);
                server.send(200, "application/json", responseStr);
            } else {
                server.send(400, "application/json", "{\"error\":\"Missing event field\"}");
            }
        } else {
            server.send(400, "application/json", "{\"error\":\"Missing body\"}");
        }
    });

    // === KILLER FEATURE : pop-up d'approbation / choix multiple ===
    // POST /api/prompt {id, question, options?[], yes_label?, no_label?, urgency?, timeout_s?}
    server.on("/api/prompt", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"Missing body\"}"); return; }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));

        promptId = (const char*)(doc["id"] | "");
        promptQuestion = (const char*)(doc["question"] | TXT_PROMPT_DEFAULT_Q);
        promptUrgency = (const char*)(doc["urgency"] | "normal");
        promptTimeoutMs = (long)(doc["timeout_s"] | 0) * 1000L;

        // Options : soit un tableau "options", soit yes_label/no_label (compat)
        promptOptionCount = 0;
        if (doc["options"].is<JsonArray>()) {
            for (JsonVariant v : doc["options"].as<JsonArray>()) {
                if (promptOptionCount >= 4) break;
                promptOptions[promptOptionCount++] = v.as<String>();
            }
        }
        if (promptOptionCount == 0) {
            promptOptions[0] = (const char*)(doc["yes_label"] | TXT_YES);
            promptOptions[1] = (const char*)(doc["no_label"]  | TXT_NO);
            promptOptionCount = 2;
        }

        promptAnswered = false;
        promptChoice = "";
        promptChoiceIdx = -1;
        promptChoiceLabel = "";
        promptActive = true;
        promptShownAt = millis();
        lastPromptRedraw = millis();

        drawPrompt();

        JsonDocument r; r["status"] = "shown"; r["id"] = promptId;
        String s; serializeJson(r, s);
        server.send(200, "application/json", s);
    });

    // GET /api/prompt/result?id=... -> {answered, choice, choice_index, choice_label}
    server.on("/api/prompt/result", HTTP_GET, []() {
        JsonDocument r;
        bool match = (server.arg("id") == promptId) || (server.arg("id") == "");
        r["answered"] = match ? promptAnswered : false;
        r["choice"] = match ? promptChoice : "";
        r["choice_index"] = match ? promptChoiceIdx : -1;
        r["choice_label"] = match ? promptChoiceLabel : "";
        r["active"] = promptActive;
        String s; serializeJson(r, s);
        server.send(200, "application/json", s);
    });

    // GET /api/history -> liste des dernieres decisions
    server.on("/api/history", HTTP_GET, []() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < historyCount; i++) {
            int idx = (historyHead - 1 - i + 10) % 10;
            arr.add(history[idx]);
        }
        String s; serializeJson(doc, s);
        server.send(200, "application/json", s);
    });

    // POST /api/mode {mode:"normal"|"party"|"night"|"hacker"}
    server.on("/api/mode", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"Missing body\"}"); return; }
        JsonDocument doc; deserializeJson(doc, server.arg("plain"));
        currentMode = (const char*)(doc["mode"] | "normal");

        focusActive = false;
        face.setParty(currentMode == "party");
        if (currentMode == "night") {
            ledcWrite(BL_CHANNEL, 18);  // backlight tres faible
            currentMood = "sleeping"; face.setExpression(EXP_SLEEPING); face.setText(TXT_SLEEP);
        } else {
            ledcWrite(BL_CHANNEL, 255); // pleine luminosite
            if (currentMode == "hacker") { currentMood = "hacker"; face.setExpression(EXP_HACKER); }
            else if (currentMode == "party") { currentMood = "happy"; face.setExpression(EXP_HAPPY); face.setText(TXT_PARTY); }
            else { currentMood = "idle"; face.setExpression(EXP_IDLE); }
        }
        gfx->fillScreen(RGB565_BLACK); face.forceRedraw();
        lastActivityTime = millis();
        JsonDocument r; r["status"] = "ok"; r["mode"] = currentMode;
        String s; serializeJson(r, s); server.send(200, "application/json", s);
    });

    // POST /api/progress {percent, text}  (percent -1 = indetermine)
    server.on("/api/progress", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"Missing body\"}"); return; }
        JsonDocument doc; deserializeJson(doc, server.arg("plain"));
        int pct = doc["percent"] | -1;
        String txt = (const char*)(doc["text"] | "Working...");
        bool needFrame = (!progressActive) || (txt != progressText);
        progressPct = pct;
        progressText = txt;
        progressActive = true;
        promptActive = false; isStreamingMedia = false; focusActive = false;
        if (needFrame) drawProgressFrame();
        drawProgressBar();
        JsonDocument r; r["status"] = "ok"; String s; serializeJson(r, s);
        server.send(200, "application/json", s);
    });

    // POST /api/focus {minutes:25}
    server.on("/api/focus", HTTP_POST, []() {
        int minutes = 25;
        if (server.hasArg("plain")) {
            JsonDocument doc; deserializeJson(doc, server.arg("plain"));
            minutes = doc["minutes"] | 25;
        }
        focusActive = true;
        currentMode = "focus";
        focusEndTime = millis() + (unsigned long)minutes * 60000UL;
        ledcWrite(BL_CHANNEL, 255);
        JsonDocument r; r["status"] = "ok"; r["minutes"] = minutes;
        String s; serializeJson(r, s); server.send(200, "application/json", s);
    });

    // POST /api/notify {text, urgency?} -> Beee attire l'attention + affiche le message
    server.on("/api/notify", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"Missing body\"}"); return; }
        JsonDocument doc; deserializeJson(doc, server.arg("plain"));
        String txt = (const char*)(doc["text"] | TXT_NOTIFY_DEFAULT);
        String urg = (const char*)(doc["urgency"] | "normal");

        // Sort des modes plein-ecran pour montrer le visage + message
        promptActive = false; isStreamingMedia = false; focusActive = false;
        gfx->fillScreen(RGB565_BLACK);
        currentMood = (urg == "critical") ? "panic" : "happy";
        face.setExpression(parseExpression(currentMood));
        currentText = txt; face.setText(txt);
        face.forceRedraw();
        // flash d'attention sur le backlight
        for (int i = 0; i < (urg == "critical" ? 6 : 3); i++) {
            ledcWrite(BL_CHANNEL, 30); delay(70);
            ledcWrite(BL_CHANNEL, 255); delay(70);
        }
        lastActivityTime = millis();
        broadcastStateToClients();
        JsonDocument r; r["status"] = "ok"; String s; serializeJson(r, s);
        server.send(200, "application/json", s);
    });

    // POST /api/rotate {flip:true|false}  (rotation manuelle, pas de gyro sur la 1.47)
    server.on("/api/rotate", HTTP_POST, []() {
        bool flip = !screenFlipped;
        if (server.hasArg("plain")) {
            JsonDocument doc; deserializeJson(doc, server.arg("plain"));
            if (doc["flip"].is<bool>()) flip = doc["flip"].as<bool>();
        }
        screenFlipped = flip;
        applyOrientation(screenFlipped);
        JsonDocument r; r["status"] = "ok"; r["flipped"] = screenFlipped;
        String s; serializeJson(r, s);
        server.send(200, "application/json", s);
    });
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("======================================");
    Serial.println("Beee starting...");
    
    // Backlight en PWM (LEDC) pour permettre le mode nuit (luminosite variable)
    ledcSetup(BL_CHANNEL, 5000, 8);
    ledcAttachPin(LCD_BL, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, 255); // pleine luminosite
    
    // Initialize GFX screen
    if (!gfx->begin()) {
        Serial.println("Error initializing GFX display!");
    }
    initLCDRegisters();
    gfx->fillScreen(RGB565_BLACK);

    // Splash d'identite Beee (~1.5s)
    drawBootSplash();
    delay(1500);

    // === MIRE DE DIAGNOSTIC ECRAN ===
    // Mettre a 0 pour desactiver une fois l'orientation/couleurs validees.
#define DISPLAY_TEST 0
#if DISPLAY_TEST
    {
        int wHalf = SCREEN_WIDTH / 2;   // 86
        int hHalf = SCREEN_HEIGHT / 2;  // 160
        // Quadrants de couleur PURE
        gfx->fillRect(0,     0,     wHalf, hHalf, 0xF800); // Haut-Gauche  = ROUGE
        gfx->fillRect(wHalf, 0,     wHalf, hHalf, 0x07E0); // Haut-Droite  = VERT
        gfx->fillRect(0,     hHalf, wHalf, hHalf, 0x001F); // Bas-Gauche   = BLEU
        gfx->fillRect(wHalf, hHalf, wHalf, hHalf, 0xFFE0); // Bas-Droite   = JAUNE
        // Labels (couleur attendue ecrite en toutes lettres)
        gfx->setTextSize(2);
        gfx->setTextColor(0xFFFF);
        gfx->setCursor(6,   6);   gfx->print("ROUGE");
        gfx->setCursor(96,  6);   gfx->print("VERT");
        gfx->setTextColor(0x0000);
        gfx->setCursor(6,   166); gfx->print("BLEU");
        gfx->setCursor(96,  166); gfx->print("JAUNE");
        // Repere "HAUT" pour l'orientation verticale
        gfx->setTextColor(0xFFFF);
        gfx->setCursor(40, 150);  gfx->print("^HAUT^");
        while (true) { delay(1000); } // fige sur la mire
    }
#endif

    // Begin Face renderer
    face.begin();
    face.setText(TXT_BOOT_NET);
    face.setExpression(EXP_THINKING);
    face.draw();
    
    // Initialize Touch controller
    touch.begin(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_ROTATION);

    // IMU (QMI8658) absent sur la 1.47 (scan I2C: seul le tactile 0x63 repond).
    // begin() echoue proprement -> auto-rotation gyro desactivee. Rotation manuelle
    // possible via /api/rotate (voir plus bas).
    imu.begin();
    
    // Connect to WiFi
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        
        // Start mDNS resolver (allows connection via http://beee.local)
        if (MDNS.begin("beee")) {
            Serial.println("mDNS responder started: http://beee.local");
        }
        
        currentMood = "happy"; // coherent avec l'expression -> drift vers idle apres 12s
        face.setText(String(TXT_BOOT_READY) + WiFi.localIP().toString());
        face.setExpression(EXP_HAPPY);
        face.draw();
    } else {
        Serial.println("\nWiFi Connection Failed! Offline mode active.");
        currentMood = "sad";
        face.setText(TXT_BOOT_WIFI_FAIL);
        face.setExpression(EXP_SAD);
        face.draw();
    }
    
    // Setup HTTP Web Routes and start server
    setupWebRoutes();
    server.begin();
    Serial.println("HTTP server started on port 80");
    
    // Start WebSocket Server on port 81
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket server started on port 81");

    lastActivityTime = millis(); // amorce le minuteur de retour a idle
}

void loop() {
    server.handleClient();
    webSocket.loop();
    
    unsigned long now = millis();
    
    // Poll touch coordinates
    int tx = 0, ty = 0;
    if (touch.readTouch(tx, ty)) {
        if (promptActive) {
            // Pop-up active : le tactile choisit un bouton (1 a 4 options)
            int hit = (now - promptShownAt > 400) ? promptHitTest(tx, ty, promptOptionCount) : -1;
            if (hit >= 0) {
                promptChoiceIdx = hit;
                promptChoiceLabel = promptOptions[hit];
                promptChoice = (promptOptionCount == 2) ? (hit == 0 ? "yes" : "no") : "";
                promptAnswered = true;
                promptActive = false;
                pushHistory(promptQuestion + " -> " + promptChoiceLabel);
                Serial.printf("Prompt repondu: [%d] %s\n", hit, promptChoiceLabel.c_str());

                bool pos = (promptOptionCount == 2 && hit == 0);
                face.setExpression(pos ? EXP_HAPPY : EXP_SARCASMATIC);
                face.setText(pos ? String(TXT_APPROVED) : ("> " + promptChoiceLabel));
                currentMood = pos ? "happy" : "idle";
                gfx->fillScreen(RGB565_BLACK);
                face.forceRedraw();
                lastActivityTime = now;
                broadcastStateToClients();
            }
        } else if (now - lastTouchTime > 250) { // throttle 250ms
            lastTouchTime = now;
            Serial.printf("Touch registered on screen: x=%d, y=%d\n", tx, ty);
            face.triggerBlink();

            // Easter egg : triple-tap rapide (<1.2s) -> mode hacker secret
            static unsigned long firstTapTime = 0;
            static int tapCount = 0;
            if (now - firstTapTime > 1200) { firstTapTime = now; tapCount = 0; }
            tapCount++;
            if (tapCount >= 3) {
                tapCount = 0;
                currentMood = "hacker";
                currentText = "ACCESS GRANTED";
                face.setExpression(EXP_HACKER);
                face.setText(currentText);
                lastActivityTime = now;
                broadcastStateToClients();
            } else {
                registerInteraction("touch", tx, ty);
            }
        }
    }
    
    // Natural decay of persona values every 30 seconds
    if (now - lastDecayTime > 30000) {
        lastDecayTime = now;
        decayPersona();
    }

    // Auto-rotation : on lit l'accelerometre ~5x/sec et on bascule 0<->180 avec
    // hysteresis (l'etat candidat doit tenir 600ms pour eviter le clignotement).
    if (imu.present() && now - lastOrientationCheck > 200) {
        lastOrientationCheck = now;
        float ax, ay, az;
        if (imu.readAccel(ax, ay, az)) {
            // ay est l'axe vertical de l'ecran : ~+1g a l'endroit, ~-1g retourne.
            // (Si la rotation se declenche a l'envers, inverser le test ci-dessous.)
            bool wantFlip = screenFlipped;
            if (ay < -0.45f) wantFlip = true;   // franchement retourne
            else if (ay > 0.45f) wantFlip = false; // franchement a l'endroit

            if (wantFlip != screenFlipped) {
                if (!flipCandidate || flipCandidate != wantFlip) {
                    flipCandidate = wantFlip;
                    flipCandidateSince = now;
                } else if (now - flipCandidateSince > 600) {
                    screenFlipped = wantFlip;
                    applyOrientation(screenFlipped);
                    Serial.printf("Orientation: %s\n", screenFlipped ? "180 (retourne)" : "0 (normal)");
                }
            } else {
                flipCandidate = screenFlipped;
            }
        }
    }

    // Mode focus/pomodoro : compte a rebours plein ecran, puis celebration.
    if (focusActive) {
        if (now >= focusEndTime) {
            focusActive = false;
            currentMode = "normal";
            currentMood = "happy";
            gfx->fillScreen(RGB565_BLACK);
            face.setExpression(EXP_HAPPY);
            face.setText(TXT_FOCUS_DONE);
            face.forceRedraw();
            lastActivityTime = now;
            broadcastStateToClients();
        } else {
            static unsigned long lastFocusDraw = 0;
            if (now - lastFocusDraw > 250) { lastFocusDraw = now; drawFocus(); }
        }
        return; // rien d'autre pendant le focus
    }

    // Barre de progression (reactions PC) : on anime la barre, on saute le visage.
    if (progressActive) {
        static unsigned long lastProgDraw = 0;
        if (now - lastProgDraw > 60) { lastProgDraw = now; drawProgressBar(); }
        return;
    }

    // Retour automatique a l'etat idle apres ~12s sans activite (debloque les clins d'oeil).
    if (!promptActive && !isStreamingMedia && currentMode != "night" &&
        now - lastActivityTime > 12000 &&
        currentMood != "idle" && currentMood != "sleeping" && currentMood != "hacker") {
        currentMood = "idle";
        face.setExpression(EXP_IDLE);
        lastActivityTime = now;
        broadcastStateToClients();
    }

    // Pop-up : overlay (minuteur + clignotement critique) rafraichi periodiquement,
    // et expiration (timeout perso ou 120s par defaut) -> l'agent recoit "timeout".
    if (promptActive) {
        if (now - lastPromptRedraw > 150) { lastPromptRedraw = now; drawPromptOverlay(); }
        long limit = (promptTimeoutMs > 0) ? promptTimeoutMs : 120000L;
        if ((long)(now - promptShownAt) > limit) {
            promptActive = false;
            promptAnswered = true;
            promptChoice = "timeout";
            promptChoiceIdx = -1;
            pushHistory(promptQuestion + " -> (expire)");
            gfx->fillScreen(RGB565_BLACK);
            face.forceRedraw();
            lastActivityTime = now;
        }
    }

    // Citations aleatoires en idle (mode normal) : un peu de personnalite.
    if (!promptActive && !isStreamingMedia && currentMode == "normal" &&
        currentMood == "idle" && now - lastQuoteTime > 45000UL + random(0, 45000)) {
        lastQuoteTime = now;
        currentText = QUOTES[random(0, QUOTE_COUNT)];
        face.setText(currentText);
        broadcastStateToClients();
    }

    // Rendu du visage a 30 FPS (sauf pop-up / media : ecran dedie conserve).
    if (!isStreamingMedia && !promptActive) {
        static unsigned long lastDrawTime = 0;
        if (now - lastDrawTime > 33) {
            lastDrawTime = now;
            face.draw();
        }
    }
}
