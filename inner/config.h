#pragma once

// Board: Ozobot DRVKit (esp32:esp32:ozobot_drvkit), ESP32-S3. Same board used
// for OUTER. Requires Tools > Pin Numbering: "By GPIO number (legacy)" - see
// the long comment in outer/config.h for why (FastLED/io_pin_remap.h conflict
// under the default "By Arduino pin" mode).
//
// This board has a built-in dual motor driver (GPIO18/17 and GPIO21/33) -
// deliberately NOT used for the lock actuator here; the lock uses its own
// separate external H-bridge (see LockDriver.h) on free general-purpose
// pins instead, so the onboard driver channels are left unused/unconfigured.
// Free general-purpose pins used below are GPIO1-10 ("A0-A9" on the board's
// own silkscreen) plus the board's labelled I2C header (GPIO47/48).

// --- I2C for DS3231 RTC and VL53L1X (TOF400C) - board's labelled I2C header ---
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define TOF_ADDR 0x29 // VL53L1X default address

// --- Inter-board Link UART (Serial2) - free general-purpose pins ---
#define LINK_RX_PIN 1
#define LINK_TX_PIN 2
#define LINK_UART_BAUD 115200

// --- Motor driver (2-channel H-bridge, IN1/IN2) - separate from the board's
// own onboard motor driver, per design decision (see comment above) ---
#define MOTOR_IN1_PIN 3
#define MOTOR_IN2_PIN 4
#define MOTOR_PULSE_MS 500

// --- Maintenance button: held at boot = pairing window; held 3s at runtime = clear tamper lockout ---
#define MAINT_BUTTON_PIN 5

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
#define LINK_ALIVE_TIMEOUT_MS 10000UL  // Link::isAlive() threshold, either transport

// --- ESP-NOW (fallback transport) ---
#define WIFI_CHANNEL 6 // INNER picks this at pairing time, broadcasts it to OUTER; both must match
