#pragma once
#include <Arduino.h>
#include "frm1213.h"

enum class Frm1213Event { NONE, MATCHED, NO_MATCH, TIMEOUT, RESET_DONE };

// Неблокуючий драйвер HLK-FRM1213: жодних delay()/спін-очікувань в update().
// Викликач керує через startScan()/triggerReset() і читає стан через
// lastEvent()/lastFaceId() з кожного тіку loop().
class Frm1213Driver {
public:
    Frm1213Driver(HardwareSerial& uart, uint8_t rxPin, uint8_t txPin,
                  uint32_t verifyTimeoutMs, uint32_t resetAckTimeoutMs, uint32_t resetReadyWaitMs,
                  uint32_t baud = 115200)
        : _uart(uart), _rxPin(rxPin), _txPin(txPin), _baud(baud),
          _verifyTimeoutMs(verifyTimeoutMs), _resetAckTimeoutMs(resetAckTimeoutMs),
          _resetReadyWaitMs(resetReadyWaitMs) {}

    bool begin();
    void update(); // викликати щотік з loop()

    bool isIdle() const { return _state == State::IDLE; }
    void startScan();     // кнопка сканування - без ефекту, якщо не IDLE
    void triggerReset();  // після успішного unlock - без ефекту, якщо не IDLE

    // Повертає і скидає останню подію (NONE, якщо нової немає).
    Frm1213Event lastEvent();
    uint16_t lastFaceId() const { return _lastFaceId; }

private:
    enum class State { IDLE, VERIFY_WAIT, RESET_WAIT_ACK, RESET_WAIT_READY };
    enum class RxState { HDR0, HDR1, MSGTYPE, LENHI, LENLO, PAYLOAD, CRC };

    HardwareSerial& _uart;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;
    uint32_t _verifyTimeoutMs, _resetAckTimeoutMs, _resetReadyWaitMs;

    State _state = State::IDLE;
    uint32_t _deadline = 0;
    Frm1213Event _pendingEvent = Frm1213Event::NONE;
    uint16_t _lastFaceId = 0;

    RxState _rxState = RxState::HDR0;
    uint8_t _rxMsgType = 0;
    uint16_t _rxLen = 0, _rxIndex = 0;
    uint8_t _rxPayload[FRM1213_MAX_FRAME];

    void sendCommand(uint8_t cmd, const uint8_t* data, uint16_t len);
    void beginReset();
    // Просуває парсер на всі байти, наявні зараз в UART. true = зібрано валідний кадр
    // (msgType/respCmd/errorCode/data заповнені), інакше - жодного або лише часткового кадру.
    bool pumpRx(uint8_t& msgType, uint8_t& respCmd, uint8_t& errorCode,
                uint8_t* outData, uint16_t& outDataLen, uint16_t maxOutData);
};
