#pragma once
#include <Arduino.h>

// Неблокуючий індикатор стану на єдиному адресному пікселі плати (RGB_LED,
// WS2812-сумісний, GPIO42 - не стрічка). Пін фіксується як шаблонний параметр
// FastLED на етапі компіляції (LedStrip.cpp includes config.h напряму для
// цього) - не проблема тут, бо LedStrip не залежить від бізнес-логіки
// OuterController, лише від фізичного піна індикатора.
class LedStrip {
public:
    LedStrip() = default;

    void begin();
    void update(); // викликати щотік з loop() - анімує без delay()

    void setOff();
    void setScanning();                              // сканування обличчя - тьмяний синій
    void setProgress(uint8_t entered, uint8_t total); // ввід PIN - заповнення бурштиновим
    void setBlinkRed();                               // відмова/увага - миготіння червоним
    void setRunningGreen();                            // unlock - короткий зелений спалах
    void setTamperAlert();                              // тампер - швидке миготіння червоним

private:
    enum class Mode { OFF, SCANNING, PROGRESS, BLINK_RED, RUNNING_GREEN, TAMPER };

    Mode _mode = Mode::OFF;
    uint8_t _progressEntered = 0, _progressTotal = 1;
    uint32_t _animStart = 0;
    bool _blinkOn = false;
    uint32_t _lastBlinkMs = 0;
};
