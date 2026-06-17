#include "face_renderer.h"
#include "i18n.h"

// Convertit une chaine UTF-8 (francais) vers l'encodage CP437 utilise par la
// police classique 5x7 de GFX (font[c*5]). Les lettres accentuees existent en
// 0x80-0xFF dans cette police -> on obtient de VRAIS accents (é è à ç ...) sans
// embarquer de police custom. Utilise uniquement pour le rendu sur l'ecran.
static String frToCp437(const String &in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        uint8_t c = (uint8_t)in[i];
        if (c < 0x80) {
            out += (char)c; // ASCII pur (identique en CP437)
        } else if (c == 0xC3 && i + 1 < in.length()) {
            // Latin-1 supplement (U+00C0..U+00FF) : minuscules et majuscules accentuees
            uint8_t n = (uint8_t)in[++i];
            switch (n) {
                // minuscules
                case 0xA7: out += (char)0x87; break; // ç
                case 0xA0: out += (char)0x85; break; // à
                case 0xA1: out += (char)0xA0; break; // á
                case 0xA2: out += (char)0x83; break; // â
                case 0xA4: out += (char)0x84; break; // ä
                case 0xA8: out += (char)0x8A; break; // è
                case 0xA9: out += (char)0x82; break; // é
                case 0xAA: out += (char)0x88; break; // ê
                case 0xAB: out += (char)0x89; break; // ë
                case 0xAC: out += (char)0x8D; break; // ì
                case 0xAD: out += (char)0xA1; break; // í
                case 0xAE: out += (char)0x8C; break; // î
                case 0xAF: out += (char)0x8B; break; // ï
                case 0xB1: out += (char)0xA4; break; // ñ
                case 0xB2: out += (char)0x95; break; // ò
                case 0xB3: out += (char)0xA2; break; // ó
                case 0xB4: out += (char)0x93; break; // ô
                case 0xB6: out += (char)0x94; break; // ö
                case 0xB9: out += (char)0x97; break; // ù
                case 0xBA: out += (char)0xA3; break; // ú
                case 0xBB: out += (char)0x96; break; // û
                case 0xBC: out += (char)0x81; break; // ü
                // majuscules (glyphe CP437 dispo sinon repli sur la lettre simple)
                case 0x87: out += (char)0x80; break; // Ç
                case 0x89: out += (char)0x90; break; // É
                case 0x84: out += (char)0x8E; break; // Ä
                case 0x96: out += (char)0x99; break; // Ö
                case 0x9C: out += (char)0x9A; break; // Ü
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x85: out += 'A'; break; // À Á Â Ã Å
                case 0x88: case 0x8A: case 0x8B: out += 'E'; break; // È Ê Ë
                case 0x8C: case 0x8D: case 0x8E: case 0x8F: out += 'I'; break; // Ì Í Î Ï
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x99: out += 'O'; break; // Ò Ó Ô Õ Ù->O? (rare)
                case 0x9A: case 0x9B: out += 'U'; break; // Ú Û
                default:   out += '?'; break;
            }
        } else if (c == 0xC2 && i + 1 < in.length()) {
            // Latin-1 bas (U+0080..U+00BF) : guillemets, degre, etc.
            uint8_t n = (uint8_t)in[++i];
            switch (n) {
                case 0xAB: out += (char)0xAE; break; // «
                case 0xBB: out += (char)0xAF; break; // »
                case 0xB0: out += (char)0xF8; break; // °
                case 0xA0: out += ' ';        break; // espace insecable
                default:   /* ignore */       break;
            }
        } else if (c == 0xE2 && i + 2 < in.length()) {
            // Ponctuation typographique (’ ‘ “ ” …)
            uint8_t n1 = (uint8_t)in[i + 1];
            uint8_t n2 = (uint8_t)in[i + 2];
            i += 2;
            if (n1 == 0x80 && (n2 == 0x98 || n2 == 0x99)) out += '\'';
            else if (n1 == 0x80 && (n2 == 0x9C || n2 == 0x9D)) out += '"';
            else out += ' ';
        }
        // tout autre octet multi-octets non gere est ignore (evite le charabia)
    }
    return out;
}

