#pragma once
#include <Arduino.h>

enum class FaceResult { NO_FACE, RECOGNIZED, NOT_RECOGNIZED, TIMEOUT };

// Абстракція -> легко підмінити на іншу камеру/модуль в майбутньому.
class IFaceRecognizer {
public:
    virtual bool begin() = 0;
    virtual FaceResult tryRecognize(uint32_t timeoutMs) = 0;
    virtual ~IFaceRecognizer() = default;
};

// HLK-FRM1213 (протокол ML-FRM/MRM series, User Agreement Document V1.1.2, отриманий від продавця).
// Кадр: 0xEF 0xAA <cmd/msgType> <len:2 BE> <payload> <checksum>.
// checksum = XOR усіх байтів між заголовком і самим checksum (GetParityCheck, розд. 8.4 документа).
// UART: 115200 8N1, напівдуплекс, модуль ігнорує команди перші ~400мс після подачі живлення (розд. 3.2).
class FRM1213Recognizer : public IFaceRecognizer {
public:
    FRM1213Recognizer(HardwareSerial& uart, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200)
        : _uart(uart), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

    bool begin() override;
    FaceResult tryRecognize(uint32_t timeoutMs) override;

    // Адміністративні функції поза FSM - для окремого сценарію реєстрації користувача.
    // faceDir: 0x01 прямий погляд / 0x10 вгору / 0x08 вниз / 0x04 ліворуч / 0x02 праворуч (розд. 5.15).
    bool enrollFace(const char* name, uint8_t faceDir, uint8_t timeoutSec, uint16_t& outUserId,
                    bool isAdmin = false, bool allowDuplicate = false, uint8_t enrollType = 0);
    bool deleteFace(uint16_t userId);
    bool deleteAllFaces();

private:
    static constexpr uint8_t HDR0 = 0xEF;
    static constexpr uint8_t HDR1 = 0xAA;
    static constexpr uint8_t CMD_STANDBY = 0x23;   // повернення модуля в очікування - підтверджено прикладом (розд. 5.1)
    static constexpr uint8_t CMD_GET_STATUS = 0x11;
    static constexpr uint8_t CMD_MATCH = 0x12;
    static constexpr uint8_t CMD_DELETE_ID = 0x20;
    static constexpr uint8_t CMD_DELETE_ALL = 0x21;
    static constexpr uint8_t CMD_ENROLL = 0x26;
    static constexpr uint8_t MSG_REPLY = 0x00;
    static constexpr uint8_t MSG_NOTE = 0x01;      // проміжні сповіщення під час скану/реєстрації - не відповідь
    static constexpr uint8_t ERR_OK = 0x00;
    static constexpr uint8_t ERR_TIMEOUT = 0x0D;
    static constexpr uint8_t MAX_FRAME = 64;       // з запасом понад найбільшу відому корисну відповідь (36 байт)

    HardwareSerial& _uart;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;

    void sendCommand(uint8_t cmd, const uint8_t* data, uint16_t len);
    bool readByte(uint32_t deadlineMs, uint8_t& out);
    // Читає один кадр відповіді модуля до deadlineMs. false = дедлайн вичерпано або checksum не збігся.
    bool readFrame(uint32_t deadlineMs, uint8_t& msgType, uint8_t& respCmd, uint8_t& errorCode,
                   uint8_t* outData, uint16_t& outDataLen, uint16_t maxOutData);
    // Чекає саме REPLY на конкретну команду (ігноруючи проміжні NOTE) до deadlineMs.
    bool waitReply(uint32_t deadlineMs, uint8_t expectedCmd, uint8_t& errorCode,
                   uint8_t* outData, uint16_t& outDataLen, uint16_t maxOutData);
};
