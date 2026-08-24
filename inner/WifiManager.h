#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <IPAddress.h>

// Неблокуюче STA-з'єднання з домашньою мережею для OTA/моніторингу/MQTT -
// повністю ІЗОЛЬОВАНЕ від Link (UartLink/EspNowLink), яким ідуть
// REQ/NONCE/AUTH/UNLOCK_OK. Втрата чи відсутність цього з'єднання ніяк не
// впливає на здатність відмикати замок.
class WifiManager {
public:
    WifiManager(const char* ssid, const char* password,
                IPAddress ip, IPAddress gateway, IPAddress subnet,
                uint32_t reconnectIntervalMs)
        : _ssid(ssid), _password(password), _ip(ip), _gateway(gateway),
          _subnet(subnet), _reconnectIntervalMs(reconnectIntervalMs) {}

    void begin() {
        WiFi.mode(WIFI_STA);
        WiFi.config(_ip, _gateway, _subnet);
        WiFi.begin(_ssid, _password);
        _lastAttemptMs = millis();
    }

    // Викликати щотік - неблокуюче перепідключення при розриві.
    void update() {
        if (WiFi.status() == WL_CONNECTED) {
            if (!_wasConnected) {
                Serial.print("[WIFI] connected, IP ");
                Serial.println(WiFi.localIP());
                _wasConnected = true;
            }
            return;
        }
        if (_wasConnected) {
            Serial.println("[WIFI] connection lost");
            _wasConnected = false;
        }
        uint32_t now = millis();
        if (now - _lastAttemptMs >= _reconnectIntervalMs) {
            WiFi.begin(_ssid, _password);
            _lastAttemptMs = now;
        }
    }

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

private:
    const char* _ssid;
    const char* _password;
    IPAddress _ip, _gateway, _subnet;
    uint32_t _reconnectIntervalMs;
    uint32_t _lastAttemptMs = 0;
    bool _wasConnected = false;
};