FaceRenderer::FaceRenderer(Arduino_GFX *gfx) : _gfx(gfx) {
    _canvas = new Arduino_Canvas(SCREEN_WIDTH, 160, _gfx, 0, 0, 0);
    _currentExpression = EXP_IDLE;
    _text = TXT_INIT;
    _lastText = "";
    
    _eyeOffsetX = 0.0f;
    _eyeOffsetY = 0.0f;
    _targetEyeOffsetX = 0.0f;
    _targetEyeOffsetY = 0.0f;
    
    _blinkRatio = 1.0f;
    _isBlinking = false;
    _lastBlinkTime = 0;
    _blinkDuration = 150; // 150ms blink
    _expressionStartTime = 0;
    _breathingCycle = 0.0f;

    _rightEyeRatio = 1.0f;
    _isWinking = false;
    _winkStartTime = 0;
    _lastWinkTime = 0;
    _winkDuration = 260; // clin d'oeil un peu plus lent qu'un clignement

    _talkingUntil = 0;
    _lastGazeTime = 0;
    
    _hackerLineCount = 0;
    _lastHackerUpdate = 0;
    _hackerDirty = true;
    _glitchActive = false;
    _partyMode = false;
    
    // Sleek cyberpunk palette
    _colorBackground = 0x0000; // Black
    _colorFace = 0x07FF;       // Cyan (0, 255, 255)
    _colorAccent = 0xF81F;     // Magenta (255, 0, 255)
    _colorText = 0xFFFF;       // White
}

void FaceRenderer::begin() {
    if (_canvas) {
        // GFX_SKIP_OUTPUT_BEGIN : on alloue le framebuffer du canvas SANS re-init
        // de l'ecran. Sinon _output->begin() rejouerait l'init ST7789 par defaut et
        // ecraserait notre MADCTL JD9853 (0x36=0x00) -> retour miroir + couleurs BGR.
        _canvas->begin(GFX_SKIP_OUTPUT_BEGIN);
    }
    _gfx->fillScreen(_colorBackground);
    _lastBlinkTime = millis();
    _expressionStartTime = millis();
}

void FaceRenderer::setExpression(Expression exp) {
    if (_currentExpression != exp) {
        _currentExpression = exp;
        _expressionStartTime = millis();
        
        // Reset expression specific variables
        if (exp == EXP_HACKER) {
            _hackerLineCount = 0;
            _hackerLines[0] = ">> ACCESS GRANTED";
            _hackerLines[1] = ">> DECRYPTING PAYLOAD...";
            _hackerLines[2] = ">> BYPASSING FIREWALL...";
            _hackerLineCount = 3;
            _lastHackerUpdate = millis();
            _hackerDirty = true;
        }
        
        // Instant redraw of face area
        _gfx->fillRect(0, 0, SCREEN_WIDTH, 160, _colorBackground);
        _lastText = ""; // Force text redraw on expression change
    }
}

void FaceRenderer::setText(const String &text) {
    String enc = frToCp437(text); // accents UTF-8 -> CP437 (vrais accents a l'ecran)
    if (_text != enc) {
        _text = enc;
        // Declenche l'animation "il parle" : duree proportionnelle a la longueur
        // du texte (bornee), ~45ms par caractere, entre 0.6s et 4s.
        unsigned long talkMs = constrain((unsigned long)enc.length() * 45UL, 600UL, 4000UL);
        _talkingUntil = millis() + talkMs;
    }
}

void FaceRenderer::forceRedraw() {
    // Force le redessin du texte au prochain draw() (le visage est deja redessine
    // a chaque frame). Utile apres un fillScreen (rotation, fin de media...).
    _lastText = "";
}

