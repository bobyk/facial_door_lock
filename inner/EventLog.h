#pragma once
#include <Arduino.h>

// Кільцевий буфер останніх подій: друкує в Serial (як і раніше) І зберігає
// для StatusServer/MqttReporter, які лише читають його - жодного зв'язку з
// бізнес-логікою InnerController, тільки текстові рядки.
class EventLog {
public:
    static constexpr uint8_t CAPACITY = 20;
    static constexpr uint8_t LINE_LEN = 96;

    void add(const char* line) {
        Serial.println(line);
        strncpy(_lines[_head], line, LINE_LEN - 1);
        _lines[_head][LINE_LEN - 1] = '\0';
        _head = (_head + 1) % CAPACITY;
        if (_count < CAPACITY) _count++;
        _totalAdded++;
    }

    // Монотонний лічильник усіх доданих рядків за час роботи - дозволяє
    // MqttReporter виявляти нові події без порівняння вмісту.
    uint32_t totalAdded() const { return _totalAdded; }

    // Копіює до maxCount останніх рядків (найстаріший спочатку) у out.
    // Повертає фактичну кількість скопійованих.
    uint8_t recent(char out[][LINE_LEN], uint8_t maxCount) const {
        uint8_t n = (_count < maxCount) ? _count : maxCount;
        uint8_t start = (_head + CAPACITY - n) % CAPACITY;
        for (uint8_t i = 0; i < n; ++i) {
            strncpy(out[i], _lines[(start + i) % CAPACITY], LINE_LEN);
        }
        return n;
    }

private:
    char _lines[CAPACITY][LINE_LEN] = {};
    uint8_t _head = 0;
    uint8_t _count = 0;
    uint32_t _totalAdded = 0;
};
