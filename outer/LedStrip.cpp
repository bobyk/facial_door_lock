#include "LedStrip.h"
#include <FastLED.h>
#include "config.h"

// Один-єдиний адресний піксель на платі (RGB_LED, GPIO42) - не стрічка.
// Публічний інтерфейс (setProgress/setRunningGreen тощо) лишається таким же,
// як був би для стрічки, щоб OuterController не залежав від фізичної форми
// індикатора; тут ці режими виражені через яскравість/тривалість одного пікселя
// замість "заповнення N з count" чи "хвилі по довжині".
static CRGB g_led[1];
static constexpr uint32_t UNLOCK_FLASH_MS = 800; // тривалість підтвердження unlock (заміна "хвилі" для 1 пікселя)

void LedStrip::begin() {
    FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(g_led, 1);
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
            g_led[0] = CRGB::Black;
            break;

        case Mode::SCANNING:
            g_led[0] = CRGB(0, 0, 40);
            break;

        case Mode::PROGRESS: {
            // Яскравість пропорційна введеним/загальним цифрам - аналог "заповнення" для 1 пікселя.
            uint8_t brightness = (uint8_t)((uint16_t)60 * _progressEntered / _progressTotal);
            g_led[0] = CRGB(brightness, (uint8_t)(brightness * 2 / 3), 0); // бурштиновий відтінок
            break;
        }

        case Mode::BLINK_RED:
            if (now - _lastBlinkMs >= 300) {
                _blinkOn = !_blinkOn;
                _lastBlinkMs = now;
            }
            g_led[0] = _blinkOn ? CRGB(60, 0, 0) : CRGB::Black;
            break;

        case Mode::TAMPER:
            if (now - _lastBlinkMs >= 100) { // швидше за BLINK_RED - явно інша подія
                _blinkOn = !_blinkOn;
                _lastBlinkMs = now;
            }
            g_led[0] = _blinkOn ? CRGB(80, 0, 0) : CRGB::Black;
            break;

        case Mode::RUNNING_GREEN:
            if (now - _animStart >= UNLOCK_FLASH_MS) {
                _mode = Mode::OFF; // короткий спалах підтвердження - і повертаємось у стан очікування
                g_led[0] = CRGB::Black;
            } else {
                g_led[0] = CRGB(0, 60, 0);
            }
            break;
    }
    FastLED.show();
}
