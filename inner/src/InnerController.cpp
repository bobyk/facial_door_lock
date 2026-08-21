#include "InnerController.h"
#include "config.h"
#include "LinkFrame.h"
#include <WiFi.h>
#include <string.h>

InnerController::InnerController(Link& link, Crypto& crypto, NvsStore& nvs, LockDriver& lock,
                                  ToFPresenceSensor& tof, RTCModule& rtc, uint8_t maintButtonPin)
    : _link(link), _crypto(crypto), _nvs(nvs), _lock(lock), _tof(tof), _rtc(rtc),
      _maintButtonPin(maintButtonPin) {}

void InnerController::sendMessage(uint8_t type, const uint8_t* payload, uint8_t len) {
    uint8_t frame[LINK_FRAME_MAX];
    uint8_t frameLen = encodeLinkFrame(type, payload, len, frame);
    _link.send(frame, frameLen);
}

bool InnerController::recvMessage(uint8_t& type, uint8_t* payload, uint8_t& len, uint8_t maxLen) {
    uint8_t buf[LINK_FRAME_MAX];
    size_t bufLen = sizeof(buf);
    if (!_link.receive(buf, bufLen)) return false;
    return decodeLinkFrame(buf, bufLen, type, payload, len, maxLen);
}

void InnerController::begin() {
    pinMode(_maintButtonPin, INPUT_PULLUP);
    _link.begin();
    _lock.begin();
    _tof.begin();
    _rtc.begin();
    _bootMs = millis();
    Serial.print("[LINK] using ");
    Serial.print(_link.name());
    Serial.println(" transport");

    // Перевірка "кнопка утримана на старті" - одноразово, до входу в loop().
    bool held = (digitalRead(_maintButtonPin) == LOW);
    if (held) {
        uint32_t start = millis();
        while (digitalRead(_maintButtonPin) == LOW && millis() - start < PAIRING_BOOT_HOLD_MS) {}
        held = (digitalRead(_maintButtonPin) == LOW) && (millis() - start >= PAIRING_BOOT_HOLD_MS);
    }
    if (held) {
        logEvent("boot button held - entering pairing window, generating new key");
        _crypto.randomBytes(_pairingKey, KEY_LEN);
        _pairingChannel = WIFI_CHANNEL; // INNER обирає канал ESP-NOW для пари, повідомляє його OUTER
        _pairingDeadline = millis() + PAIRING_WINDOW_MS;
        _state = State::PAIRING;
        return;
    }

    _state = _nvs.lockedOut() ? State::LOCKED_OUT : State::IDLE;
    if (_state == State::LOCKED_OUT) logEvent("boot: tamper lockout persisted from previous session");
}

void InnerController::logEvent(const char* msg) {
    char ts[24];
    _rtc.formatTimestamp(ts, sizeof(ts));
    Serial.print('[');
    Serial.print(ts);
    Serial.print("] ");
    Serial.println(msg);
}

void InnerController::update() {
    checkHeartbeat();
    checkMaintButton();
    checkTof(); // egress unlock - unconditional, checked every tick regardless of state

    uint8_t type, payload[LINK_MAX_PAYLOAD], len;
    bool got = recvMessage(type, payload, len, sizeof(payload));

    if (got && type == MSG_TAMPER) {
        handleTamper();
        return; // тампер - найвищий пріоритет цього тіку
    }

    switch (_state) {
        case State::PAIRING:
            tickPairing(got, type, payload, len);
            break;

        case State::LOCKED_OUT:
            break; // REQ/AUTH ігноруються повністю; вихід лише через maint-button (checkMaintButton)

        case State::IDLE:
            if (got && type == MSG_REQ) handleReq();
            break;

        case State::AWAITING_AUTH:
            if (got && type == MSG_REQ) {
                handleReq(); // OUTER повторив запит - переозброюємось свіжим nonce
            } else if (got && type == MSG_AUTH) {
                handleAuth(payload, len);
            } else if (millis() - _nonceIssuedMs > NONCE_VALIDITY_MS) {
                _state = State::IDLE; // nonce протух без спроби - це не невдала спроба
            }
            break;
    }
}

