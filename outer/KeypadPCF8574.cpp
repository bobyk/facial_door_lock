#include "KeypadPCF8574.h"

constexpr char KeypadPCF8574::KEYS[ROWS][COLS];

bool KeypadPCF8574::begin(TwoWire& wire) {
    _wire = &wire;
    // P0-P3 рядки = LOW по черзі (вихід), P4-P6 стовпці = HIGH (вхід з pull-up), P7 не використовується.
    writePort(0b11110000);
    return true;
}

uint8_t KeypadPCF8574::readPort() {
    _wire->requestFrom((int)_addr, 1);
    if (_wire->available()) return (uint8_t)_wire->read();
    return 0xFF;
}

void KeypadPCF8574::writePort(uint8_t value) {
    _wire->beginTransmission(_addr);
    _wire->write(value);
    _wire->endTransmission();
}

char KeypadPCF8574::scan() {
    char found = 0;
    for (uint8_t r = 0; r < ROWS && !found; ++r) {
        // Активний рядок - LOW, решта рядків HIGH (щоб не давати хибних коротких замикань
        // між рядками через клавішу), стовпці завжди HIGH (вхід з pull-up).
        uint8_t out = 0b11110000 | (0x0F & ~(1 << r));
        writePort(out);
        uint8_t in = readPort();
        for (uint8_t c = 0; c < COLS; ++c) {
            if (!(in & (1 << (4 + c)))) {
                found = KEYS[r][c];
                break;
            }
        }
    }
    writePort(0b11110000); // повернути рядки в стан очікування

    uint32_t now = millis();
    if (found != _lastKey) {
        _lastChangeMs = now;
        _lastKey = found;
        _emitted = false;
        return 0; // чекаємо стабілізації перед видачею нового символу
    }
    if (found && !_emitted && (now - _lastChangeMs) >= DEBOUNCE_MS) {
        _emitted = true; // видаємо рівно один раз на стабільне натискання
        return found;
    }
    return 0;
}
