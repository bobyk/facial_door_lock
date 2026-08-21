#pragma once
#include <Arduino.h>
#include <string.h>
#include "Link.h"

// Логічний кадр повідомлення - однаковий незалежно від транспорту (UART чи
// ESP-NOW), саме це дає "same framing" з вимоги: type(1) + len(1) + payload(len) + crc8(1).
// Викликач (Outer/InnerController) кодує/декодує кадр цими функціями і передає
// вже готові байти в Link::send()/отримує їх з Link::receive().
static constexpr uint8_t LINK_FRAME_OVERHEAD = 3; // type+len+crc8
static constexpr uint8_t LINK_FRAME_MAX = LINK_FRAME_OVERHEAD + LINK_MAX_PAYLOAD;

inline uint8_t linkFrameCrc8(const uint8_t* data, size_t len) {
    // CRC-8-CCITT (poly 0x07) - виявлення побитового шуму, не криптографічний
    // захист (той - на рівні HMAC в Protocol.h/controllers).
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

inline uint8_t encodeLinkFrame(uint8_t type, const uint8_t* payload, uint8_t len,
                                uint8_t out[LINK_FRAME_MAX]) {
    out[0] = type;
    out[1] = len;
    if (len > 0) memcpy(out + 2, payload, len);
    out[2 + len] = linkFrameCrc8(out, (size_t)2 + len);
    return (uint8_t)(2 + len + 1);
}

inline bool decodeLinkFrame(const uint8_t* in, size_t inLen, uint8_t& type,
                             uint8_t* payload, uint8_t& payloadLen, uint8_t maxPayload) {
    if (inLen < LINK_FRAME_OVERHEAD) return false;
    uint8_t len = in[1];
    if ((size_t)(2 + len + 1) != inLen) return false;
    if (linkFrameCrc8(in, (size_t)2 + len) != in[2 + len]) return false;

    type = in[0];
    payloadLen = (len < maxPayload) ? len : maxPayload;
    if (payloadLen > 0) memcpy(payload, in + 2, payloadLen);
    return true;
}
