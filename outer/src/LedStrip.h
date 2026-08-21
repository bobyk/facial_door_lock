#pragma once
#include <Arduino.h>

// Неблокуючий індикатор стану на WS2812 (FastLED). Пін фіксується як
// шаблонний параметр FastLED на етапі компіляції (LedStrip.cpp includes
// Config.h напряму для цього) - не проблема тут, бо LedStrip не залежить
// від бізнес-логіки OuterController, лише від фізичного піна стрічки.
class LedStrip {
public:
    explicit LedStrip(uint16_t count) : _count(count) {}

    void begin();
    void update(); // викликати щотік з loop() - анімує без delay()

    void setOff();
    void setScanning();                              // сканування обличчя - тьмяний синій
    void setProgress(uint8_t entered, uint8_t total); // ввід PIN - заповнення бурштиновим
    void setBlinkRed();                               // відмова/увага - миготіння червоним
    void setRunningGreen();                            // unlock - зелена хвиля знизу вгору
    void setTamperAlert();                              // тампер - швидке миготіння червоним

private:
    enum class Mode { OFF, SCANNING, PROGRESS, BLINK_RED, RUNNING_GREEN, TAMPER };

    uint16_t _count;
    Mode _mode = Mode::OFF;
    uint8_t _progressEntered = 0, _progressTotal = 1;
    uint32_t _animStart = 0;
    bool _blinkOn = false;
    uint32_t _lastBlinkMs = 0;
};
