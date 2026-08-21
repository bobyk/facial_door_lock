// INNER board: RTC, motor lock driver, internal TOF400C presence sensor.
// The ONLY board with an electrical path to the lock, and the sole authority
// that decides whether to drive it.
// Board: Ozobot DRVKit (esp32:esp32:ozobot_drvkit), same as OUTER. Requires
// Tools > Pin Numbering: "By GPIO number (legacy)" - see config.h for why.
// Libraries via Library Manager (Sketch > Include Library > Manage Libraries):
//   - RTClib (Adafruit)
//   - VL53L1X (Pololu)
// (mbedtls/esp_random/bootloader_random/esp_now ship with the ESP32 core, no install needed.)
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "UartLink.h"
#include "EspNowLink.h"
#include "TransportProbe.h"
#include "CryptoMbedTLS.h"
#include "NvsStore.h"
#include "LockDriver.h"
#include "ToFPresenceSensor.h"
#include "RTCModule.h"
#include "InnerController.h"

UartLink uartLink(Serial2, LINK_RX_PIN, LINK_TX_PIN, LINK_ALIVE_TIMEOUT_MS, LINK_UART_BAUD);
EspNowLink* espnowLink = nullptr;
Link* activeLink = nullptr;

CryptoMbedTLS crypto;
NvsStore nvs;
LockDriver lock(MOTOR_IN1_PIN, MOTOR_IN2_PIN, MOTOR_PULSE_MS);
ToFPresenceSensor tof(TOF_PRESENCE_THRESHOLD_MM);
RTCModule rtc;
InnerController* controller = nullptr;

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

    controller = new InnerController(*activeLink, crypto, nvs, lock, tof, rtc, MAINT_BUTTON_PIN);
    controller->begin();
}

void loop() {
    controller->update();
    handleSerialCommand();
}
