#ifndef FACE_RENDERER_H
#define FACE_RENDERER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "config.h"

enum Expression {
    EXP_IDLE,
    EXP_HAPPY,
    EXP_SAD,
    EXP_ANGRY,
    EXP_SARCASMATIC,
    EXP_THINKING,
    EXP_SLEEPING,
    EXP_PANIC,
    EXP_HACKER
};

class FaceRenderer {
public:
    FaceRenderer(Arduino_GFX *gfx);
    void begin();
    void update();
    void draw();
    
    void setExpression(Expression exp);
    void setText(const String &text);
    void forceRedraw();                 // force un redessin complet (texte inclus)
    void triggerBlink();
    void setEyeOffset(float x, float y); // x, y from -1.0 to 1.0
    void setGlitch(bool active);
    void setParty(bool on);              // mode party : cycle de couleurs neon
    
private:
    Arduino_GFX *_gfx;
    Arduino_Canvas *_canvas;
    Expression _currentExpression;
    String _text;
    String _lastText;
    
    // Animation states
    float _eyeOffsetX, _eyeOffsetY;       // Current eye offset
    float _targetEyeOffsetX, _targetEyeOffsetY; // Target eye offset
    float _blinkRatio;                   // 1.0 = open, 0.0 = closed
    bool _isBlinking;
    unsigned long _lastBlinkTime;
    unsigned long _blinkDuration;
    unsigned long _expressionStartTime;

    // Clin d'oeil independant (oeil droit uniquement)
    float _rightEyeRatio;                // 1.0 = ouvert, 0.0 = ferme (clin d'oeil)
    bool _isWinking;
    unsigned long _winkStartTime;
    unsigned long _lastWinkTime;
    unsigned long _winkDuration;

    // Animation "il parle" (bouche) : actif tant que millis() < _talkingUntil
    unsigned long _talkingUntil;
    
    // Breathing animation
    float _breathingCycle; // 0.0 to 2*PI

    // Regard autonome (saccades) en idle : le regard balaye tout seul
    unsigned long _lastGazeTime;
    
    // Hacker mode state
    String _hackerLines[10];
    int _hackerLineCount;
    unsigned long _lastHackerUpdate;
    bool _hackerDirty;            // ne redessine l'ecran hacker que quand le texte change
    
    // Glitch status
    bool _glitchActive;
    bool _partyMode;
    
    void drawFace();
    void drawText();
    void drawHackerMode();
    void updateAnimations();
    
    // Colors
    uint16_t _colorBackground;
    uint16_t _colorFace;
    uint16_t _colorAccent;
    uint16_t _colorText;
};

#endif // FACE_RENDERER_H
