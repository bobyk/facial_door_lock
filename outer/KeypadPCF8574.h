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

    // Логічний рядок/стовпець (індекс у KEYS) -> реальний біт P на PCF8574.
    // Визначено емпірично зі спостереженої підміни клавіш на конкретному
    // фізичному підключенні (напр. фізичний рядок "4,5,6" виявився на P0,
    // не на P1) - ribbon-кабель клавіатури не має стандартного порядку
    // виводів, тож підлаштовуємось під факт підключення, а не навпаки.
    static constexpr uint8_t ROW_BIT[ROWS] = {2, 0, 1, 3};
    static constexpr uint8_t COL_BIT[COLS] = {4, 6, 5};

    TwoWire* _wire = nullptr;
    uint8_t _addr;
    char _lastKey = 0;
    uint32_t _lastChangeMs = 0;
    bool _emitted = false;
    uint32_t _lastI2cErrorLogMs = 0;

    // ok=false якщо PCF8574 не ACK-нув на цій адресі - інакше НЕДОСЯЖНІСТЬ
    // пристрою невідрізнима від "жодну клавішу не натиснуто" (обидві дають 0xFF).
    uint8_t readPort(bool& ok);
    bool writePort(uint8_t value);
    void logI2cErrorRateLimited();
};
