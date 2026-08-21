#pragma once
#include <Arduino.h>
#include <RTClib.h>

// DS3231 - лише для міток часу в логах безпеки (auth success/fail, tamper,
// pairing, lockout). Не бере участі у прийнятті рішень FSM.
class RTCModule {
public:
    bool begin();
    // Формат "YYYY-MM-DD hh:mm:ss" у переданий буфер (мін. 20 байт).
    void formatTimestamp(char* out, size_t outLen);

private:
    RTC_DS3231 _rtc;
    bool _ok = false;
};
