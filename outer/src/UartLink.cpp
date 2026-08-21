#include "UartLink.h"
#include <string.h>

static constexpr uint8_t LINK_SYNC = 0xA5;

uint8_t UartLink::crc8(const uint8_t* data, size_t len) {
    // CRC-8-CCITT (poly 0x07) - лише виявлення побитового шуму на кабелі 5м,
    // не криптографічний захист (той на рівні HMAC у Protocol/Crypto).
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

bool UartLink::begin() {
    _uart.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    return true;
}

bool UartLink::send(uint8_t type, const uint8_t* payload, uint8_t len) {
    if (len > LINK_MAX_PAYLOAD) return false;
    uint8_t header[2] = { type, len };
    uint8_t crcBuf[2 + LINK_MAX_PAYLOAD];
    crcBuf[0] = type;
    crcBuf[1] = len;
    if (len > 0) memcpy(crcBuf + 2, payload, len);
    uint8_t crc = crc8(crcBuf, 2 + len);

    _uart.write(LINK_SYNC);
    _uart.write(header, 2);
    if (len > 0) _uart.write(payload, len);
    _uart.write(crc);
    return true;
}

bool UartLink::poll(uint8_t& type, uint8_t* payload, uint8_t& len, uint8_t maxLen) {
    while (_uart.available()) {
        uint8_t b = (uint8_t)_uart.read();
        switch (_rxState) {
            case RxState::SYNC:
                if (b == LINK_SYNC) _rxState = RxState::TYPE;
                break;
            case RxState::TYPE:
                _rxType = b;
                _rxState = RxState::LEN;
                break;
            case RxState::LEN:
                if (b > LINK_MAX_PAYLOAD) {
                    _rxState = RxState::SYNC; // неможлива довжина - кадр пошкоджено, ресинхронізація
                    break;
                }
                _rxLen = b;
                _rxIndex = 0;
                _rxState = (_rxLen == 0) ? RxState::CRC : RxState::PAYLOAD;
                break;
            case RxState::PAYLOAD:
                _rxPayload[_rxIndex++] = b;
                if (_rxIndex >= _rxLen) _rxState = RxState::CRC;
                break;
            case RxState::CRC: {
                uint8_t crcBuf[2 + LINK_MAX_PAYLOAD];
                crcBuf[0] = _rxType;
                crcBuf[1] = _rxLen;
                memcpy(crcBuf + 2, _rxPayload, _rxLen);
                _rxState = RxState::SYNC; // готові до наступного кадру незалежно від результату
                if (crc8(crcBuf, 2 + _rxLen) != b) break; // checksum не збігся - відкидаємо кадр

                type = _rxType;
                len = (_rxLen < maxLen) ? _rxLen : maxLen;
                memcpy(payload, _rxPayload, len);
                _lastRxMillis = millis();
                return true;
            }
        }
    }
    return false;
}
