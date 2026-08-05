#pragma once
#include <FastLED.h>

// Обгортка над FastLED з готовими ефектами для UI замка.
// Виклики set*() лише виставляють режим, update() треба викликати в циклі (неблокуюче).
class LedStrip {
public:
    enum class Mode { OFF, PROGRESS, RUNNING_GREEN, BLINK_RED, RUNNING_RED };

    LedStrip(uint8_t pin, uint16_t numLeds);

    void begin();
    void update();                     // викликати кожен tick з loop()

    void setProgress(uint8_t enteredDigits, uint8_t totalDigits); // заповнення при вводі коду
    void setRunningGreen();            // успішне відкриття
    void setBlinkRed();                // 3 невдалі спроби -> треба пароль
    void setRunningRed();              // блокування вводу
    void setOff();

private:
    CRGB* _leds;
    uint16_t _numLeds;
    uint8_t _pin;
    Mode _mode = Mode::OFF;

    uint8_t _progressCount = 0, _progressTotal = 1;
    uint32_t _lastStep = 0;
    uint16_t _animPos = 0;
    bool _blinkOn = false;
};
