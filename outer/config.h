#pragma once

// Board: ESP32-S3 Super Mini. Requires Tools > USB CDC On Boot: Enabled, so
// Serial (debug console) runs over native USB (GPIO19/20) and all three
// hardware UART peripherals (Serial0/1/2) stay free for FRM1213, the
// inter-board Link, and the LD2420 presence sensor.
// Pin pool avoids strapping pins (GPIO0, 45, 46) and native-USB pins (19, 20),
// per the board's limited/odd GPIO map. See WIRING.md for the full picture.

// --- FRM1213 face module UART (Serial1) ---
#define FACE_RX_PIN 17
#define FACE_TX_PIN 18
#define FACE_UART_BAUD 115200

// --- Inter-board Link UART (Serial2) - see UartLink.h ---
#define LINK_RX_PIN 15
#define LINK_TX_PIN 16
#define LINK_UART_BAUD 115200

// --- LD2420 external presence radar UART (Serial0, default UART0 pins,
// freed for peripheral use once console Serial moves to native USB) ---
#define PRESENCE_RX_PIN 44
#define PRESENCE_TX_PIN 43
#define PRESENCE_UART_BAUD 115200

// --- I2C for PCF8574 keypad ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define PCF8574_ADDR 0x20

// --- Buttons / switches ---
#define SCAN_BUTTON_PIN 4  // scan trigger (press) and pairing trigger (held at boot)
#define TAMPER_PIN 5       // NC switch on enclosure lid, internal pullup: HIGH = opened

// --- LED strip (WS2812, FastLED - pin must be a compile-time constant) ---
#define LED_DATA_PIN 6
#define LED_NUM 12

// --- NVS namespace ---
#define NVS_NAMESPACE "outercfg"

// --- Timeouts (ms) ---
#define FRM1213_VERIFY_TIMEOUT_MS 5000UL   // "5s response timeout" per spec
#define FRM1213_RESET_ACK_TIMEOUT_MS 500UL
#define FRM1213_RESET_READY_WAIT_MS 1000UL
#define NONCE_WAIT_TIMEOUT_MS 1000UL       // waiting for NONCE after REQ
#define UNLOCK_ACK_TIMEOUT_MS 2000UL       // waiting for UNLOCK_OK after AUTH
#define HEARTBEAT_INTERVAL_MS 2000UL
#define PAIRING_WINDOW_MS 30000UL
#define PAIRING_BOOT_HOLD_MS 50UL          // debounce for "button held at boot"
#define LINK_ALIVE_TIMEOUT_MS 10000UL      // Link::isAlive() threshold, either transport

// --- Behaviour ---
#define MAX_FACE_ATTEMPTS 3 // consecutive face-scan failures before falling back to PIN entry
