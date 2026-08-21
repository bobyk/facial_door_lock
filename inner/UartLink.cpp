#include "UartLink.h"
#include <string.h>

static constexpr uint8_t UART_LINK_SYNC = 0xA5;

bool UartLink::begin() {
    _uart.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    return true;
}

bool UartLink::send(const uint8_t* data, size_t len) {
    if (len > LINK_FRAME_MAX) return false;
    uint8_t crc = linkFrameCrc8(data, len); // окрема "обгортка" над уже закодованим LinkFrame

    _uart.write(UART_LINK_SYNC);
    _uart.write((uint8_t)len);
    if (len > 0) _uart.write(data, len);
    _uart.write(crc);
    return true;
}

bool UartLink::receive(uint8_t* buf, size_t& len) {
    while (_uart.available()) {
        uint8_t b = (uint8_t)_uart.read();
        switch (_rxState) {
            case RxState::SYNC:
                if (b == UART_LINK_SYNC) _rxState = RxState::LEN;
                break;
            case RxState::LEN:
                if (b > LINK_FRAME_MAX) {
                    _rxState = RxState::SYNC; // неможлива довжина - кадр пошкоджено
                    break;
                }
                _rxLen = b;
                _rxIndex = 0;
                _rxState = (_rxLen == 0) ? RxState::CRC : RxState::PAYLOAD;
                break;
            case RxState::PAYLOAD:
                _rxBuf[_rxIndex++] = b;
                if (_rxIndex >= _rxLen) _rxState = RxState::CRC;
                break;
            case RxState::CRC:
                _rxState = RxState::SYNC; // готові до наступного кадру незалежно від результату
                if (linkFrameCrc8(_rxBuf, _rxLen) != b) break; // checksum не збігся - відкидаємо

                len = _rxLen;
                memcpy(buf, _rxBuf, _rxLen);
                _lastRxMillis = millis();
                return true;
        }
    }
    return false;
}
