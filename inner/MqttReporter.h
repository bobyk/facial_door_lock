#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "NvsStore.h"

// Публікує стан для Home Assistant - НІКОЛИ не підписується на жоден топік.
// Це навмисне рішення: топіку виду ".../unlock" немає, тому навіть повністю
// скомпрометований MQTT-брокер чи Home Assistant не може відкрити замок -
// лише InnerController::handleAuth() (через REQ/NONCE/AUTH) викликає
// _lock.open(). MqttReporter лише читає NvsStore (const-геттери), про
// InnerController не знає взагалі.
class MqttReporter {
public:
    MqttReporter(WiFiClient& wifiClient, const char* host, uint16_t port,
                 const char* clientId, const char* username, const char* password,
                 const char* topicPrefix, NvsStore& nvs,
                 uint32_t publishIntervalMs, uint32_t reconnectIntervalMs)
        : _client(wifiClient), _host(host), _port(port), _clientId(clientId),
          _username(username), _password(password), _topicPrefix(topicPrefix), _nvs(nvs),
          _publishIntervalMs(publishIntervalMs), _reconnectIntervalMs(reconnectIntervalMs) {}

    void begin() {
        _client.setServer(_host, _port);
        // PubSubClient::connect() blocks on the underlying socket until this
        // timeout - left at its 15s default, an unreachable broker would
        // stall loop() (and with it InnerController::update(), including
        // maintenance-button hold detection) for that long on every retry.
        // 1s keeps the worst case well under the shortest thing this shares
        // a loop with (the 2s pairing-hold gesture).
        _client.setSocketTimeout(1);
    }

    // Викликати щотік лише коли Wi-Fi підключено - неблокуючий reconnect +
    // періодична публікація стану.
    void update(bool wifiConnected) {
        if (!wifiConnected) return;

        if (!_client.connected()) {
            uint32_t now = millis();
            if (now - _lastReconnectAttemptMs >= _reconnectIntervalMs) {
                _lastReconnectAttemptMs = now;
                bool ok = (_username && _username[0])
                    ? _client.connect(_clientId, _username, _password)
                    : _client.connect(_clientId);
                if (ok) {
                    Serial.println("[MQTT] connected");
                    publishStatus();
                } else {
                    Serial.print("[MQTT] connect failed, rc=");
                    Serial.println(_client.state());
                }
            }
            return;
        }

        _client.loop();

        uint32_t now = millis();
        if (now - _lastPublishMs >= _publishIntervalMs) {
            _lastPublishMs = now;
            publishStatus();
        }
    }

    // Викликати з InnerController-адаптера (див. inner.ino) щоб надіслати
    // останню подію одразу, а не чекати наступного періодичного publishStatus().
    void publishEvent(const char* msg) {
        if (!_client.connected()) return;
        char topic[64];
        snprintf(topic, sizeof(topic), "%s/event", _topicPrefix);
        _client.publish(topic, msg);
    }

private:
    PubSubClient _client;
    const char* _host;
    uint16_t _port;
    const char* _clientId;
    const char* _username;
    const char* _password;
    const char* _topicPrefix;
    NvsStore& _nvs;
    uint32_t _publishIntervalMs;
    uint32_t _reconnectIntervalMs;
    uint32_t _lastReconnectAttemptMs = 0;
    uint32_t _lastPublishMs = 0;

    void publishStatus() {
        char topic[64];
        char payload[16];

        snprintf(topic, sizeof(topic), "%s/paired", _topicPrefix);
        _client.publish(topic, _nvs.isPaired() ? "ON" : "OFF", true);

        snprintf(topic, sizeof(topic), "%s/locked_out", _topicPrefix);
        _client.publish(topic, _nvs.lockedOut() ? "ON" : "OFF", true);

        snprintf(topic, sizeof(topic), "%s/generation", _topicPrefix);
        snprintf(payload, sizeof(payload), "%lu", (unsigned long)_nvs.generation());
        _client.publish(topic, payload, true);

        snprintf(topic, sizeof(topic), "%s/counter", _topicPrefix);
        snprintf(payload, sizeof(payload), "%lu", (unsigned long)_nvs.counter());
        _client.publish(topic, payload, true);
    }
};
