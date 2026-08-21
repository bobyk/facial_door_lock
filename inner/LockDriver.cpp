#include "LockDriver.h"

void LockDriver::begin() {
    pinMode(_in1, OUTPUT);
    pinMode(_in2, OUTPUT);
    stop();
}

void LockDriver::open() {
    digitalWrite(_in1, HIGH);
    digitalWrite(_in2, LOW);
    delay(_pulseMs); // документований виняток - див. коментар в LockDriver.h
    stop();
}

void LockDriver::close() {
    digitalWrite(_in1, LOW);
    digitalWrite(_in2, HIGH);
    delay(_pulseMs);
    stop();
}

void LockDriver::stop() {
    digitalWrite(_in1, LOW);
    digitalWrite(_in2, LOW);
}
