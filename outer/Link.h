#pragma once
#include <Arduino.h>

static constexpr uint8_t LINK_MAX_PAYLOAD = 40; // найбільше повідомлення - PAIR_KEY+MAC+channel (39)

enum class TransportMode : uint8_t { AUTO = 0, UART = 1, ESPNOW = 2 };

// Транспортно-незалежний канал сирих кадрів між OUTER і INNER. Реалізації:
// UartLink (кабель, штатний) і EspNowLink (радіо, резервний/бенчовий).
// Обидві приймають/віддають вже повністю закодований кадр (type+len+payload+crc8,
// див. LinkFrame.h) як непрозорий байтовий буфер - жодна не знає про типи
// повідомлень, HMAC, nonce чи лічильник. Транспорт не несе відповідальності за
// безпеку - лише за доставку байтів; це на рівні застосунку (Outer/InnerController).
class Link {
public:
    virtual bool begin() = 0;
    virtual bool send(const uint8_t* data, size_t len) = 0;
    // Неблокуючий прийом. true, якщо в buf записано ціле валідне повідомлення
    // (в len - фактична довжина). Викликати щотік з update().
    virtual bool receive(uint8_t* buf, size_t& len) = 0;
    // Чи бачили напарника нещодавно (поріг задається конкретній реалізації при конструюванні).
    virtual bool isAlive() = 0;
    virtual const char* name() = 0;
    virtual ~Link() = default;
};