void FaceRenderer::setEyeOffset(float x, float y) {
    _targetEyeOffsetX = constrain(x, -1.0f, 1.0f) * 15.0f;
    _targetEyeOffsetY = constrain(y, -1.0f, 1.0f) * 10.0f;
}

void FaceRenderer::triggerBlink() {
    if (!_isBlinking && _currentExpression != EXP_SLEEPING) {
        _isBlinking = true;
        _lastBlinkTime = millis();
    }
}

void FaceRenderer::setGlitch(bool active) {
    _glitchActive = active;
}

void FaceRenderer::setParty(bool on) {
    _partyMode = on;
    if (!on) _colorFace = 0x07FF; // retour cyan par defaut
}

void FaceRenderer::updateAnimations() {
    // 1. Smoothly interpolate eye offsets (lerp)
    _eyeOffsetX += (_targetEyeOffsetX - _eyeOffsetX) * 0.15f;
    _eyeOffsetY += (_targetEyeOffsetY - _eyeOffsetY) * 0.15f;
    
    // 2. Breathing animation (sinusoidal)
    _breathingCycle += 0.05f;
    if (_breathingCycle > 2 * PI) _breathingCycle -= 2 * PI;

    // 2b. Regard autonome : en idle, Beee balaye du regard tout seul (saccades).
    //     70% du temps il revient vers le centre -> air curieux mais pas agite.
    unsigned long gnow = millis();
    if (_currentExpression == EXP_IDLE && gnow - _lastGazeTime > 1400 + random(0, 2600)) {
        _lastGazeTime = gnow;
        if (random(0, 10) < 7) {
            _targetEyeOffsetX = (random(-60, 61) / 100.0f) * 15.0f;
            _targetEyeOffsetY = (random(-50, 51) / 100.0f) * 10.0f;
        } else {
            _targetEyeOffsetX = 0.0f; // recentre
            _targetEyeOffsetY = 0.0f;
        }
    }
    
    // 3. Auto-blinking logic
    unsigned long now = millis();
    if (_currentExpression != EXP_SLEEPING) {
        if (!_isBlinking && (now - _lastBlinkTime > 3000 + random(0, 4000))) {
            triggerBlink();
        }
        
        if (_isBlinking) {
            unsigned long elapsed = now - _lastBlinkTime;
            if (elapsed < _blinkDuration / 2) {
                // Closing
                _blinkRatio = 1.0f - ((float)elapsed / (_blinkDuration / 2.0f));
            } else if (elapsed < _blinkDuration) {
                // Opening
                _blinkRatio = ((float)(elapsed - _blinkDuration / 2) / (_blinkDuration / 2.0f));
            } else {
                // Done blinking
                _blinkRatio = 1.0f;
                _isBlinking = false;
                _lastBlinkTime = now;
            }
        }
    } else {
        _blinkRatio = 0.0f; // Kept closed in sleep mode
    }

    // 3b. Clin d'oeil independant (oeil droit), seulement en IDLE/HAPPY et hors clignement
    bool winkAllowed = (_currentExpression == EXP_IDLE || _currentExpression == EXP_HAPPY);
    if (winkAllowed) {
        if (!_isWinking && !_isBlinking && (now - _lastWinkTime > 6000 + random(0, 5000))) {
            _isWinking = true;
            _winkStartTime = now;
        }
        if (_isWinking) {
            unsigned long e = now - _winkStartTime;
            if (e < _winkDuration / 2) {
                _rightEyeRatio = 1.0f - ((float)e / (_winkDuration / 2.0f)); // fermeture
            } else if (e < _winkDuration) {
                _rightEyeRatio = (float)(e - _winkDuration / 2) / (_winkDuration / 2.0f); // ouverture
            } else {
                _rightEyeRatio = 1.0f;
                _isWinking = false;
                _lastWinkTime = now;
            }
        }
    } else {
        _rightEyeRatio = 1.0f;
        _isWinking = false;
    }

    // 4. Update Hacker lines
    if (_currentExpression == EXP_HACKER && now - _lastHackerUpdate > 400) {
        _lastHackerUpdate = now;
        _hackerDirty = true; // nouveau contenu -> autorise un redraw
        if (_hackerLineCount < 9) {
            const char* sampleCmds[] = {
                "TCP_CONNECT 127.0.0.1:8000",
                "WS_HANDSHAKE: OK",
                "EMOTION_ROUTER: ACTIVE",
                "SYSTEM_LOAD: 2.45%",
                "MEMORY_FREE: 6.82MB",
                "BEEE_CORE: PERCEPTIVE",
                "SHELL: /bin/zsh",
                "PING LOCALHOST: 1.2ms",
                "MEME_STREAMER: STANDBY",
                "SENSORS_CHECK: OK"
            };
            _hackerLines[_hackerLineCount] = String(">> ") + sampleCmds[random(0, 10)];
            _hackerLineCount++;
        } else {
            // Shift lines up
            for (int i = 0; i < 8; i++) {
                _hackerLines[i] = _hackerLines[i+1];
            }
            const char* sampleCmds[] = {
                "STATUS: NEURAL_ONLINE",
                "SENSORS: READING_TOUCH",
                "WS_STREAM: OK",
                "UI_RENDER: 60FPS",
                "CPU_TEMP: 42 C",
                "HEAP_CAP: 8192KB",
                "WIFI_RSSI: -52dBm"
            };
            _hackerLines[8] = String(">> ") + sampleCmds[random(0, 7)];
        }
    }
}

