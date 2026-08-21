#pragma once
#include <Arduino.h>

// Абстракція -> дозволяє підмінити реалізацію датчика присутності, не чіпаючи OuterController.
class IPresenceSensor {
public:
    virtual bool begin() = 0;
    virtual bool isPresent() = 0;
    virtual ~IPresenceSensor() = default;
};
