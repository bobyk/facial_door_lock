#include "FaceRecognizer.h"
#include <string.h>

static uint8_t checksum(const uint8_t* frame, size_t len) {
    // GetParityCheck (документ, розд. 8.4): XOR всього, крім заголовка (2 байти) і самого checksum.
    uint8_t c = 0;
    for (size_t i = 2; i + 1 < len; ++i) c ^= frame[i];
    return c;
}

bool FRM1213Recognizer::begin() {
    _uart.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    delay(450); // модуль ігнорує команди перші ~400мс після живлення (розд. 3.2)
    while (_uart.available()) _uart.read();

    sendCommand(CMD_GET_STATUS, nullptr, 0);
    uint8_t errorCode;
    uint16_t dataLen;
    uint8_t data[8];
    return waitReply(millis() + 500, CMD_GET_STATUS, errorCode, data, dataLen, sizeof(data));
}

FaceResult FRM1213Recognizer::tryRecognize(uint32_t timeoutMs) {
    // Поле "matching timeout" в документі - 2 байти big-endian, одиниці виміру не вказані явно.
    // Реальний дедлайн все одно контролюємо власним millis() нижче, тож навіть якщо припущення
    // про одиниці (секунди) хибне, довше за timeoutMs очікувати не будемо.
    uint16_t timeoutUnits = (uint16_t)max((uint32_t)1, timeoutMs / 1000);
    uint8_t payload[2] = { (uint8_t)(timeoutUnits >> 8), (uint8_t)timeoutUnits };
    sendCommand(CMD_MATCH, payload, sizeof(payload));

    uint32_t deadline = millis() + timeoutMs;
    uint8_t errorCode;
    uint16_t dataLen;
    uint8_t data[40];
    if (!waitReply(deadline, CMD_MATCH, errorCode, data, dataLen, sizeof(data))) {
        sendCommand(CMD_STANDBY, nullptr, 0); // перериваємо скан на модулі, коли наш дедлайн сплив першим
        return FaceResult::TIMEOUT;
    }

    if (errorCode == ERR_OK) return FaceResult::RECOGNIZED;
    if (errorCode == ERR_TIMEOUT) return FaceResult::TIMEOUT;
    return FaceResult::NOT_RECOGNIZED;
}

bool FRM1213Recognizer::enrollFace(const char* name, uint8_t faceDir, uint8_t timeoutSec,
                                    uint16_t& outUserId, bool isAdmin, bool allowDuplicate,
                                    uint8_t enrollType) {
    // Порядок полів підтверджений по прикладу відповіді 0x26 (розд. 5.6, Табл. 5.16-5.18):
    // is_admin(1) + username(32) + face_dir(1) + enroll_type(1) + allow_duplicate(1) + timeout(1) + reserved(3)
    uint8_t payload[40] = {0};
    payload[0] = isAdmin ? 0x01 : 0x00;
    strncpy((char*)&payload[1], name, 32);
    payload[33] = faceDir;
    payload[34] = enrollType;
    payload[35] = allowDuplicate ? 0x01 : 0x00;
    payload[36] = timeoutSec;
    // payload[37..39] - reserved, лишаємо 0x00

    sendCommand(CMD_ENROLL, payload, sizeof(payload));

    uint8_t errorCode;
    uint16_t dataLen;
    uint8_t data[8];
    uint32_t deadline = millis() + (uint32_t)timeoutSec * 1000 + 1000;
    if (!waitReply(deadline, CMD_ENROLL, errorCode, data, dataLen, sizeof(data))) return false;
    if (errorCode != ERR_OK || dataLen < 2) return false;

    outUserId = ((uint16_t)data[0] << 8) | data[1];
    return true;
}

