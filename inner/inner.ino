// INNER board: RTC, motor lock driver, internal TOF400C presence sensor.
// The ONLY board with an electrical path to the lock, and the sole authority
// that decides whether to drive it.
// Libraries via Library Manager (Sketch > Include Library > Manage Libraries):
//   - RTClib (Adafruit)
//   - VL53L1X (Pololu)
// (mbedtls/esp_random/bootloader_random ship with the ESP32 core, no install needed.)
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "UartLink.h"
#include "CryptoMbedTLS.h"
#include "NvsStore.h"
#include "LockDriver.h"
#include "ToFPresenceSensor.h"
#include "RTCModule.h"
#include "InnerController.h"

UartLink link(Serial2, LINK_RX_PIN, LINK_TX_PIN, LINK_UART_BAUD);
CryptoMbedTLS crypto;
NvsStore nvs;
LockDriver lock(MOTOR_IN1_PIN, MOTOR_IN2_PIN, MOTOR_PULSE_MS);
ToFPresenceSensor tof(TOF_PRESENCE_THRESHOLD_MM);
RTCModule rtc;

InnerController controller(link, crypto, nvs, lock, tof, rtc, MAINT_BUTTON_PIN);

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    nvs.begin(NVS_NAMESPACE);
    controller.begin();
}

void loop() {
    controller.update();
}
