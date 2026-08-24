#pragma once
#include <Arduino.h>

// Неблокуючий індикатор на єдиному адресному пікселі плати (RGB_LED,
// WS2812-сумісний, GPIO47 - не стрічка, той самий фізичний піксель що й на
// OUTER). INNER має лише один режим індикації - миготіння червоним під час
// PAIRING - на відміну від OUTER, де індикатор відображає весь хід
// face/PIN-авторизації.
class LedStrip {
public:
    LedStrip() = default;

    void begin();
    void update(); // викликати щотік з loop() - анімує без delay()

    void setOff();
    void setBlinkRed(); // PAIRING - миготіння червоним

private:
    enum class Mode { OFF, BLINK_RED };

    Mode _mode = Mode::OFF;
    bool _blinkOn = false;
    uint32_t _lastBlinkMs = 0;
};
