#include "LedStrip.h"
#include "Config.h"

LedStrip::LedStrip(uint8_t pin, uint16_t numLeds) : _pin(pin), _numLeds(numLeds) {
    _leds = new CRGB[numLeds];
}

void LedStrip::begin() {
    // Пін фіксований в Config.h (LED_DATA_PIN) - обмеження FastLED, шаблонний параметр.
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(_leds, _numLeds);
    FastLED.setBrightness(120);
    setOff();
}

void LedStrip::setProgress(uint8_t enteredDigits, uint8_t totalDigits) {
    _mode = Mode::PROGRESS;
    _progressCount = enteredDigits;
    _progressTotal = totalDigits ? totalDigits : 1;
}

void LedStrip::setRunningGreen() { _mode = Mode::RUNNING_GREEN; _animPos = 0; _lastStep = 0; }
void LedStrip::setBlinkRed()     { _mode = Mode::BLINK_RED; _blinkOn = false; _lastStep = 0; }
void LedStrip::setRunningRed()   { _mode = Mode::RUNNING_RED; _animPos = 0; _lastStep = 0; }
void LedStrip::setOff()          { _mode = Mode::OFF; fill_solid(_leds, _numLeds, CRGB::Black); FastLED.show(); }

void LedStrip::update() {
    uint32_t t = millis();

    switch (_mode) {
        case Mode::OFF:
            break;

        case Mode::PROGRESS: {
            fill_solid(_leds, _numLeds, CRGB::Black);
            uint16_t lit = (uint32_t)_numLeds * _progressCount / _progressTotal;
            for (uint16_t i = 0; i < lit; i++) _leds[i] = CRGB::Blue;
            FastLED.show();
            break;
        }

        case Mode::RUNNING_GREEN: {
            if (t - _lastStep < 30) return;
            _lastStep = t;
            fill_solid(_leds, _numLeds, CRGB::Black);
            for (uint16_t i = 0; i <= _animPos && i < _numLeds; i++) _leds[i] = CRGB::Green;
            FastLED.show();
            if (_animPos < _numLeds) _animPos++;
            else _mode = Mode::OFF; // анімація завершена -> замок відкрито, гасимо
            break;
        }

        case Mode::BLINK_RED: {
            if (t - _lastStep < 400) return;
            _lastStep = t;
            _blinkOn = !_blinkOn;
            fill_solid(_leds, _numLeds, _blinkOn ? CRGB::Red : CRGB::Black);
            FastLED.show();
            break;
        }

        case Mode::RUNNING_RED: {
            if (t - _lastStep < 40) return;
            _lastStep = t;
            fill_solid(_leds, _numLeds, CRGB::Black);
            _leds[_animPos % _numLeds] = CRGB::Red;
            FastLED.show();
            _animPos++;
            break;
        }
    }
}
