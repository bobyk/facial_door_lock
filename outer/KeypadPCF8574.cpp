#include "KeypadPCF8574.h"

constexpr char KeypadPCF8574::KEYS[ROWS][COLS];
constexpr uint8_t KeypadPCF8574::ROW_BIT[ROWS];
constexpr uint8_t KeypadPCF8574::COL_BIT[COLS];

bool KeypadPCF8574::begin(TwoWire& wire) {
    _wire = &wire;
    // P0-P3 рядки = LOW по черзі (вихід), P4-P6 стовпці = HIGH (вхід з pull-up), P7 не використовується.
    return writePort(0b11110000);
}

uint8_t KeypadPCF8574::readPort(bool& ok) {
    uint8_t n = _wire->requestFrom((int)_addr, (int)1);
    ok = (n == 1) && _wire->available();
    if (ok) return (uint8_t)_wire->read();
    return 0xFF; // те саме значення, що й "жодна клавіша не натиснута" - див. ok
}

bool KeypadPCF8574::writePort(uint8_t value) {
    _wire->beginTransmission(_addr);
    _wire->write(value);
    return _wire->endTransmission() == 0; // 0 = ACK, інакше пристрій не відповів
}

void KeypadPCF8574::dumpRaw() {
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 300) return;
    lastMs = now;

    // Керує голими бітами P0-P3 напряму (без ROW_BIT) - щоб побачити реальну
    // електрику незалежно від будь-якого припущення про мапінг рядків/стовпців.
    uint8_t vals[4];
    for (uint8_t p = 0; p < 4; ++p) {
        writePort(0xF0 | (0x0F & (uint8_t)~(1 << p)));
        bool ok;
        vals[p] = readPort(ok);
    }
    writePort(0xF0);
    Serial.printf("[RAW] P0low=0x%02X P1low=0x%02X P2low=0x%02X P3low=0x%02X\n",
                  vals[0], vals[1], vals[2], vals[3]);
}

void KeypadPCF8574::logI2cErrorRateLimited() {
    uint32_t now = millis();
    if (now - _lastI2cErrorLogMs < 2000) return;
    _lastI2cErrorLogMs = now;
    Serial.printf("[KEYPAD] PCF8574 not responding at I2C address 0x%02X - check wiring/address/pull-ups\n", _addr);
}

char KeypadPCF8574::scan() {
    char found = 0;
    bool anyOk = false;
    for (uint8_t r = 0; r < ROWS && !found; ++r) {
        // Активний рядок - LOW, решта рядків HIGH (щоб не давати хибних коротких замикань
        // між рядками через клавішу), стовпці завжди HIGH (вхід з pull-up).
        // ROW_BIT/COL_BIT перекладають логічний row/col у реальний P-біт (див. коментар в .h).
        uint8_t out = 0b11110000 | (0x0F & ~(1 << ROW_BIT[r]));
        bool wOk = writePort(out);
        bool rOk;
        uint8_t in = readPort(rOk);
        anyOk = anyOk || (wOk && rOk);
        for (uint8_t c = 0; c < COLS; ++c) {
            if (!(in & (1 << COL_BIT[c]))) {
                found = KEYS[r][c];
                break;
            }
        }
    }
    writePort(0b11110000); // повернути рядки в стан очікування

    if (!anyOk) {
        logI2cErrorRateLimited();
        return 0; // недосяжний пристрій - нічого не інтерпретуємо як натискання
    }

    uint32_t now = millis();
    if (found != _lastKey) {
        _lastChangeMs = now;
        _lastKey = found;
        _emitted = false;
        return 0; // чекаємо стабілізації перед видачею нового символу
    }
    if (found && !_emitted && (now - _lastChangeMs) >= DEBOUNCE_MS) {
        _emitted = true; // видаємо рівно один раз на стабільне натискання
        Serial.printf("[KEYPAD] key='%c'\n", found);
        return found;
    }
    return 0;
}
