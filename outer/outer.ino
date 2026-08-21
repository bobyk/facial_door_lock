// OUTER board: face module, scan button, tamper switch, presence radar, keypad, LED.
// No electrical path to the lock - every unlock decision is made by INNER.
// Requires Tools > USB CDC On Boot: Enabled (see config.h).
// Libraries via Library Manager (Sketch > Include Library > Manage Libraries):
//   - FastLED
// (VL53L1X/RTClib are NOT needed on this board - those are INNER-only.
//  mbedtls/esp_random/bootloader_random ship with the ESP32 core, no install needed.)
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "UartLink.h"
#include "CryptoMbedTLS.h"
#include "NvsStore.h"
#include "Frm1213Driver.h"
#include "PresenceSensor.h"
#include "LD2420PresenceSensor.h"
#include "KeypadPCF8574.h"
#include "LedStrip.h"
#include "OuterController.h"

UartLink link(Serial2, LINK_RX_PIN, LINK_TX_PIN, LINK_UART_BAUD);
CryptoMbedTLS crypto;
NvsStore nvs;
Frm1213Driver face(Serial1, FACE_RX_PIN, FACE_TX_PIN,
                    FRM1213_VERIFY_TIMEOUT_MS, FRM1213_RESET_ACK_TIMEOUT_MS, FRM1213_RESET_READY_WAIT_MS,
                    FACE_UART_BAUD);
LD2420PresenceSensor presence(Serial0, PRESENCE_RX_PIN, PRESENCE_TX_PIN, PRESENCE_UART_BAUD);
KeypadPCF8574 keypad(PCF8574_ADDR);
LedStrip led(LED_NUM);

OuterController controller(link, crypto, nvs, face, presence, keypad, led, SCAN_BUTTON_PIN, TAMPER_PIN);

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    nvs.begin(NVS_NAMESPACE);
    keypad.begin(Wire);
    controller.begin();
}

void loop() {
    controller.update();
}
