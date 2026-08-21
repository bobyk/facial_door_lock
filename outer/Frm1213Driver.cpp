#include "Frm1213Driver.h"
#include <string.h>

static uint8_t frm1213Checksum(const uint8_t* frame, size_t len) {
    // GetParityCheck з протокольного документа: XOR всього, крім заголовка (2 байти)
    // і самого checksum-байта.
    uint8_t c = 0;
    for (size_t i = 2; i + 1 < len; ++i) c ^= frame[i];
    return c;
}

bool Frm1213Driver::begin() {
    _uart.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    return true;
}

void Frm1213Driver::startScan() {
    if (_state != State::IDLE) return;
    sendCommand(FRM1213_CMD_VERIFY, nullptr, 0);
    _deadline = millis() + _verifyTimeoutMs;
    _state = State::VERIFY_WAIT;
}

void Frm1213Driver::triggerReset() {
    if (_state != State::IDLE) return;
    beginReset();
}

void Frm1213Driver::beginReset() {
    sendCommand(FRM1213_CMD_STANDBY, nullptr, 0);
    _deadline = millis() + _resetAckTimeoutMs;
    _state = State::RESET_WAIT_ACK;
}

Frm1213Event Frm1213Driver::lastEvent() {
    Frm1213Event e = _pendingEvent;
    _pendingEvent = Frm1213Event::NONE;
    return e;
}

void Frm1213Driver::update() {
    uint8_t msgType, respCmd, errorCode;
    uint16_t dataLen;
    uint8_t data[40];
    bool gotFrame = pumpRx(msgType, respCmd, errorCode, data, dataLen, sizeof(data));

    switch (_state) {
        case State::IDLE:
            break; // очікуємо startScan()/triggerReset() ззовні; сторонні кадри (NOTE тощо) ігноруються

        case State::VERIFY_WAIT:
            if (gotFrame && msgType == FRM1213_MSG_REPLY && respCmd == FRM1213_CMD_VERIFY) {
                if (errorCode == FRM1213_ERR_OK && dataLen >= 2) {
                    _lastFaceId = ((uint16_t)data[0] << 8) | data[1]; // MatchID, big-endian
                    _pendingEvent = Frm1213Event::MATCHED;
                } else {
                    _pendingEvent = Frm1213Event::NO_MATCH;
                }
                _state = State::IDLE;
            } else if (millis() >= _deadline) {
                _pendingEvent = Frm1213Event::TIMEOUT;
                beginReset(); // 5с без відповіді - модуль явно завис, м'який ресет (вимога ТЗ)
            }
            break;

        case State::RESET_WAIT_ACK:
            if ((gotFrame && msgType == FRM1213_MSG_REPLY && respCmd == FRM1213_CMD_STANDBY)
                || millis() >= _deadline) {
                // Ack прийнято або дедлайн вичерпано - в обох випадках рухаємось далі,
                // щоб не зависнути назавжди на непідтвердженій поведінці модуля.
                _deadline = millis() + _resetReadyWaitMs;
                _state = State::RESET_WAIT_READY;
            }
            break;

        case State::RESET_WAIT_READY:
            // Даташит підтверджує NID_READY лише на power-on, не на команду reset.
            // Тому будь-який NOTE тут трактуємо як "готовий", але не блокуємось назавжди,
            // якщо модуль взагалі його не надішле - дедлайн все одно завершить очікування.
            if ((gotFrame && msgType == FRM1213_MSG_NOTE) || millis() >= _deadline) {
                _pendingEvent = Frm1213Event::RESET_DONE;
                _state = State::IDLE;
            }
            break;
    }
}

void Frm1213Driver::sendCommand(uint8_t cmd, const uint8_t* data, uint16_t len) {
    uint8_t buf[FRM1213_MAX_FRAME];
    buf[0] = FRM1213_HDR0;
    buf[1] = FRM1213_HDR1;
    buf[2] = cmd;
    buf[3] = (uint8_t)(len >> 8);
    buf[4] = (uint8_t)len;
    if (len > 0) memcpy(buf + 5, data, len);
    buf[5 + len] = frm1213Checksum(buf, 5 + len + 1);
    _uart.write(buf, 5 + len + 1);
}

bool Frm1213Driver::pumpRx(uint8_t& msgType, uint8_t& respCmd, uint8_t& errorCode,
                            uint8_t* outData, uint16_t& outDataLen, uint16_t maxOutData) {
    while (_uart.available()) {
        uint8_t b = (uint8_t)_uart.read();
        switch (_rxState) {
            case RxState::HDR0:
                if (b == FRM1213_HDR0) _rxState = RxState::HDR1;
                break;
            case RxState::HDR1:
                _rxState = (b == FRM1213_HDR1) ? RxState::MSGTYPE : RxState::HDR0;
                break;
            case RxState::MSGTYPE:
                _rxMsgType = b;
                _rxState = RxState::LENHI;
                break;
            case RxState::LENHI:
                _rxLen = (uint16_t)b << 8;
                _rxState = RxState::LENLO;
                break;
            case RxState::LENLO:
                _rxLen |= b;
                if (_rxLen < 2 || _rxLen > FRM1213_MAX_FRAME - 6) {
                    _rxState = RxState::HDR0; // неможлива довжина - кадр пошкоджено
                    break;
                }
                _rxIndex = 0;
                _rxState = RxState::PAYLOAD;
                break;
            case RxState::PAYLOAD:
                _rxPayload[_rxIndex++] = b;
                if (_rxIndex >= _rxLen) _rxState = RxState::CRC;
                break;
            case RxState::CRC: {
                _rxState = RxState::HDR0; // готові до наступного кадру незалежно від результату
                uint8_t frame[FRM1213_MAX_FRAME];
                frame[0] = FRM1213_HDR0;
                frame[1] = FRM1213_HDR1;
                frame[2] = _rxMsgType;
                frame[3] = (uint8_t)(_rxLen >> 8);
                frame[4] = (uint8_t)_rxLen;
                memcpy(frame + 5, _rxPayload, _rxLen);
                frame[5 + _rxLen] = b;
                if (frm1213Checksum(frame, 5 + _rxLen + 1) != b) break; // checksum не збігся

                msgType = _rxMsgType;
                respCmd = _rxPayload[0];
                errorCode = _rxPayload[1];
                outDataLen = _rxLen - 2;
                if (outDataLen > maxOutData) outDataLen = maxOutData;
                memcpy(outData, _rxPayload + 2, outDataLen);
                return true;
            }
        }
    }
    return false;
}
