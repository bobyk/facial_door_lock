// INNER board: RTC, motor lock driver, internal TOF400C presence sensor.
// The ONLY board with an electrical path to the lock, and the sole authority
// that decides whether to drive it.
// Board: Ozobot DRVKit (esp32:esp32:ozobot_drvkit), same as OUTER. Requires
// Tools > Pin Numbering: "By GPIO number (legacy)" - see config.h for why.
// Libraries via Library Manager (Sketch > Include Library > Manage Libraries):
//   - RTClib (Adafruit)
//   - VL53L1X (Pololu)
//   - PubSubClient (Nick O'Leary) - MQTT status publishing only
//   - FastLED - onboard status LED (pairing indicator)
// (mbedtls/esp_random/bootloader_random/esp_now/WiFi/WebServer/ArduinoOTA ship
// with the ESP32 core, no install needed.)
#include <Arduino.h>
#include <Wire.h>
#include <WiFiClient.h>
#include "config.h"
#include "UartLink.h"
#include "EspNowLink.h"
#include "TransportProbe.h"
#include "CryptoMbedTLS.h"
#include "NvsStore.h"
#include "LockDriver.h"
#include "ToFPresenceSensor.h"
#include "RTCModule.h"
#include "EventLog.h"
#include "LedStrip.h"
#include "InnerController.h"
#include "WifiManager.h"
#include "OtaUpdater.h"
#include "StatusServer.h"
#include "MqttReporter.h"

UartLink uartLink(Serial2, LINK_RX_PIN, LINK_TX_PIN, LINK_ALIVE_TIMEOUT_MS, LINK_UART_BAUD);
EspNowLink* espnowLink = nullptr;
Link* activeLink = nullptr;

CryptoMbedTLS crypto;
NvsStore nvs;
LockDriver lock(MOTOR_IN1_PIN, MOTOR_IN2_PIN, MOTOR_PULSE_MS);
ToFPresenceSensor tof(TOF_PRESENCE_THRESHOLD_MM);
RTCModule rtc;
EventLog eventLog;
LedStrip led;
InnerController* controller = nullptr;

// --- Auxiliary home Wi-Fi (OTA / monitoring / MQTT) - entirely separate from
// the Link/crypto path above. See config.h for the channel-conflict caveat
// when running on ESP-NOW transport, and MqttReporter.h/StatusServer.h for
// why these surfaces are read-only. ---
WifiManager wifiManager(WIFI_HOME_SSID, WIFI_HOME_PASSWORD,
                         IPAddress(WIFI_STATIC_IP), IPAddress(WIFI_STATIC_GATEWAY),
                         IPAddress(WIFI_STATIC_SUBNET), WIFI_RECONNECT_INTERVAL_MS);
OtaUpdater ota(OTA_HOSTNAME, OTA_PASSWORD);
StatusServer statusServer(nvs, eventLog, STATUS_SERVER_PORT);
WiFiClient mqttWifiClient;
MqttReporter mqtt(mqttWifiClient, MQTT_BROKER_HOST, MQTT_BROKER_PORT, MQTT_CLIENT_ID,
                   MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC_PREFIX, nvs,
                   MQTT_PUBLISH_INTERVAL_MS, MQTT_RECONNECT_INTERVAL_MS);
uint32_t lastForwardedEventCount = 0;

// Forwards any new EventLog lines to MQTT since the last check - keeps
// MqttReporter decoupled from InnerController (it only reads EventLog/NvsStore).
void forwardNewEventsToMqtt() {
    uint32_t total = eventLog.totalAdded();
    if (total == lastForwardedEventCount) return;
    char lines[EventLog::CAPACITY][EventLog::LINE_LEN];
    uint8_t n = eventLog.recent(lines, EventLog::CAPACITY);
    uint32_t newCount = total - lastForwardedEventCount;
    if (newCount > n) newCount = n; // ring buffer wrapped past what we could forward
    for (uint8_t i = n - (uint8_t)newCount; i < n; ++i) mqtt.publishEvent(lines[i]);
    lastForwardedEventCount = total;
}

// Дзеркало логіки в outer.ino - див. коментар там. Обидві плати мають прийти
// до ОДНАКОВОГО вибору транспорту незалежно (UART/ESPNOW - детерміновано з
// NVS; AUTO - симетричний PING/PONG пробінг, узгоджується сам собою).
void selectTransport() {
    TransportMode mode = nvs.transportMode();

    if (mode == TransportMode::UART) {
        uartLink.begin();
        activeLink = &uartLink;
        return;
    }
    if (mode == TransportMode::ESPNOW) {
        espnowLink = new EspNowLink(nvs.peerMac(), nvs.wifiChannel(), LINK_ALIVE_TIMEOUT_MS);
        espnowLink->begin();
        activeLink = espnowLink;
        return;
    }

    if (probeUartPeer(uartLink)) {
        activeLink = &uartLink;
    } else {
        espnowLink = new EspNowLink(nvs.peerMac(), nvs.wifiChannel(), LINK_ALIVE_TIMEOUT_MS);
        espnowLink->begin();
        activeLink = espnowLink;
    }
}

// Неблокуюче накопичення рядка команди по байтах - Serial.readStringUntil()
// може блокувати loop() до свого таймауту, якщо решта рядка ще в дорозі.
void handleSerialCommand() {
    static String line;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c != '\n') { line += c; continue; }

        line.trim();
        if (line == "transport uart") {
            nvs.setTransportMode(TransportMode::UART);
            Serial.println("[CMD] transport pinned to UART - reboot to apply");
        } else if (line == "transport espnow") {
            nvs.setTransportMode(TransportMode::ESPNOW);
            Serial.println("[CMD] transport pinned to ESP-NOW - reboot to apply");
        } else if (line == "transport auto") {
            nvs.setTransportMode(TransportMode::AUTO);
            Serial.println("[CMD] transport set to AUTO (bench/debug only) - reboot to apply");
        } else if (line.length() > 0) {
            Serial.println("[CMD] unknown command (try: transport uart|espnow|auto)");
        }
        line = "";
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    nvs.begin(NVS_NAMESPACE);

    selectTransport();

    controller = new InnerController(*activeLink, crypto, nvs, lock, tof, rtc, MAINT_BUTTON_PIN, eventLog, led);
    controller->begin();

    // ESP-NOW and Wi-Fi STA share one radio and must sit on the same channel.
    // Joining the home AP would force ESP-NOW onto whatever channel THAT AP
    // uses, which silently breaks the OUTER<->INNER Link (pairing broadcasts,
    // heartbeats, everything) - so the aux Wi-Fi stack stays off entirely
    // while running on ESP-NOW. It only comes up on UART transport, which
    // doesn't touch the radio at all. See config.h/WIRING.md.
    if (activeLink == &uartLink) {
        wifiManager.begin();
        statusServer.begin();
        mqtt.begin();
    } else {
        Serial.println("[WIFI] skipped: Link transport is ESP-NOW - home Wi-Fi would break it "
                        "(shared radio channel). Run `transport uart` to enable OTA/monitoring/MQTT.");
    }
}

void loop() {
    controller->update();
    handleSerialCommand();

    // Auxiliary network stack - independent of the loop above; any failure or
    // absence here has no effect on Link/auth/lock behaviour.
    wifiManager.update();
    bool wifiUp = wifiManager.isConnected();
    ota.update(wifiUp);
    if (wifiUp) statusServer.update();
    mqtt.update(wifiUp);
    forwardNewEventsToMqtt();
}
