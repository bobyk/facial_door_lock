#include "RTCModule.h"

bool RTCModule::begin() {
    _ok = _rtc.begin();
    return _ok;
}

DateTime RTCModule::now() {
    return _rtc.now();
}

bool RTCModule::lostPower() {
    return _ok && _rtc.lostPower();
}

void RTCModule::adjust(const DateTime& dt) {
    if (_ok) _rtc.adjust(dt);
}
