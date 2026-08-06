#pragma once
#include <RTClib.h>

// Обгортка над DS3231. Не залежить від решти проєкту -> переносима.
class RTCModule {
public:
    bool begin();                 // ініціалізація, true якщо модуль знайдено
    DateTime now();                // поточний час
    bool lostPower();              // true якщо був відрив живлення (треба виставити час)
    void adjust(const DateTime& dt);

private:
    RTC_DS3231 _rtc;
    bool _ok = false;
};