void FaceRenderer::draw() {
    updateAnimations();

    // Mode party : le visage cycle a travers une palette neon.
    if (_partyMode) {
        static const uint16_t party[] = {0x07FF, 0xF81F, 0xFFE0, 0x07E0, 0xFD20, 0x781F};
        _colorFace = party[(millis() / 120) % 6];
    }
    
    // Double-buffered styled drawing directly or using fast regional updates
    // In our case, we clear the face area and redraw it. Since we draw simple vectors,
    // we can redraw at 30-40fps with zero flicker if we do it fast.
    
    if (_currentExpression == EXP_HACKER) {
        drawHackerMode();
    } else {
        drawFace();
    }
    
    drawText();
}

void FaceRenderer::drawFace() {
    if (!_canvas) return;

    int centerX = SCREEN_WIDTH / 2;
    int centerY = 160 / 2; // Center in the 172x160 face area
    
    // Face breathing factor
    float breath = sin(_breathingCycle) * 2.0f;
    
    // Clear the face zone in RAM (on the canvas)
    _canvas->fillRect(0, 0, SCREEN_WIDTH, 160, _colorBackground);
    
    // Grid removed to prevent drawing flicker on direct rendering.
    
    // Glitch effect: offset rendering randomly
    int glitchX = 0;
    int glitchY = 0;
    if (_glitchActive || (_currentExpression == EXP_PANIC && random(0, 10) == 1)) {
        glitchX = random(-5, 6);
        glitchY = random(-2, 3);
        _canvas->fillRect(random(0, SCREEN_WIDTH), random(0, 160), random(10, 40), random(2, 8), _colorAccent);
    }
    
    // Draw Eyes
    int eyeDistance = 38;
    int leftEyeX = centerX - eyeDistance + _eyeOffsetX + glitchX;
    int rightEyeX = centerX + eyeDistance + _eyeOffsetX + glitchX;
    int eyeY = centerY - 10 + _eyeOffsetY + glitchY + breath;
    
    int eyeW = 22;
    int eyeH = 26;
    
    // Expressions modify eye shape and size
    switch (_currentExpression) {
        case EXP_HAPPY:
            // Curved upward arcs for happy eyes (drawn using overlapping rounded outlines or custom lines)
            for (int i = 0; i < 4; i++) {
                _canvas->drawArc(leftEyeX, eyeY + 5, 12 - i, 12 - i, 180, 360, _colorFace);
                _canvas->drawArc(rightEyeX, eyeY + 5, 12 - i, 12 - i, 180, 360, _colorFace);
            }
            break;
            
        case EXP_SAD:
            // Curved downward arcs for sad eyes
            for (int i = 0; i < 4; i++) {
                _canvas->drawArc(leftEyeX, eyeY - 2, 12 - i, 12 - i, 0, 180, _colorFace);
                _canvas->drawArc(rightEyeX, eyeY - 2, 12 - i, 12 - i, 0, 180, _colorFace);
            }
            break;
            
        case EXP_ANGRY:
            // Slanted eyes: diagonal thick lines
            for (int i = 0; i < 5; i++) {
                // Left eye slanted \
                _canvas->drawLine(leftEyeX - 10, eyeY - 8 + i, leftEyeX + 10, eyeY + 6 + i, _colorFace);
                // Right eye slanted /
                _canvas->drawLine(rightEyeX - 10, eyeY + 6 + i, rightEyeX + 10, eyeY - 8 + i, _colorFace);
            }
            break;
            
        case EXP_SLEEPING:
            // Flat sleeping lines: - -
            _canvas->fillRect(leftEyeX - 12, eyeY, 24, 4, _colorFace);
            _canvas->fillRect(rightEyeX - 12, eyeY, 24, 4, _colorFace);
            
            // Draw floating "Zzz"
            {
                int zCycle = (millis() / 500) % 3;
                int zY = eyeY - 20 - ((millis() / 30) % 25);
                int zX = rightEyeX + 10 + ((millis() / 40) % 15);
                _canvas->setTextColor(_colorAccent);
                _canvas->setTextSize(zCycle + 1);
                _canvas->setCursor(zX, zY);
                _canvas->print("Z");
                _canvas->setTextColor(_colorFace);
            }
            break;
            
        case EXP_PANIC:
            // Giant round circles that shake
            _canvas->fillCircle(leftEyeX, eyeY, 14 + random(-1, 2), _colorFace);
            _canvas->fillCircle(rightEyeX, eyeY, 14 + random(-1, 2), _colorFace);
            _canvas->fillCircle(leftEyeX, eyeY, 4, _colorBackground);
            _canvas->fillCircle(rightEyeX, eyeY, 4, _colorBackground);
            break;
            
        case EXP_SARCASMATIC:
            // Left eye normal, right eye squinted/smaller, looking to the side
            _canvas->fillRoundRect(leftEyeX - 10, eyeY - 13, 20, 26, 8, _colorFace);
            _canvas->fillRoundRect(rightEyeX - 10, eyeY - 6, 20, 12, 4, _colorFace);
            
            // Pupils looking right
            _canvas->fillCircle(leftEyeX + 4, eyeY, 4, _colorBackground);
            _canvas->fillCircle(rightEyeX + 4, eyeY, 2, _colorBackground);
            break;
            
        case EXP_THINKING:
            // Round eyes looking up
            _canvas->fillRoundRect(leftEyeX - 10, eyeY - 13, 20, 26, 8, _colorFace);
            _canvas->fillRoundRect(rightEyeX - 10, eyeY - 13, 20, 26, 8, _colorFace);
            
            // Pupils looking up-right
            _canvas->fillCircle(leftEyeX + 3, eyeY - 6, 4, _colorBackground);
            _canvas->fillCircle(rightEyeX + 3, eyeY - 6, 4, _colorBackground);
            
            // Floating question mark
            {
                int qY = eyeY - 25 + sin(_breathingCycle * 2) * 2;
                _canvas->setTextColor(_colorAccent);
                _canvas->setTextSize(2);
                _canvas->setCursor(rightEyeX + 15, qY);
                _canvas->print("?");
                _canvas->setTextColor(_colorFace);
            }
            break;
            
        case EXP_IDLE:
        default: {
            // Yeux arrondis. L'oeil gauche suit le clignement; l'oeil droit combine
            // clignement + clin d'oeil independant (_rightEyeRatio).
            int pupilOffsetX = _eyeOffsetX * 0.4f;
            int pupilOffsetY = _eyeOffsetY * 0.4f;

            int leftH  = eyeH * _blinkRatio;
            int rightH = eyeH * min(_blinkRatio, _rightEyeRatio);

            // Oeil gauche
            if (leftH > 2) {
                _canvas->fillRoundRect(leftEyeX - (eyeW / 2), eyeY - (leftH / 2), eyeW, leftH, 8, _colorFace);
                _canvas->fillCircle(leftEyeX + pupilOffsetX, eyeY + pupilOffsetY, 4, _colorBackground);
            } else {
                _canvas->fillRect(leftEyeX - (eyeW / 2), eyeY, eyeW, 2, _colorFace);
            }

            // Oeil droit
            if (rightH > 2) {
                _canvas->fillRoundRect(rightEyeX - (eyeW / 2), eyeY - (rightH / 2), eyeW, rightH, 8, _colorFace);
                _canvas->fillCircle(rightEyeX + pupilOffsetX, eyeY + pupilOffsetY, 4, _colorBackground);
            } else {
                // Clin d'oeil : trait incurve facon ">" pour un effet espiegle
                _canvas->fillRect(rightEyeX - (eyeW / 2), eyeY, eyeW, 2, _colorFace);
            }
            break;
        }
    }
    
    // Draw Mouth
    int mouthY = centerY + 25 + glitchY + breath * 0.5f;

    // Animation "il parle" : si un texte vient d'arriver, la bouche s'ouvre/ferme
    // rapidement (sur les expressions "parlantes"), sinon on garde la bouche normale.
    bool talking = (millis() < _talkingUntil) &&
                   (_currentExpression == EXP_IDLE || _currentExpression == EXP_HAPPY ||
                    _currentExpression == EXP_THINKING || _currentExpression == EXP_SARCASMATIC);
    if (talking) {
        float ph = sin((float)millis() * 0.025f);     // ~4 Hz
        int openH = 3 + (int)(fabs(ph) * 9.0f);       // 3..12 px d'ouverture
        _canvas->fillRoundRect(centerX - 9 + glitchX, mouthY - openH / 2, 18, openH, 3, _colorFace);
        if (openH > 5) {
            _canvas->fillRoundRect(centerX - 6 + glitchX, mouthY - openH / 4, 12, max(2, openH / 2), 2, _colorBackground);
        }
    } else
    switch (_currentExpression) {
        case EXP_HAPPY:
            // Big smile
            for(int i = 0; i < 3; i++) {
                _canvas->drawArc(centerX + glitchX, mouthY - 3, 14, 14, 0, 180, _colorFace);
                _canvas->drawArc(centerX + glitchX, mouthY - 3 - i, 14, 14, 0, 180, _colorFace);
            }
            break;
            
        case EXP_SAD:
            // Downward frown
            for(int i = 0; i < 3; i++) {
                _canvas->drawArc(centerX + glitchX, mouthY + 5, 12, 12, 180, 360, _colorFace);
                _canvas->drawArc(centerX + glitchX, mouthY + 5 + i, 12, 12, 180, 360, _colorFace);
            }
            break;
            
        case EXP_ANGRY:
            // Grrr flat or jagged mouth
            _canvas->fillRect(centerX - 10 + glitchX, mouthY, 20, 3, _colorFace);
            break;
            
        case EXP_SLEEPING:
            // Pulsing 'o' mouth
            {
                int r = 3 + sin(_breathingCycle) * 1.5f;
                if (r < 2) r = 2;
                _canvas->drawCircle(centerX + glitchX, mouthY, r, _colorFace);
            }
            break;
            
        case EXP_PANIC:
            // Big shocked 'O' mouth
            _canvas->fillCircle(centerX + glitchX, mouthY + 5, 8 + random(-1, 2), _colorFace);
            _canvas->fillCircle(centerX + glitchX, mouthY + 5, 5, _colorBackground);
            break;
            
        case EXP_SARCASMATIC:
            // Smirk: diagonal right-upwards line
            _canvas->drawLine(centerX - 10 + glitchX, mouthY + 2, centerX + 10 + glitchX, mouthY - 3, _colorFace);
            _canvas->drawLine(centerX + 6 + glitchX, mouthY - 3, centerX + 10 + glitchX, mouthY - 5, _colorFace); // corner
            break;
            
        case EXP_THINKING:
            // Wavy mouth
            for (int x = -12; x <= 12; x++) {
                int yOffset = sin((x / 4.0f) * PI) * 2;
                _canvas->drawPixel(centerX + x + glitchX, mouthY + yOffset, _colorFace);
                _canvas->drawPixel(centerX + x + glitchX, mouthY + yOffset + 1, _colorFace);
            }
            break;
            
        case EXP_IDLE:
        default:
            // Smooth small smile/line
            _canvas->fillRect(centerX - 8 + glitchX, mouthY, 16, 2, _colorFace);
            break;
    }
    
    // Flush completed canvas to the physical screen
    _canvas->flush();
}

