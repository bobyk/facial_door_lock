// OUTER board: face module, scan button, tamper switch, presence radar, keypad, LED.
// No electrical path to the lock - every unlock decision is made by INNER.
// Board: Ozobot DRVKit (esp32:esp32:ozobot_drvkit). Requires Tools > Pin
// Numbering: "By GPIO number (legacy)" - see config.h for why.
// Libraries via Library Manager (Sketch > Include Library > Manage Libraries):
//   - FastLED
// (VL53L1X/RTClib are NOT needed on this board - those are INNER-only.
//  mbedtls/esp_random/bootloader_random/esp_now ship with the ESP32 core, no install needed.)
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "UartLink.h"
#include "EspNowLink.h"
#include "TransportProbe.h"
#include "CryptoMbedTLS.h"
#include "NvsStore.h"
#include "Frm1213Driver.h"
#include "PresenceSensor.h"
#include "LD2420PresenceSensor.h"
#include "KeypadPCF8574.h"
#include "LedStrip.h"
#include "OuterController.h"

UartLink uartLink(Serial2, LINK_RX_PIN, LINK_TX_PIN, LINK_ALIVE_TIMEOUT_MS, LINK_UART_BAUD);
EspNowLink* espnowLink = nullptr; // сконструюємо в setup() лише якщо потрібен - peerMac відомий тільки після nvs.begin()
Link* activeLink = nullptr;

CryptoMbedTLS crypto;
NvsStore nvs;
Frm1213Driver face(Serial1, FACE_RX_PIN, FACE_TX_PIN,
                    FRM1213_VERIFY_TIMEOUT_MS, FRM1213_RESET_ACK_TIMEOUT_MS, FRM1213_RESET_READY_WAIT_MS,
                    FACE_UART_BAUD);
LD2420PresenceSensor presence(Serial0, PRESENCE_RX_PIN, PRESENCE_TX_PIN, PRESENCE_UART_BAUD);
KeypadPCF8574 keypad(PCF8574_ADDR);
LedStrip led;
OuterController* controller = nullptr;

// Обирає транспорт за transport_mode з NVS. AUTO - лише для стенду/дебагу:
// перерізаний кабель тихо понижує систему до радіо, що суперечить моделі
// "фізичний доступ до кабелю = довіра" - тому для розгортання транспорт
// треба явно закріпити командою `transport uart` (документовано в WIRING.md).
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

    // AUTO
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
    keypad.begin(Wire);

    selectTransport();

    controller = new OuterController(*activeLink, crypto, nvs, face, presence, keypad, led,
                                      SCAN_BUTTON_PIN, TAMPER_PIN);
    controller->begin();
}

void loop() {
    controller->update();
    handleSerialCommand();
}
