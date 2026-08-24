#pragma once

// Board: Ozobot DRVKit (esp32:esp32:ozobot_drvkit), ESP32-S3. Same board used
// for INNER. Requires Tools > Pin Numbering: "By GPIO number (legacy)" -
// the default "By Arduino pin" mode activates io_pin_remap.h macro
// redefinitions of pinMode/digitalWrite/digitalRead/analogRead/analogWrite
// that collide with FastLED's own internal pin.h (compile error). Switching
// to raw GPIO numbering avoids that entirely and matches every pin number
// below being a literal GPIO, not a remapped "Dx" logical pin.
//
// This board has FIXED onboard peripherals (see its pins_arduino.h): a dual
// motor driver (unused here - INNER uses a separate external driver), a
// single button sharing GPIO0/BOOT, and a single onboard addressable RGB LED
// on GPIO47 (see LED_DATA_PIN comment below - the board's own software pin
// file claimed GPIO42 instead, but a pin chart photo of the actual board
// shows the RGB LED at GPIO47, which is what's used here). GPIO18/17/21/33
// (motor driver) and GPIO0 (boot/button) are physically committed - not
// repurposed here.
// Free general-purpose pins used below are GPIO1-10, confirmed empirically
// against the real board rather than trusted from the software pin file -
// see the I2C comment below for why that distinction matters here.

// --- Onboard button (BUTTON = shares GPIO0/BOOT) and RGB LED (GPIO47) ---
// Reusing GPIO0 as a runtime input AFTER boot is fine (standard "BOOT button
// doubles as user button" pattern) - but ONLY at runtime. GPIO0 is also the
// ESP32's bootloader-entry strapping pin, checked by the ROM before any of
// our code runs: hold it LOW across a reset and the chip enters USB download
// mode instead of running the app at all. So unlike INNER's maintenance
// button, this one can NEVER be used as a "hold at boot to enter pairing"
// trigger - pairing on OUTER is entered via the `pair` serial command
// instead (see OuterController::enterPairingNow()).
#define SCAN_BUTTON_PIN 0   // = BUTTON. Press (at runtime only) = scan trigger.
// GPIO47 per a pin chart of the actual board (labelled RGB_LED there); the
// Ozobot DRVKit software pin file (pins_arduino.h) claims GPIO42 instead -
// going with the pin chart since the software file was already wrong once
// on this same board (see the I2C comment below). Still worth confirming
// live (does it light up / animate correctly) rather than assuming.
#define LED_DATA_PIN 47     // = RGB_LED

// --- I2C for PCF8574 keypad ---
// Confirmed empirically on real hardware (see git history) - the board's
// software definition claimed a "labelled I2C header" at GPIO47/48, but the
// keypad only responded once moved to GPIO8/9, which is also what's actually
// silkscreened at the header the keypad is wired to (bare "8"/"9", no
// software-defined role). The keypad's own ribbon wiring also didn't match a
// naive "P0-P3=rows, P4-P6=columns" order - see ROW_BIT/COL_BIT in
// KeypadPCF8574.h for the confirmed real mapping.
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define PCF8574_ADDR 0x20

// --- FRM1213 face module UART (Serial1) - free general-purpose pins ---
#define FACE_RX_PIN 1
#define FACE_TX_PIN 2
#define FACE_UART_BAUD 115200

// --- Inter-board Link UART (Serial2) - see UartLink.h ---
#define LINK_RX_PIN 3
#define LINK_TX_PIN 4
#define LINK_UART_BAUD 115200

// --- LD2420 external presence radar UART (Serial0) ---
#define PRESENCE_RX_PIN 5
#define PRESENCE_TX_PIN 6
#define PRESENCE_UART_BAUD 115200

// --- Tamper switch (external NC switch on enclosure lid, internal pullup) ---
#define TAMPER_PIN 7 // HIGH = opened (see OuterController::checkTamper)

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
#define LINK_ALIVE_TIMEOUT_MS 10000UL      // Link::isAlive() threshold, either transport

// --- Behaviour ---
#define MAX_FACE_ATTEMPTS 3 // consecutive face-scan failures before falling back to PIN entry

// --- Keypad PIN lockout (escalating, RAM-only - resets on reboot like INNER's own rate limit) ---
#define PIN_LOCKOUT_MAX_FAILS 3        // failed PIN attempts before the keypad locks out
#define PIN_LOCKOUT_BASE_MS (60UL * 1000) // 1st lockout = 1 min, doubles each time it re-triggers