bool FRM1213Recognizer::deleteFace(uint16_t userId) {
    uint8_t payload[2] = { (uint8_t)(userId >> 8), (uint8_t)userId };
    sendCommand(CMD_DELETE_ID, payload, sizeof(payload));

    uint8_t errorCode;
    uint16_t dataLen;
    uint8_t data[4];
    if (!waitReply(millis() + 500, CMD_DELETE_ID, errorCode, data, dataLen, sizeof(data))) return false;
    return errorCode == ERR_OK;
}

bool FRM1213Recognizer::deleteAllFaces() {
    sendCommand(CMD_DELETE_ALL, nullptr, 0);

    uint8_t errorCode;
    uint16_t dataLen;
    uint8_t data[4];
    if (!waitReply(millis() + 500, CMD_DELETE_ALL, errorCode, data, dataLen, sizeof(data))) return false;
    return errorCode == ERR_OK;
}

void FRM1213Recognizer::sendCommand(uint8_t cmd, const uint8_t* data, uint16_t len) {
    uint8_t buf[MAX_FRAME];
    buf[0] = HDR0;
    buf[1] = HDR1;
    buf[2] = cmd;
    buf[3] = (uint8_t)(len >> 8);
    buf[4] = (uint8_t)len;
    if (len > 0) memcpy(buf + 5, data, len);
    buf[5 + len] = checksum(buf, 5 + len + 1);
    _uart.write(buf, 5 + len + 1);
}

bool FRM1213Recognizer::readByte(uint32_t deadlineMs, uint8_t& out) {
    while (!_uart.available()) {
        if (millis() >= deadlineMs) return false;
    }
    out = (uint8_t)_uart.read();
    return true;
}

bool FRM1213Recognizer::readFrame(uint32_t deadlineMs, uint8_t& msgType, uint8_t& respCmd,
                                   uint8_t& errorCode, uint8_t* outData, uint16_t& outDataLen,
                                   uint16_t maxOutData) {
    uint8_t b;
    // синхронізація на заголовок 0xEF 0xAA - зайві байти між кадрами відкидаємо
    do {
        if (!readByte(deadlineMs, b)) return false;
    } while (b != HDR0);
    if (!readByte(deadlineMs, b) || b != HDR1) return false;

    uint8_t frame[MAX_FRAME];
    frame[0] = HDR0;
    frame[1] = HDR1;

    if (!readByte(deadlineMs, msgType)) return false;
    frame[2] = msgType;

    uint8_t lenHi, lenLo;
    if (!readByte(deadlineMs, lenHi) || !readByte(deadlineMs, lenLo)) return false;
    frame[3] = lenHi;
    frame[4] = lenLo;
    uint16_t len = ((uint16_t)lenHi << 8) | lenLo; // respCmd(1) + errorCode(1) + N2

    if (len < 2 || len > MAX_FRAME - 6) return false; // захист від переповнення буфера

    for (uint16_t i = 0; i < len; ++i) {
        if (!readByte(deadlineMs, frame[5 + i])) return false;
    }
    uint8_t chk;
    if (!readByte(deadlineMs, chk)) return false;
    frame[5 + len] = chk;

    if (checksum(frame, 5 + len + 1) != chk) return false;

    respCmd = frame[5];
    errorCode = frame[6];
    outDataLen = len - 2;
    if (outDataLen > maxOutData) outDataLen = maxOutData;
    memcpy(outData, frame + 7, outDataLen);
    return true;
}

bool FRM1213Recognizer::waitReply(uint32_t deadlineMs, uint8_t expectedCmd, uint8_t& errorCode,
                                   uint8_t* outData, uint16_t& outDataLen, uint16_t maxOutData) {
    while (millis() < deadlineMs) {
        uint8_t msgType, respCmd;
        if (!readFrame(deadlineMs, msgType, respCmd, errorCode, outData, outDataLen, maxOutData)) {
            return false;
        }
        if (msgType == MSG_REPLY && respCmd == expectedCmd) return true;
        // MSG_NOTE (проміжні сповіщення під час скану/реєстрації) чи чужа відповідь - чекаємо далі
    }
    return false;
}
