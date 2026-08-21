#pragma once
#include <Arduino.h>
#include "Link.h"

// UART реалізація Link. Кадр: 0xA5 | type(1) | len(1) | payload[len] | crc8.
// crc8 рахується над type+len+payload (без sync-байта).
class UartLink : public Link {
public:
    UartLink(HardwareSerial& uart, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200)
        : _uart(uart), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

    bool begin() override;
    bool send(uint8_t type, const uint8_t* payload, uint8_t len) override;
    bool poll(uint8_t& type, uint8_t* payload, uint8_t& len, uint8_t maxLen) override;
    uint32_t lastRxMillis() const override { return _lastRxMillis; }

private:
    enum class RxState { SYNC, TYPE, LEN, PAYLOAD, CRC };

    HardwareSerial& _uart;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;

    RxState _rxState = RxState::SYNC;
    uint8_t _rxType = 0;
    uint8_t _rxLen = 0;
    uint8_t _rxIndex = 0;
    uint8_t _rxPayload[LINK_MAX_PAYLOAD];
    uint32_t _lastRxMillis = 0;

    static uint8_t crc8(const uint8_t* data, size_t len);
};
