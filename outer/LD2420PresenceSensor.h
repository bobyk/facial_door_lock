#pragma once
#include <Arduino.h>
#include "PresenceSensor.h"

// LD2420 (Hi-Link) - 24ГГц радар присутності, зв'язок по UART.
// !!! Точний бінарний протокол кадрів LD2420 (заголовки, код стану цілі, checksum)
// публічно не звірений. Парсинг кадру лишається заглушкою (UART відкривається,
// байти вичитуються, але isPresent() завжди повертає false) до звірки з офіційним
// даташитом Hi-Link або сніфінгом UART - той самий принцип, що й для frm1213.h,
// де протокол натомість БУВ звірений і тому реалізований по-справжньому.
// Альтернатива без парсингу протоколу: пін OT1 модуля видає готовий цифровий
// сигнал присутності (HIGH/LOW) - вартий розгляду, якщо парсинг кадрів зайвий.
class LD2420PresenceSensor : public IPresenceSensor {
public:
    LD2420PresenceSensor(HardwareSerial& uart, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200)
        : _uart(uart), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

    bool begin() override {
        _uart.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
        return true;
    }

    bool isPresent() override {
        // TODO: розпарсити реальний формат кадру LD2420. Поки що лише вичитуємо
        // буфер, щоб він не переповнювався; _present свідомо не оновлюється.
        while (_uart.available()) _uart.read();
        return _present;
    }

private:
    HardwareSerial& _uart;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;
    bool _present = false;
};
