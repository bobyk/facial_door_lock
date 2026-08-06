// Плата: ESP32-S3 (Arduino IDE, Tools > Board: "ESP32S3 Dev Module" або відповідна super mini).
// Бібліотеки через Library Manager (Sketch > Include Library > Manage Libraries):
//   - RTClib (Adafruit)
//   - FastLED
//   - VL53L1X (Pololu)
#include <Arduino.h>
#include <Wire.h>
#include "Config.h"
#include "RTCModule.h"
#include "KeypadPCF8574.h"
#include "LedStrip.h"
#include "ActuatorDriver.h"
#include "PresenceSensor.h"
#include "LD2420PresenceSensor.h"
#include "FaceRecognizer.h"
#include "LockController.h"
#include "CronScheduler.h"

// Поріг для внутрішнього ToF - ~1м від дверей
static constexpr uint16_t PRESENCE_THRESHOLD_MM = 1000;
static constexpr uint32_t AUTO_LOCK_INTERVAL_MS = 10UL * 60 * 1000; // N=10 хв, підлаштувати

RTCModule rtc;
KeypadPCF8574 keypad(PCF8574_ADDR);
LedStrip led(LED_DATA_PIN, LED_NUM);
ActuatorDriver actuator(ACTUATOR_IN1_PIN, ACTUATOR_IN2_PIN);
// Зовнішній датчик - радар LD2420 по UART (бачить через немeталеві перешкоди, для входу ззовні прийнятно).
LD2420PresenceSensor outerPresence(Serial2, RADAR_RX_PIN, RADAR_TX_PIN);
// Внутрішній датчик - ToF (VL53L1X), вузький кут огляду, фізично не бачить крізь стіни.
// Єдиний VL53L1X на I2C-шині - конфлікту адрес 0x29 немає (зовнішній сенсор не на I2C).
ToFPresenceSensor innerPresence(PRESENCE_THRESHOLD_MM);
FRM1213Recognizer face(Serial1, 115200);
CronScheduler autoLockCron;

LockController lock(outerPresence, innerPresence, face, keypad, led, actuator, "1234");

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    rtc.begin();
    keypad.begin(Wire);
    led.begin();
    actuator.begin();
    outerPresence.begin();
    innerPresence.begin();
    face.begin();

    autoLockCron.every(AUTO_LOCK_INTERVAL_MS, []() { lock.forceLock(); });
}

void loop() {
    lock.update();
    autoLockCron.tick();
}
