#include "CryptoMbedTLS.h"
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <esp_random.h>
#include <bootloader_random.h>

void CryptoMbedTLS::hmacSha256(const uint8_t* key, size_t keyLen,
                                const uint8_t* data, size_t dataLen,
                                uint8_t out[32]) {
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(info, key, keyLen, data, dataLen, out);
}

void CryptoMbedTLS::sha256(const uint8_t* data, size_t dataLen, uint8_t out[32]) {
    mbedtls_sha256(data, dataLen, out, 0); // 0 = SHA-256, не SHA-224
}

void CryptoMbedTLS::randomBytes(uint8_t* buf, size_t len) {
    // Проєкт ніколи не вмикає Wi-Fi/BT (поза межами ТЗ), тож радіо завжди
    // вимкнене - апаратний TRNG потребує bootloader_random_enable(), інакше
    // ентропія деградує без RF-шуму як джерела. Вимикаємо одразу після використання,
    // як і задокументовано в ESP-IDF (bootloader_random.h).
    bootloader_random_enable();
    esp_fill_random(buf, len);
    bootloader_random_disable();
}

bool constantTimeEqual(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}
