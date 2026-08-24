#include "KeypadPCF8574.h"

constexpr char KeypadPCF8574::KEYS[ROWS][COLS];
constexpr uint8_t KeypadPCF8574::ROW_BIT[ROWS];
constexpr uint8_t KeypadPCF8574::COL_BIT[COLS];

bool KeypadPCF8574::begin(TwoWire& wire) {
    _wire = &wire;
    // Все відпущено (жоден пін не тягнеться вниз) - safe idle незалежно від того,
    // які конкретні біти зараз призначені рядкам чи стовпцям (ROW_BIT/COL_BIT).
    return writePort(0xFF);
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

    // Керує голими бітами напряму, без ЖОДНОГО припущення "P0-P3=рядки,
    // P4-P6=стовпці" - по черзі проганяємо ВСІ 7 реальних пінів (+P7 про запас)
    // як вихід і друкуємо повний байт. Це може виявити навіть з'єднання
    // стовпець-стовпець чи рядок-рядок, які scan()/попередня версія dumpRaw()
    // не змогли б побачити в принципі (бо перевіряли лише P0-P3 як вихід).
    static const uint8_t TEST_PINS[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t vals[8];
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t p = TEST_PINS[i];
        writePort((uint8_t)~(1 << p)); // усі біти 1, крім p
        bool ok;
        vals[i] = readPort(ok);
    }
    writePort(0xF0);
    Serial.printf("[RAW] P0=0x%02X P1=0x%02X P2=0x%02X P3=0x%02X P4=0x%02X P5=0x%02X P6=0x%02X P7=0x%02X\n",
                  vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
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
        // Активний рядок - LOW, решта пінів HIGH, стовпці завжди HIGH (вхід з pull-up).
        // ROW_BIT/COL_BIT перекладають логічний row/col у реальний P-біт (див. коментар в .h).
        // ВАЖЛИВО: раніше тут був варіант "0b11110000 | (0x0F & ~(1<<ROW_BIT[r]))", який
        // працював лише для ROW_BIT у діапазоні 0-3 (низький ніббл) - для будь-якого
        // значення 4-7 (високий ніббл) маска 0x0F обнуляла скидання біта, а наступний
        // `0xF0 |` повертав його назад у 1, тож той рядок ніколи насправді не тягнувся
        // вниз. З ROW_BIT, що виходить за межі 0-3 (напр. {2,7,6,4}), це давало 0xFF
        // (жоден пін не обраний) для кожного такого рядка - саме тому 7,8,9 (P6) не
        // реагували. Формула нижче коректна для будь-якого біта 0-7.
        uint8_t out = (uint8_t)~(1 << ROW_BIT[r]);
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
