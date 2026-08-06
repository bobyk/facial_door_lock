#include "LD2420PresenceSensor.h"

bool LD2420PresenceSensor::begin() {
    _uart.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    return true; // TODO: реальний handshake/перевірка модуля, коли буде протокол
}

bool LD2420PresenceSensor::isPresent() {
    pollFrames();
    return _present;
}

void LD2420PresenceSensor::pollFrames() {
    // TODO: розпарсити реальний формат кадру LD2420 (заголовок/довжина/стан цілі/checksum).
    // Поки що лише вичитуємо буфер, щоб він не переповнювався; _present свідомо не
    // оновлюється - непрацююча заглушка (як FRM1213Recognizer::tryRecognize), щоб
    // FSM компілювався і тестувався окремо від апаратного протоколу.
    while (_uart.available()) {
        _uart.read();
    }
}
