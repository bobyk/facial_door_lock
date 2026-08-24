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

// --- I2C for DS3231 RTC and VL53L1X (TOF400C) ---
// NOT the board's "labelled I2C header" (GPIO47/48) - that turned out wrong
// on OUTER (same board model): the software pin file claims it, but the real
// I2C only responded once moved to GPIO8/9, and GPIO47 is actually the fixed
// onboard RGB LED. Going with the same confirmed pins here rather than
// repeating the same mistake - still worth an I2C scan to confirm once wired.
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define TOF_ADDR 0x29 // VL53L1X default address

// --- Status LED (onboard RGB_LED, WS2812-compatible, single pixel) ---
#define LED_DATA_PIN 47

// --- Inter-board Link UART (Serial2) - free general-purpose pins ---
#define LINK_RX_PIN 1
#define LINK_TX_PIN 2
#define LINK_UART_BAUD 115200

// --- Motor driver (2-channel H-bridge, IN1/IN2) - separate from the board's
// own onboard motor driver, per design decision (see comment above) ---
#define MOTOR_IN1_PIN 3
#define MOTOR_IN2_PIN 4
#define MOTOR_PULSE_MS 500

// --- Maintenance button: held 2s at runtime = pairing window; held 3s while
// LOCKED_OUT = clear tamper lockout. No boot-time hold needed (checked live,
// no reboot required) - simpler and more reliable than timing a boot-hold. ---
#define MAINT_BUTTON_PIN 5

// --- NVS namespace ---
#define NVS_NAMESPACE "innercfg"

// --- Timeouts / thresholds (ms unless noted) ---
#define NONCE_VALIDITY_MS 2000UL       // "nonce validity window 2000ms" per spec
#define RATE_LIMIT_MAX_FAILS 5
#define RATE_LIMIT_BLOCK_MS (5UL * 60 * 1000)
#define TAMPER_CLEAR_HOLD_MS 3000UL
#define PAIRING_WINDOW_MS 30000UL
#define PAIRING_HOLD_MS 2000UL         // hold maintenance button this long (not locked out) to pair
#define HEARTBEAT_MISSING_MS 10000UL   // OUTER heartbeat missing this long -> suspicious log, no lockout
#define TOF_PRESENCE_THRESHOLD_MM 1000 // internal ToF unlock-without-auth threshold
#define LINK_ALIVE_TIMEOUT_MS 10000UL  // Link::isAlive() threshold, either transport

// --- ESP-NOW (fallback transport) ---
#define WIFI_CHANNEL 6 // INNER picks this at pairing time, broadcasts it to OUTER; both must match

// --- Home Wi-Fi (OTA / monitoring / MQTT only - NOT the OUTER<->INNER Link) ---
// This is a SEPARATE, auxiliary network connection. The lock's own auth
// protocol (REQ/NONCE/AUTH/UNLOCK_OK) never touches this - it only ever runs
// over UartLink/EspNowLink. See WifiManager.h/StatusServer.h/MqttReporter.h
// for why the network surfaces below are deliberately read-only.
//
// CAUTION if running on ESP-NOW transport: ESP-NOW and Wi-Fi STA share the
// same radio and MUST be on the same channel. Once WifiManager connects to
// the home AP, ESP-NOW is forced onto whatever channel THAT AP uses - if it
// doesn't match WIFI_CHANNEL above (and what OUTER was paired with), the
// OUTER<->INNER link breaks silently. Pin the router to channel 6, or (better,
// and already the documented "required commissioning setting" per WIRING.md)
// switch Link transport to UART, which has no such conflict at all.
#define WIFI_HOME_SSID "Xiaomi_25E9"
#define WIFI_HOME_PASSWORD "1234567890"
#define WIFI_STATIC_IP        192, 168, 1, 227
#define WIFI_STATIC_GATEWAY   192, 168, 1, 1    // UNCONFIRMED - assumed from Xiaomi_25E9 being the router itself
#define WIFI_STATIC_SUBNET    255, 255, 255, 0
#define WIFI_RECONNECT_INTERVAL_MS 10000UL

// --- OTA (ArduinoOTA) ---
#define OTA_HOSTNAME "inner-lock"
// CHANGE THIS before deploying - a network-reachable OTA endpoint with a
// default password is a flashing-firmware-remotely risk. Set via Arduino
// IDE Tools, or edit here, before relying on this in the field.
#define OTA_PASSWORD "change-me-before-deploy"

// --- Read-only status HTTP server (WebServer.h, no new dependency) ---
#define STATUS_SERVER_PORT 80

// --- MQTT (read-only status publish, no subscribe/command topic - see MqttReporter.h) ---
#define MQTT_BROKER_HOST "192.168.1.47"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "inner-lock"
#define MQTT_USERNAME "lock"
#define MQTT_PASSWORD "nhfnfnf"
#define MQTT_TOPIC_PREFIX "doorlock/inner"
#define MQTT_PUBLISH_INTERVAL_MS 15000UL
#define MQTT_RECONNECT_INTERVAL_MS 5000UL
