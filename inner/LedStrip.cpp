#include "LedStrip.h"
#include <FastLED.h>
#include "config.h"

static CRGB g_led[1];

void LedStrip::begin() {
    FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(g_led, 1);
    FastLED.clear(true);
}

void LedStrip::setOff() { _mode = Mode::OFF; }

void LedStrip::setBlinkRed() {
    _mode = Mode::BLINK_RED;
    _lastBlinkMs = millis();
    _blinkOn = true;
}

void LedStrip::update() {
    uint32_t now = millis();
    switch (_mode) {
        case Mode::OFF:
            g_led[0] = CRGB::Black;
            break;

        case Mode::BLINK_RED:
            if (now - _lastBlinkMs >= 300) {
                _blinkOn = !_blinkOn;
                _lastBlinkMs = now;
            }
            g_led[0] = _blinkOn ? CRGB(60, 0, 0) : CRGB::Black;
            break;
    }
    FastLED.show();
}
