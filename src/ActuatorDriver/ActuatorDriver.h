#pragma once
#include <Arduino.h>

// Керування актуатором замка через 2-канальний драйвер (H-міст або 2 реле).
// IN1=HIGH,IN2=LOW -> відкрити; IN1=LOW,IN2=HIGH -> закрити; обидва LOW -> стоп.
// Імпульсне керування (актуатор автозамка зазвичай не тримає напругу постійно).
class ActuatorDriver {
public:
    ActuatorDriver(uint8_t in1Pin, uint8_t in2Pin, uint16_t pulseMs = 500)
        : _in1(in1Pin), _in2(in2Pin), _pulseMs(pulseMs) {}

    void begin();
    void open();   // блокуючий імпульс на _pulseMs (для простоти; можна замінити на неблокуючий)
    void close();
    void stop();

private:
    uint8_t _in1, _in2;
    uint16_t _pulseMs;
};
