#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "NvsStore.h"
#include "EventLog.h"

// Лише читання: GET /status (JSON стан) і GET /logs (останні події з
// EventLog). Навмисно НЕМАЄ жодного маршруту, який міг би відкрити замок чи
// змінити стан - мережева поверхня не повинна ставати обхідним шляхом
// авторизації повз REQ/NONCE/AUTH. NvsStore/EventLog передаються сюди лише
// для читання (const-геттери), InnerController про StatusServer не знає.
class StatusServer {
public:
    StatusServer(NvsStore& nvs, EventLog& log, uint16_t port)
        : _nvs(nvs), _log(log), _server(port) {}

    void begin() {
        _server.on("/", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/status", HTTP_GET, [this]() { handleStatus(); });
        _server.on("/logs", HTTP_GET, [this]() { handleLogs(); });
        _server.begin();
        Serial.println("[HTTP] status server ready");
    }

    void update() { _server.handleClient(); }

private:
    NvsStore& _nvs;
    EventLog& _log;
    WebServer _server;

    void handleRoot() {
        _server.send(200, "text/plain",
            "inner-lock status server (read-only)\nGET /status\nGET /logs\n");
    }

    void handleStatus() {
        char body[256];
        snprintf(body, sizeof(body),
            "{\"paired\":%s,\"locked_out\":%s,\"generation\":%lu,\"counter\":%lu,"
            "\"uptime_ms\":%lu}",
            _nvs.isPaired() ? "true" : "false",
            _nvs.lockedOut() ? "true" : "false",
            (unsigned long)_nvs.generation(),
            (unsigned long)_nvs.counter(),
            (unsigned long)millis());
        _server.send(200, "application/json", body);
    }

    void handleLogs() {
        char lines[EventLog::CAPACITY][EventLog::LINE_LEN];
        uint8_t n = _log.recent(lines, EventLog::CAPACITY);

        String body = "[";
        for (uint8_t i = 0; i < n; ++i) {
            if (i > 0) body += ',';
            body += '"';
            for (const char* p = lines[i]; *p; ++p) {
                if (*p == '"' || *p == '\\') body += '\\';
                body += *p;
            }
            body += '"';
        }
        body += ']';
        _server.send(200, "application/json", body);
    }
};
