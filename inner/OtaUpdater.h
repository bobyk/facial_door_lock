#pragma once
#include <Arduino.h>
#include <ArduinoOTA.h>

// Тонка обгортка над ArduinoOTA. begin() безпечно викликати повторно щотік
// до підключення Wi-Fi - реально стартує лише один раз, коли з'єднання вже є.
class OtaUpdater {
public:
    OtaUpdater(const char* hostname, const char* password)
        : _hostname(hostname), _password(password) {}

    // Викликати щотік; фактичний ArduinoOTA.begin() відбувається лише коли
    // wifiConnected стане true вперше.
    void update(bool wifiConnected) {
        if (!_started) {
            if (!wifiConnected) return;
            ArduinoOTA.setHostname(_hostname);
            ArduinoOTA.setPassword(_password);
            ArduinoOTA.begin();
            Serial.println("[OTA] ready");
            _started = true;
        }
        ArduinoOTA.handle();
    }

private:
    const char* _hostname;
    const char* _password;
    bool _started = false;
};
