#pragma once

// --- I2C шина (RTC DS3231, PCF8574 клавіатура, VL53L1X ToF) ---
#define I2C_SDA_PIN     8
#define I2C_SCL_PIN     9

// --- LED стрічка WS2812 ---
// FastLED вимагає пін як compile-time константу (шаблонний параметр),
// тому він фіксується тут, а не передається в рантаймі в LedStrip.
#define LED_DATA_PIN    4
#define LED_NUM         30

// --- UART FRM1213 (розпізнавання обличчя) ---
#define FACE_UART_NUM   1
#define FACE_RX_PIN     17
#define FACE_TX_PIN     18

// --- UART LD2420 (зовнішній радар присутності, 24ГГц) ---
#define RADAR_UART_NUM  2
#define RADAR_RX_PIN    15
#define RADAR_TX_PIN    16

// --- Драйвер актуатора замка ---
#define ACTUATOR_IN1_PIN 5
#define ACTUATOR_IN2_PIN 6

// --- Кнопки решітки/зірочки клавіатури обробляються програмно як символи '#' і '*' ---

// --- I2C адреси ---
#define PCF8574_ADDR    0x20
// Внутрішній датчик присутності (VL53L1X) - єдиний ToF на шині: зовнішній датчик
// тепер LD2420 по UART, тож конфлікту адрес 0x29 між двома VL53L1X немає.
#define TOF_ADDR        0x29 // дефолтна адреса VL53L1X
