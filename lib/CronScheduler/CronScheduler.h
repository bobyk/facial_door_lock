#pragma once
#include <Arduino.h>
#include <functional>

// Мінімальний "крон": викликає callback кожні intervalMs.
// Незалежний від RTC (працює на millis) - для точнішого прив'язування до дзиґарка
// можна замінити on tick() на порівняння RTCModule::now().
class CronScheduler {
public:
    using Callback = std::function<void()>;

    void every(uint32_t intervalMs, Callback cb) {
        _interval = intervalMs;
        _cb = cb;
        _last = millis();
    }

    void tick() {
        if (!_cb) return;
        uint32_t now = millis();
        if (now - _last >= _interval) {
            _last = now;
            _cb();
        }
    }

private:
    uint32_t _interval = 0;
    uint32_t _last = 0;
    Callback _cb;
};
