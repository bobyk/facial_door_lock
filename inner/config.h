#pragma once

// Board: ESP32-S3 Super Mini. Pin pool avoids strapping pins (GPIO0, 45, 46)
// and native-USB pins (19, 20), per the board's limited/odd GPIO map.
// See WIRING.md for the full picture.

// --- Inter-board Link UART (Serial2) - see src/UartLink.h ---
#define LINK_RX_PIN 15
#define LINK_TX_PIN 16
#define LINK_UART_BAUD 115200

// --- I2C for DS3231 RTC and VL53L1X (TOF400C) ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define TOF_ADDR 0x29 // VL53L1X default address

// --- Motor driver (2-channel H-bridge, IN1/IN2) ---
#define MOTOR_IN1_PIN 5
#define MOTOR_IN2_PIN 6
#define MOTOR_PULSE_MS 500

// --- Maintenance button: held at boot = pairing window; held 3s at runtime = clear tamper lockout ---
#define MAINT_BUTTON_PIN 4

// --- NVS namespace ---
#define NVS_NAMESPACE "innercfg"

// --- Timeouts / thresholds (ms unless noted) ---
#define NONCE_VALIDITY_MS 2000UL       // "nonce validity window 2000ms" per spec
#define RATE_LIMIT_MAX_FAILS 5
#define RATE_LIMIT_BLOCK_MS (5UL * 60 * 1000)
#define TAMPER_CLEAR_HOLD_MS 3000UL
#define PAIRING_WINDOW_MS 30000UL
#define PAIRING_BOOT_HOLD_MS 50UL
#define HEARTBEAT_MISSING_MS 10000UL   // OUTER heartbeat missing this long -> suspicious log, no lockout
#define TOF_PRESENCE_THRESHOLD_MM 1000 // internal ToF unlock-without-auth threshold
