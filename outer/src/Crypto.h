#pragma once
#include <Arduino.h>

// Абстракція над криптопримітивами -> дозволяє пізніше підмінити на апаратний
// бекенд (ATECC608 або ESP32-S3 eFuse HMAC), не чіпаючи виклики в бізнес-логіці.
class Crypto {
public:
    virtual void hmacSha256(const uint8_t* key, size_t keyLen,
                             const uint8_t* data, size_t dataLen,
                             uint8_t out[32]) = 0;
    // Незакейований SHA-256 - використовується лише для хешування PIN у NVS
    // (INNER ніколи не зберігає PIN у відкритому вигляді).
    virtual void sha256(const uint8_t* data, size_t dataLen, uint8_t out[32]) = 0;
    // Криптостійкі випадкові байти. Реалізація сама дбає про коректне джерело
    // ентропії (напр. вмикає апаратний RNG від радіошуму, якщо радіо вимкнене).
    virtual void randomBytes(uint8_t* buf, size_t len) = 0;
    virtual ~Crypto() = default;
};

// Порівняння за постійний час - захист від timing-атак на TAG. Це чиста логіка,
// не апаратний примітив, тому лишається вільною функцією поза Crypto-інтерфейсом.
bool constantTimeEqual(const uint8_t* a, const uint8_t* b, size_t len);
