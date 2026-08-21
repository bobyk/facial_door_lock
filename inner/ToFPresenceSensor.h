#pragma once
#include <Arduino.h>
#include <VL53L1X.h>
#include "PresenceSensor.h"

// TOF400C (VL53L1X) - лазерний оптичний дальномір, вузький кут огляду,
// фізично не бачить крізь стіни. Використовується для відмикання зсередини
// без будь-якої авторизації (egress) - див. InnerController.
class ToFPresenceSensor : public IPresenceSensor {
public:
    explicit ToFPresenceSensor(uint16_t thresholdMm) : _thresholdMm(thresholdMm) {}

    bool begin() override {
        _sensor.setTimeout(500);
        if (!_sensor.init()) return false;
        _sensor.setDistanceMode(VL53L1X::Short); // короткі дистанції = точніше й менше шуму
        _sensor.setMeasurementTimingBudget(50000);
        _sensor.startContinuous(50);
        return true;
    }

    bool isPresent() override {
        uint16_t d = _sensor.read();
        if (_sensor.timeoutOccurred()) return false;
        return d > 0 && d <= _thresholdMm;
    }

private:
    VL53L1X _sensor;
    uint16_t _thresholdMm;
};