void InnerController::checkTof() {
    bool now = _tof.isPresent();
    if (now && !_tofWasPresent) {
        logEvent("internal presence detected - unlocking without authorization (egress)");
        _lock.open();
    }
    _tofWasPresent = now;
}

void InnerController::checkMaintButton() {
    bool held = (digitalRead(_maintButtonPin) == LOW);
    if (held && !_maintHeld) {
        _maintHeld = true;
        _maintHoldStartMs = millis();
    } else if (!held) {
        _maintHeld = false;
    } else if (_maintHeld && _state == State::LOCKED_OUT
               && millis() - _maintHoldStartMs >= TAMPER_CLEAR_HOLD_MS) {
        logEvent("tamper lockout cleared via maintenance button (3s hold)");
        _nvs.setLockedOut(false);
        _state = State::IDLE;
        _maintHeld = false; // не спрацьовувати повторно, поки кнопку не відпустять
    }
}

void InnerController::checkHeartbeat() {
    if (millis() - _bootMs < HEARTBEAT_MISSING_MS) return; // грейс-період одразу після старту
    if (!_link.isAlive()) {
        if (!_heartbeatMissingLogged) {
            logEvent("OUTER heartbeat missing >10s - suspicious (not locking out)");
            _heartbeatMissingLogged = true;
        }
    } else {
        _heartbeatMissingLogged = false;
    }
}

void InnerController::handleTamper() {
    if (_state != State::LOCKED_OUT) logEvent("TAMPER received from OUTER - entering LOCKED_OUT");
    _nvs.setLockedOut(true);
    _state = State::LOCKED_OUT;
}

void InnerController::handleReq() {
    if (!_nvs.isPaired()) { logEvent("REQ ignored: not paired"); return; }
    if (millis() < _rateLimitBlockedUntilMs) { logEvent("REQ ignored: rate limited"); return; }

    // Crypto::randomBytes() сам вирішує, чи потрібен bootloader_random_enable()
    // (лише коли радіо вимкнене - див. коментар в CryptoMbedTLS.cpp).
    _crypto.randomBytes(_nonce, NONCE_LEN);
    _nonceIssuedMs = millis();
    sendMessage(MSG_NONCE, _nonce, NONCE_LEN);
    _state = State::AWAITING_AUTH;
}

void InnerController::registerFailure(const char* reason) {
    _consecutiveFails++;
    logEvent(reason);
    if (_consecutiveFails >= RATE_LIMIT_MAX_FAILS) {
        _rateLimitBlockedUntilMs = millis() + RATE_LIMIT_BLOCK_MS;
        logEvent("rate limit engaged (5 consecutive failures)");
    }
}

