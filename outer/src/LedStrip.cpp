#include "LedStrip.h"
#include <FastLED.h>
#include "config.h"

static CRGB g_leds[LED_NUM];

void LedStrip::begin() {
    FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(g_leds, _count);
    FastLED.clear(true);
}

void LedStrip::setOff() { _mode = Mode::OFF; }

void LedStrip::setScanning() { _mode = Mode::SCANNING; }

void LedStrip::setProgress(uint8_t entered, uint8_t total) {
    _mode = Mode::PROGRESS;
    _progressEntered = entered;
    _progressTotal = total ? total : 1;
}

void LedStrip::setBlinkRed() {
    _mode = Mode::BLINK_RED;
    _lastBlinkMs = millis();
    _blinkOn = true;
}

void LedStrip::setRunningGreen() {
    _mode = Mode::RUNNING_GREEN;
    _animStart = millis();
}

void LedStrip::setTamperAlert() {
    _mode = Mode::TAMPER;
    _lastBlinkMs = millis();
    _blinkOn = true;
}

void LedStrip::update() {
    uint32_t now = millis();
    switch (_mode) {
        case Mode::OFF:
            fill_solid(g_leds, _count, CRGB::Black);
            break;

        case Mode::SCANNING:
            fill_solid(g_leds, _count, CRGB(0, 0, 40));
            break;

        case Mode::PROGRESS: {
            uint16_t lit = (uint16_t)((uint32_t)_count * _progressEntered / _progressTotal);
            for (uint16_t i = 0; i < _count; ++i) g_leds[i] = (i < lit) ? CRGB(60, 40, 0) : CRGB::Black;
            break;
        }

        case Mode::BLINK_RED:
            if (now - _lastBlinkMs >= 300) {
                _blinkOn = !_blinkOn;
                _lastBlinkMs = now;
            }
            fill_solid(g_leds, _count, _blinkOn ? CRGB(60, 0, 0) : CRGB::Black);
            break;

        case Mode::TAMPER:
            if (now - _lastBlinkMs >= 100) { // швидше за BLINK_RED - явно інша подія
                _blinkOn = !_blinkOn;
                _lastBlinkMs = now;
            }
            fill_solid(g_leds, _count, _blinkOn ? CRGB(80, 0, 0) : CRGB::Black);
            break;

        case Mode::RUNNING_GREEN: {
            uint32_t elapsed = now - _animStart;
            const uint32_t stepMs = 40;
            uint16_t head = (uint16_t)(elapsed / stepMs);
            fill_solid(g_leds, _count, CRGB::Black);
            for (uint16_t i = 0; i < _count && i <= head; ++i) g_leds[i] = CRGB(0, 60, 0);
            if (head > _count) _mode = Mode::OFF; // одна хвиля - і повертаємось у стан очікування
            break;
        }
    }
    FastLED.show();
}
