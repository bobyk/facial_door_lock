#include "KeypadPCF8574.h"

constexpr char KeypadPCF8574::_layout[4][3];

bool KeypadPCF8574::begin(TwoWire& wire) {
    _wire = &wire;
    _wire->beginTransmission(_addr);
    return _wire->endTransmission() == 0; // ACK -> модуль на шині
}

void KeypadPCF8574::writeByte(uint8_t v) {
    _wire->beginTransmission(_addr);
    _wire->write(v);
    _wire->endTransmission();
}

uint8_t KeypadPCF8574::readByte() {
    _wire->requestFrom(_addr, (uint8_t)1);
    return _wire->available() ? _wire->read() : 0xFF;
}

char KeypadPCF8574::scan() {
    if (!_wire) return 0;

    for (uint8_t row = 0; row < 4; row++) {
        // Рядок row тримаємо в 0, решту рядків і всі стовпці - в 1 (pull-up)
        uint8_t out = 0xFF & ~(1 << row);
        writeByte(out);
        delayMicroseconds(50); // час на встановлення рівнів

        uint8_t in = readByte();
        for (uint8_t col = 0; col < 3; col++) {
            if (!(in & (1 << (4 + col)))) {
                writeByte(0xFF); // повернути шину в стан спокою
                return _layout[row][col];
            }
        }
    }
    writeByte(0xFF);
    return 0;
}