void FaceRenderer::drawHackerMode() {
    // Anti-flicker : on ne redessine que quand le contenu a change (sinon l'ecran
    // etait efface+redessine a 30 fps -> clignotement du texte vert).
    if (!_hackerDirty) return;
    _hackerDirty = false;

    _gfx->fillScreen(_colorBackground);

    // Draw green matrix-like background or details
    _gfx->setTextColor(0x07E0); // Green
    _gfx->setTextSize(1);
    
    // Draw scrolling terminal text
    for (int i = 0; i < _hackerLineCount; i++) {
        _gfx->setCursor(5, 5 + i * 16);
        _gfx->print(_hackerLines[i]);
    }
    
    // Draw bottom status bar
    _gfx->fillRect(0, SCREEN_HEIGHT - 60, SCREEN_WIDTH, 2, 0x07E0);
    _gfx->setCursor(5, SCREEN_HEIGHT - 52);
    _gfx->print("SYSTEM: OVERLOADED");
    _gfx->setCursor(5, SCREEN_HEIGHT - 36);
    _gfx->print("MODE: CHIP_HACK_v1.0");
}

void FaceRenderer::drawText() {
    // If the expression is hacker, text is handled there
    if (_currentExpression == EXP_HACKER) return;
    
    // Draw dialogue/speech area at the bottom only when text changes
    if (_text != _lastText) {
        // Clear text area (bottom 160 pixels)
        _gfx->fillRect(0, 160, SCREEN_WIDTH, 160, _colorBackground);
        // Draw separation line at y=160
        _gfx->fillRect(0, 160, SCREEN_WIDTH, 2, 0x187F); // Cyber dark blue line
        
        _gfx->setTextSize(2);
        _gfx->setTextColor(_colorText);
        
        int xPos = 8;
        int yPos = 175; // Start rendering text below the line
        _gfx->setCursor(xPos, yPos);
        
        String word = "";
        int lineCharCount = 0;
        
        for (size_t i = 0; i < _text.length(); i++) {
            char c = _text[i];
            if (c == ' ' || c == '\n') {
                if (lineCharCount + word.length() > 12 || c == '\n') {
                    // Move to next line
                    yPos += 16;
                    if (yPos > SCREEN_HEIGHT - 10) break; // Exceeded screen height
                    _gfx->setCursor(xPos, yPos);
                    _gfx->print(word);
                    lineCharCount = word.length();
                } else {
                    _gfx->print(word);
                    _gfx->print(" ");
                    lineCharCount += word.length() + 1;
                }
                word = "";
            } else {
                word += c;
            }
        }
        
        if (word.length() > 0 && yPos <= SCREEN_HEIGHT - 10) {
            if (lineCharCount + word.length() > 12) {
                yPos += 16;
                if (yPos <= SCREEN_HEIGHT - 10) {
                    _gfx->setCursor(xPos, yPos);
                    _gfx->print(word);
                }
            } else {
                _gfx->print(word);
            }
        }
        
        _lastText = _text;
    }
}