void InnerController::handleAuth(const uint8_t* payload, uint8_t len) {
    // Nonce одноразовий - незалежно від результату цієї спроби, повертаємось в IDLE
    // (нова спроба вимагатиме нового REQ і нового nonce).
    _state = State::IDLE;

    AuthPayload auth;
    if (!decodeAuthPayload(payload, len, auth)) { registerFailure("AUTH rejected: malformed payload"); return; }

    if (millis() - _nonceIssuedMs > NONCE_VALIDITY_MS) {
        registerFailure("AUTH rejected: nonce expired");
        return;
    }
    if (auth.counter <= _nvs.counter()) {
        registerFailure("AUTH rejected: counter not strictly increasing");
        return;
    }
    if (auth.id == PIN_SENTINEL_ID) {
        if (!_nvs.hasPinHash()) { registerFailure("AUTH rejected: no PIN configured"); return; }
        uint8_t pinHash[32];
        _crypto.sha256(auth.pin, PIN_LEN, pinHash);
        if (!constantTimeEqual(pinHash, _nvs.pinHash(), sizeof(pinHash))) {
            registerFailure("AUTH rejected: bad PIN");
            return;
        }
    }

    // Лічильник - глобальний, не прив'язаний до транспорту (nvs.counter()), тож
    // кадр, перехоплений на одному транспорті, неможливо повторно застосувати
    // через інший - "replay across transports must be impossible" виконується
    // автоматично самою структурою, без додаткової логіки тут.
    uint8_t macIn[NONCE_LEN + AUTH_PAYLOAD_LEN_PIN - TAG_LEN];
    uint8_t macInLen = buildAuthMacInput(_nonce, auth.id, auth.counter, auth.pin, macIn);

    uint8_t expectedCur[32];
    _crypto.hmacSha256(_nvs.keyCurrent(), KEY_LEN, macIn, macInLen, expectedCur);
    bool matchCur = constantTimeEqual(expectedCur, auth.tag, TAG_LEN);

    bool matchPrev = false;
    if (!matchCur && _nvs.hasPreviousKey()) {
        uint8_t expectedPrev[32];
        _crypto.hmacSha256(_nvs.keyPrevious(), KEY_LEN, macIn, macInLen, expectedPrev);
        matchPrev = constantTimeEqual(expectedPrev, auth.tag, TAG_LEN);
    }

    if (!matchCur && !matchPrev) { registerFailure("AUTH rejected: bad tag"); return; }

    // Успіх - під поточним або попереднім (на одне покоління назад) ключем.
    _consecutiveFails = 0;
    _nvs.setCounter(auth.counter);
    logEvent(matchCur ? "unlock granted (current key)" : "unlock granted (previous key - resyncing OUTER)");
    _lock.open();

    if (matchCur) {
        // Ratchet просувається лише коли реально під поточним ключем - якщо це був
        // matchPrev (desync recovery), INNER лишається на тому ж поколінні і просто
        // повторно надсилає UNLOCK_OK, щоб OUTER наздогнав самостійною ротацією.
        // Ratchet-стан транспортно-незалежний - зміна транспорту сама по собі
        // ніколи не скидає й не пропускає покоління.
        uint8_t newKey[KEY_LEN];
        deriveNextKey(_crypto, _nvs.keyCurrent(), auth.counter, newKey);
        _nvs.rotateKey(newKey);
    }

    sendMessage(MSG_UNLOCK_OK, nullptr, 0);
}

void InnerController::tickPairing(bool got, uint8_t type, const uint8_t* payload, uint8_t len) {
    static uint32_t lastBroadcastMs = 0;
    uint32_t now = millis();

    if (got && type == MSG_PAIR_ACK && len == 6) {
        _nvs.commitPairing(_pairingKey, payload, _pairingChannel); // payload = OUTER's MAC
        logEvent("pairing complete (key + peer MAC exchanged)");
        _consecutiveFails = 0;
        _rateLimitBlockedUntilMs = 0;
        _state = State::IDLE;
        return;
    }
    if (got && type == MSG_PAIR_PIN && len == 32) {
        _nvs.setPinHash(payload);
        logEvent("PIN hash received during pairing");
    }

    if (now - lastBroadcastMs >= 500) {
        uint8_t bundle[KEY_LEN + 6 + 1];
        memcpy(bundle, _pairingKey, KEY_LEN);
        WiFi.macAddress(bundle + KEY_LEN); // INNER's own STA MAC, не потребує WiFi.mode(WIFI_STA)
        bundle[KEY_LEN + 6] = _pairingChannel;
        sendMessage(MSG_PAIR_KEY, bundle, sizeof(bundle));
        lastBroadcastMs = now;
    }

    if (now >= _pairingDeadline) {
        logEvent("pairing window closed without ack from OUTER");
        _state = _nvs.lockedOut() ? State::LOCKED_OUT : State::IDLE;
    }
}
