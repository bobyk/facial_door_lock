#pragma once
#include <Arduino.h>
#include <VL53L1X.h>

// Абстрактний інтерфейс -> можна підмінити реалізацію (радар, ToF, PIR) без зміни FSM.
class IPresenceSensor {
public:
    virtual bool begin() = 0;
    virtual bool isPresent() = 0;   // true якщо людина в межах порогу
    virtual ~IPresenceSensor() = default;
};

// ToF-реалізація: оптичний лазерний дальномір, не бачить крізь стіни.
// Годиться і для зовнішнього, і для внутрішнього блоку - лише міняється поріг.
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
