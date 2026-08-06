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

// !!! УВАГА: точний бінарний протокол команд HLK-FRM1213 публічно не підтверджений.
// Офіційний PDF з протоколом не знайдено у відкритому доступі (на відміну від LD2410/LD2450).
// Треба або запросити SDK/протокол у продавця/Hi-Link support (h.hlktech.com),
// або зняти обмін байтами логічним аналізатором між модулем і фірмовою PC-утилітою.
// Нижче - лише каркас під UART, команди-заглушки з TODO.
class FRM1213Recognizer : public IFaceRecognizer {
public:
    explicit FRM1213Recognizer(HardwareSerial& uart, uint32_t baud = 115200)
        : _uart(uart), _baud(baud) {}

    bool begin() override {
        _uart.begin(_baud);
        return true; // TODO: замінити на реальний handshake/перевірку модуля, коли буде протокол
    }

    FaceResult tryRecognize(uint32_t timeoutMs) override {
        // TODO: відправити реальну команду "розпізнати" замість заглушки нижче
        // _uart.write(CMD_RECOGNIZE, sizeof(CMD_RECOGNIZE));

        uint32_t start = millis();
        while (millis() - start < timeoutMs) {
            if (_uart.available()) {
                // TODO: розпарсити реальний формат відповіді модуля (заголовок/довжина/checksum)
                uint8_t b = _uart.read();
                (void)b;
                // Поки що - непрацююча заглушка, щоб FSM компілювався і тестувався окремо.
                return FaceResult::NOT_RECOGNIZED;
            }
        }
        return FaceResult::TIMEOUT;
    }

private:
    HardwareSerial& _uart;
    uint32_t _baud;
};
