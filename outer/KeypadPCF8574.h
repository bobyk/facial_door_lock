#pragma once
#include <Arduino.h>
#include <Wire.h>

// Матриця 4x3 через розширювач I2C PCF8574T. P0-P3 - рядки (вихід), P4-P6 -
// стовпці (вхід, вбудований pull-up). Неблокуючий: scan() лише читає поточний
// стан і повертає символ при новому натисканні (програмний дебаунс за millis()).
class KeypadPCF8574 {
public:
    explicit KeypadPCF8574(uint8_t i2cAddr) : _addr(i2cAddr) {}

    bool begin(TwoWire& wire);
    // Повертає символ при новому натисканні, або 0 якщо нічого нового.
    char scan();

private:
    static constexpr uint8_t ROWS = 4;
    static constexpr uint8_t COLS = 3;
    static constexpr uint32_t DEBOUNCE_MS = 30;
    static constexpr char KEYS[ROWS][COLS] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'},
        {'*', '0', '#'},
    };

    TwoWire* _wire = nullptr;
    uint8_t _addr;
    char _lastKey = 0;
    uint32_t _lastChangeMs = 0;
    bool _emitted = false;

    uint8_t readPort();
    void writePort(uint8_t value);
};
