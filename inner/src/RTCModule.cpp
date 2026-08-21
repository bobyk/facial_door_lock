#include "RTCModule.h"
#include <stdio.h>

bool RTCModule::begin() {
    _ok = _rtc.begin();
    if (_ok && _rtc.lostPower()) {
        // Час невідомий (перше живлення / розряджена батарея резервного живлення) -
        // виставляємо час компіляції прошивки як заглушку, щоб мітки часу хоч
        // монотонно росли; точний час треба виставити окремо (не входить в це ТЗ).
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return _ok;
}

void RTCModule::formatTimestamp(char* out, size_t outLen) {
    if (!_ok) {
        snprintf(out, outLen, "no-rtc");
        return;
    }
    DateTime now = _rtc.now();
    snprintf(out, outLen, "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
}
