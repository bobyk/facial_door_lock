#pragma once
#include <Arduino.h>
#include "PresenceSensor.h"

// LD2420 (Hi-Link) - 24ГГц радар присутності, зв'язок по UART.
// !!! Точний бінарний протокол кадрів LD2420 (заголовки, код стану цілі, checksum)
// в цьому ТЗ не наведений і публічно не звірений. За тим самим принципом, що й
// FRM1213Recognizer (FaceRecognizer.h) - парсинг кадру лишається заглушкою до
// звірки з офіційним даташитом Hi-Link або сніфінгу UART між модулем і фірмовою утилітою.
// Альтернатива, що не потребує парсингу протоколу: пін OT1 модуля видає вже готовий
// цифровий сигнал присутності (HIGH/LOW) - вартий розгляду, якщо парсинг кадрів зайвий.
class LD2420PresenceSensor : public IPresenceSensor {
public:
    LD2420PresenceSensor(HardwareSerial& uart, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200)
        : _uart(uart), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

    bool begin() override;
    bool isPresent() override;

private:
    HardwareSerial& _uart;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;
    bool _present = false;

    void pollFrames();
};
