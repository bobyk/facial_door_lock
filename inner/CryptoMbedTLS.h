#pragma once
#include "Crypto.h"

// Програмний бекенд Crypto: HMAC-SHA256 через mbedtls (входить в ESP32 Arduino
// core, окрема бібліотека не потрібна), RNG через апаратний TRNG ESP32-S3.
class CryptoMbedTLS : public Crypto {
public:
    void hmacSha256(const uint8_t* key, size_t keyLen,
                     const uint8_t* data, size_t dataLen,
                     uint8_t out[32]) override;
    void sha256(const uint8_t* data, size_t dataLen, uint8_t out[32]) override;
    void randomBytes(uint8_t* buf, size_t len) override;
};
