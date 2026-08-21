#pragma once
#include <Arduino.h>
#include <string.h>
#include "Crypto.h"

// Application-level message IDs exchanged between OUTER and INNER over Link.
// Payload layouts (all multi-byte fields little-endian unless noted):
//   MSG_REQ        : no payload - OUTER asks INNER to start an authorization round.
//   MSG_NONCE      : 8 bytes    - INNER -> OUTER, random challenge.
//   MSG_AUTH       : 13 bytes   - OUTER -> INNER, face_id(1) + counter(4,LE) + tag(8).
//   MSG_UNLOCK_OK  : no payload - INNER -> OUTER, authorization accepted, lock driven.
//   MSG_TAMPER     : no payload - OUTER -> INNER, enclosure lid opened.
//   MSG_PAIR_KEY   : 32 bytes   - INNER -> OUTER, new shared key (pairing window only).
//   MSG_PAIR_ACK   : no payload - OUTER -> INNER, key stored, pairing may finalize.
//   MSG_PAIR_PIN   : 32 bytes   - OUTER -> INNER, SHA-256(PIN) captured during pairing window.
//   MSG_HEARTBEAT  : no payload - OUTER -> INNER, periodic liveness signal.
enum : uint8_t {
    MSG_REQ = 0x01,
    MSG_NONCE = 0x02,
    MSG_AUTH = 0x03,
    MSG_UNLOCK_OK = 0x04,
    MSG_TAMPER = 0x05,
    MSG_PAIR_KEY = 0x06,
    MSG_PAIR_ACK = 0x07,
    MSG_PAIR_PIN = 0x08,
    MSG_HEARTBEAT = 0x09,
};

static constexpr uint8_t NONCE_LEN = 8;
static constexpr uint8_t TAG_LEN = 8;
static constexpr uint8_t KEY_LEN = 32;
static constexpr uint8_t PIN_LEN = 6; // довжина PIN у цифрах

// Зарезервований id, що позначає "це PIN-авторизація, не обличчя" в AUTH-повідомленні.
// Реальні face_id від FRM1213 лежать в 1..100 (див. Face count у даташиті), тож 0xFF безпечний.
static constexpr uint8_t PIN_SENTINEL_ID = 0xFF;

static constexpr uint8_t AUTH_PAYLOAD_LEN_FACE = 1 + 4 + TAG_LEN;             // id + counter + tag
static constexpr uint8_t AUTH_PAYLOAD_LEN_PIN = 1 + 4 + TAG_LEN + PIN_LEN;    // + PIN цифри

struct AuthPayload {
    uint8_t id;             // face_id (1..100) або PIN_SENTINEL_ID
    uint32_t counter;
    uint8_t tag[TAG_LEN];
    uint8_t pin[PIN_LEN];   // валідний лише якщо id == PIN_SENTINEL_ID
};

inline uint8_t encodeAuthPayload(const AuthPayload& in, uint8_t out[AUTH_PAYLOAD_LEN_PIN]) {
    out[0] = in.id;
    out[1] = (uint8_t)(in.counter);
    out[2] = (uint8_t)(in.counter >> 8);
    out[3] = (uint8_t)(in.counter >> 16);
    out[4] = (uint8_t)(in.counter >> 24);
    memcpy(out + 5, in.tag, TAG_LEN);
    if (in.id != PIN_SENTINEL_ID) return AUTH_PAYLOAD_LEN_FACE;
    memcpy(out + 5 + TAG_LEN, in.pin, PIN_LEN);
    return AUTH_PAYLOAD_LEN_PIN;
}

inline bool decodeAuthPayload(const uint8_t* in, uint8_t len, AuthPayload& out) {
    if (len != AUTH_PAYLOAD_LEN_FACE && len != AUTH_PAYLOAD_LEN_PIN) return false;
    out.id = in[0];
    out.counter = (uint32_t)in[1] | ((uint32_t)in[2] << 8) | ((uint32_t)in[3] << 16) | ((uint32_t)in[4] << 24);
    memcpy(out.tag, in + 5, TAG_LEN);
    if (out.id == PIN_SENTINEL_ID) {
        if (len != AUTH_PAYLOAD_LEN_PIN) return false;
        memcpy(out.pin, in + 5 + TAG_LEN, PIN_LEN);
    }
    return true;
}

// Байтова послідовність, що йде в HMAC для TAG: NONCE || id || counter(LE) || [pin, лише для PIN-шляху].
// Спільна для OUTER (рахує TAG) і INNER (перевіряє TAG) - розбіжність тут = миттєвий desync.
inline uint8_t buildAuthMacInput(const uint8_t nonce[NONCE_LEN], uint8_t id, uint32_t counter,
                                  const uint8_t* pin, uint8_t out[NONCE_LEN + AUTH_PAYLOAD_LEN_PIN - TAG_LEN]) {
    memcpy(out, nonce, NONCE_LEN);
    out[NONCE_LEN] = id;
    out[NONCE_LEN + 1] = (uint8_t)(counter);
    out[NONCE_LEN + 2] = (uint8_t)(counter >> 8);
    out[NONCE_LEN + 3] = (uint8_t)(counter >> 16);
    out[NONCE_LEN + 4] = (uint8_t)(counter >> 24);
    if (id != PIN_SENTINEL_ID) return NONCE_LEN + 5;
    memcpy(out + NONCE_LEN + 5, pin, PIN_LEN);
    return NONCE_LEN + 5 + PIN_LEN;
}

// K(n+1) = HMAC-SHA256(K(n), "ratchet" || counter_at_unlock). Shared helper so a
// byte-order or format slip can't silently desync OUTER and INNER's ratchets.
inline void deriveNextKey(Crypto& crypto, const uint8_t currentKey[KEY_LEN],
                           uint32_t counterAtUnlock, uint8_t outNewKey[KEY_LEN]) {
    uint8_t msg[7 + 4];
    memcpy(msg, "ratchet", 7);
    msg[7] = (uint8_t)(counterAtUnlock);
    msg[8] = (uint8_t)(counterAtUnlock >> 8);
    msg[9] = (uint8_t)(counterAtUnlock >> 16);
    msg[10] = (uint8_t)(counterAtUnlock >> 24);
    crypto.hmacSha256(currentKey, KEY_LEN, msg, sizeof(msg), outNewKey);
}
