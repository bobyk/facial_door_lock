#pragma once
#include <Wire.h>

// Матриця 4x3 через PCF8574 (квазі-двонаправлені піни).
// Підключення (типове): P0-P3 = рядки (виходи), P4-P6 = стовпці (входи, є вбудований pull-up).
// Переносимо на будь-який пристрій з Wire — залежить лише від адреси I2C.
class KeypadPCF8574 {
public:
    explicit KeypadPCF8574(uint8_t i2cAddr = 0x20) : _addr(i2cAddr) {}

    bool begin(TwoWire& wire = Wire);
    char scan();               // повертає символ натиснутої клавіші, або 0 якщо нічого

private:
    uint8_t _addr;
    TwoWire* _wire = nullptr;

    static constexpr char _layout[4][3] = {
        {'1','2','3'},
        {'4','5','6'},
        {'7','8','9'},
        {'*','0','#'}
    };

    void writeByte(uint8_t v);
    uint8_t readByte();
};
