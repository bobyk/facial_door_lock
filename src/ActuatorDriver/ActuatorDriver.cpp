#include "ActuatorDriver.h"

void ActuatorDriver::begin() {
    pinMode(_in1, OUTPUT);
    pinMode(_in2, OUTPUT);
    stop();
}

void ActuatorDriver::open() {
    digitalWrite(_in1, HIGH);
    digitalWrite(_in2, LOW);
    delay(_pulseMs);
    stop();
}

void ActuatorDriver::close() {
    digitalWrite(_in1, LOW);
    digitalWrite(_in2, HIGH);
    delay(_pulseMs);
    stop();
}

void ActuatorDriver::stop() {
    digitalWrite(_in1, LOW);
    digitalWrite(_in2, LOW);
}
