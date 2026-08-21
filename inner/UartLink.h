#pragma once
#include <Arduino.h>
#include "Link.h"
#include "LinkFrame.h"

// UART реалізація Link. Кадр на дроті: 0xA5 | len(1) | data[len] | crc8(1).
// data - це вже закодований LinkFrame (type+len+payload+crc8) від викликача;
// UartLink лишень додає власний sync-байт + довжину + crc8 для розмежування
// кадрів у байтовому потоці - вмісту data не інтерпретує.
class UartLink : public Link {
public:
    UartLink(HardwareSerial& uart, uint8_t rxPin, uint8_t txPin, uint32_t aliveTimeoutMs,
              uint32_t baud = 115200)
        : _uart(uart), _rxPin(rxPin), _txPin(txPin), _baud(baud), _aliveTimeoutMs(aliveTimeoutMs) {}

    bool begin() override;
    bool send(const uint8_t* data, size_t len) override;
    bool receive(uint8_t* buf, size_t& len) override;
    bool isAlive() override { return millis() - _lastRxMillis < _aliveTimeoutMs; }
    const char* name() override { return "UART"; }

private:
    enum class RxState { SYNC, LEN, PAYLOAD, CRC };

    HardwareSerial& _uart;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;
    uint32_t _aliveTimeoutMs;

    RxState _rxState = RxState::SYNC;
    uint8_t _rxLen = 0, _rxIndex = 0;
    uint8_t _rxBuf[LINK_FRAME_MAX];
    uint32_t _lastRxMillis = 0;
};
