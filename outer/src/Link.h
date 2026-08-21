#pragma once
#include <Arduino.h>

static constexpr uint8_t LINK_MAX_PAYLOAD = 32; // найбільше повідомлення - PAIR_KEY (32 байти)

// Транспортно-незалежний канал повідомлень між OUTER і INNER.
// Зараз реалізація - UART (UartLink), пізніше можна підмінити на CAN/TWAI
// без зміни жодного рядка бізнес-логіки (OuterController/InnerController),
// оскільки вони працюють лише з send()/poll() і не знають про фізичний транспорт.
class Link {
public:
    virtual bool begin() = 0;
    virtual bool send(uint8_t type, const uint8_t* payload, uint8_t len) = 0;
    // Неблокуючий опитувальний прийом. true, якщо прийнято ціле повідомлення з коректною checksum.
    virtual bool poll(uint8_t& type, uint8_t* payload, uint8_t& len, uint8_t maxLen) = 0;
    // Час останнього валідного прийнятого кадру - для контролю heartbeat/обриву зв'язку.
    virtual uint32_t lastRxMillis() const = 0;
    virtual ~Link() = default;
};
