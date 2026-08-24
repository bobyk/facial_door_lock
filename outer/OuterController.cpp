#include "OuterController.h"
#include "config.h"
#include "LinkFrame.h"
#include <WiFi.h>
#include <string.h>

OuterController::OuterController(Link& link, Crypto& crypto, NvsStore& nvs, Frm1213Driver& face,
                                  IPresenceSensor& presence, KeypadPCF8574& keypad, LedStrip& led,
                                  uint8_t scanButtonPin, uint8_t tamperPin)
    : _link(link), _crypto(crypto), _nvs(nvs), _face(face), _presence(presence),
      _keypad(keypad), _led(led), _scanButtonPin(scanButtonPin), _tamperPin(tamperPin) {}

void OuterController::sendMessage(uint8_t type, const uint8_t* payload, uint8_t len) {
    uint8_t frame[LINK_FRAME_MAX];
    uint8_t frameLen = encodeLinkFrame(type, payload, len, frame);
    _link.send(frame, frameLen);
}

bool OuterController::recvMessage(uint8_t& type, uint8_t* payload, uint8_t& len, uint8_t maxLen) {
    uint8_t buf[LINK_FRAME_MAX];
    size_t bufLen = sizeof(buf);
    if (!_link.receive(buf, bufLen)) return false;
    return decodeLinkFrame(buf, bufLen, type, payload, len, maxLen);
}

void OuterController::begin() {
    pinMode(_scanButtonPin, INPUT_PULLUP);
    pinMode(_tamperPin, INPUT_PULLUP);
    _lastTamperLevel = digitalRead(_tamperPin);

    _link.begin();
    _face.begin();
    _presence.begin();
    _led.begin();
    Serial.print("[LINK] using ");
    Serial.print(_link.name());
    Serial.println(" transport");

    _state = State::IDLE;
}

void OuterController::enterPairingNow() {
    Serial.println("[PAIR] entering pairing window (serial command)");
    _state = State::PAIRING;
    _pairingDeadline = millis() + PAIRING_WINDOW_MS;
}

void OuterController::update() {
    checkTamper();
    checkPairingHold();
    sendHeartbeatIfDue();
    _face.update();

    // Клавіатура опитується завжди, незалежно від _state - PIN можна вводити
    // (і # для закриття замку натиснути) будь-коли, не лише в PIN_ENTRY.
    // Виняток - PAIRING: там '#' підтверджує НАЛАШТУВАННЯ статичного PIN
    // (tickPairing сама читає клавіатуру), а не спробу авторизації.
    if (_state != State::PAIRING) handleKeypad();

    switch (_state) {
        case State::PAIRING: tickPairing(); break;
        case State::IDLE: tickIdle(); break;
        case State::FACE_SCANNING: tickFaceScanning(); break;
        case State::PIN_ENTRY: break; // лише LED-індикація; ввід обробляє handleKeypad() вище
        case State::WAIT_NONCE: tickWaitNonce(); break;
        case State::WAIT_UNLOCK_ACK: tickWaitUnlockAck(); break;
        case State::POST_UNLOCK_RESET: tickPostUnlockReset(); break;
    }
    _led.update();
}

void OuterController::checkTamper() {
    int level = digitalRead(_tamperPin);
    static uint32_t lastResendMs = 0;
    uint32_t now = millis();

    if (level == HIGH && _lastTamperLevel == LOW) {
        Serial.println("[TAMPER] enclosure opened - notifying INNER");
        sendMessage(MSG_TAMPER, nullptr, 0);
        lastResendMs = now;
    } else if (level == HIGH && now - lastResendMs >= 2000) {
        // Без ACK для TAMPER - періодично повторюємо, поки кришка відкрита,
        // на випадок якщо перший кадр загубився (актуально для обох транспортів).
        sendMessage(MSG_TAMPER, nullptr, 0);
        lastResendMs = now;
    }
    _lastTamperLevel = level;
}

void OuterController::checkPairingHold() {
    bool held = (digitalRead(_scanButtonPin) == LOW);
    if (held && !_scanButtonHeld) {
        _scanButtonHeld = true;
        _scanButtonHoldStartMs = millis();
    } else if (!held) {
        _scanButtonHeld = false;
    } else if (_scanButtonHeld && _state != State::PAIRING
               && millis() - _scanButtonHoldStartMs >= PAIRING_HOLD_MS) {
        enterPairingNow();
        _scanButtonHeld = false; // не спрацьовувати повторно, поки кнопку не відпустять
    }
}

void OuterController::sendHeartbeatIfDue() {
    uint32_t now = millis();
    if (now - _lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        sendMessage(MSG_HEARTBEAT, nullptr, 0);
        _lastHeartbeatMs = now;
    }
}

void OuterController::tickPairing() {
    uint8_t type, payload[LINK_MAX_PAYLOAD], len;
    if (recvMessage(type, payload, len, sizeof(payload)) && type == MSG_PAIR_KEY
        && len == KEY_LEN + 6 + 1) {
        const uint8_t* key = payload;
        const uint8_t* innerMac = payload + KEY_LEN;
        uint8_t channel = payload[KEY_LEN + 6];

        _nvs.commitPairing(key, innerMac, channel);

        uint8_t ownMac[6];
        WiFi.macAddress(ownMac); // не потребує WiFi.mode(WIFI_STA) - читає базовий MAC з efuse
        for (uint8_t i = 0; i < 3; ++i) sendMessage(MSG_PAIR_ACK, ownMac, sizeof(ownMac)); // без ack - невелика надлишковість
        Serial.println("[PAIR] key + peer MAC received and stored");
        _pinEntryLen = 0;
        _state = State::IDLE;
        return;
    }

    // В тому ж вікні можна одразу задати/оновити PIN з клавіатури (цифри + '#' підтвердити).
    char k = _keypad.scan();
    if (k == '*') {
        _pinEntryLen = 0;
    } else if (k == '#') {
        if (_pinEntryLen == PIN_LEN) {
            uint8_t hash[32];
            _crypto.sha256(_pinBuf, PIN_LEN, hash);
            sendMessage(MSG_PAIR_PIN, hash, sizeof(hash));
            Serial.println("[PAIR] PIN hash sent");
        }
        _pinEntryLen = 0;
    } else if (k && _pinEntryLen < PIN_LEN) {
        _pinBuf[_pinEntryLen++] = (uint8_t)(k - '0');
    }

    if (millis() >= _pairingDeadline) {
        Serial.println("[PAIR] window closed without receiving a key");
        _state = State::IDLE;
    }
}

void OuterController::tickIdle() {
    if (!_nvs.isPaired()) return; // неможливо авторизуватись без спільного ключа

    bool buttonPressed = (digitalRead(_scanButtonPin) == LOW);
    if (buttonPressed || _presence.isPresent()) {
        _faceFailCount = 0;
        _face.startScan();
        _state = State::FACE_SCANNING;
        _led.setScanning();
    }
}

void OuterController::tickFaceScanning() {
    Frm1213Event e = _face.lastEvent();
    if (e == Frm1213Event::MATCHED) {
        startAuthRound((uint8_t)_face.lastFaceId(), nullptr);
        return;
    }
    if (e == Frm1213Event::NO_MATCH || e == Frm1213Event::TIMEOUT) {
        _faceFailCount++;
        if (_faceFailCount >= MAX_FACE_ATTEMPTS) {
            Serial.println("[FACE] max attempts reached, falling back to PIN entry");
            _pinEntryLen = 0;
            _state = State::PIN_ENTRY;
            _led.setBlinkRed();
            return;
        }
        if (_presence.isPresent()) {
            _face.startScan(); // людина досі в межах датчика - повторний скан
        } else {
            _state = State::IDLE; // людина відійшла
        }
    }
}

void OuterController::handleKeypad() {
    char k = _keypad.scan();
    if (!k) return;

    if (k == '#') {
        // Незалежно від стану/блокування PIN - # завжди лише замикає замок,
        // це не спроба авторизації і нічого не розкриває атакеру.
        sendMessage(MSG_LOCK_CLOSE, nullptr, 0);
        Serial.println("[LOCK] close requested from keypad (#)");
        return;
    }

    if (millis() < _pinLockedUntilMs) return; // клавіатура заблокована через невдалі спроби PIN

    if (k == '*') {
        _pinEntryLen = 0;
        _led.setProgress(0, PIN_LEN);
        return;
    }

    if (_pinEntryLen < PIN_LEN) {
        _pinBuf[_pinEntryLen++] = (uint8_t)(k - '0');
        _led.setProgress(_pinEntryLen, PIN_LEN);
    }
    if (_pinEntryLen == PIN_LEN) {
        startAuthRound(PIN_SENTINEL_ID, _pinBuf);
        _pinEntryLen = 0;
    }
}

void OuterController::registerPinFailure() {
    _pinFailCount++;
    if (_pinFailCount < PIN_LOCKOUT_MAX_FAILS) return;

    uint32_t blockMs = PIN_LOCKOUT_BASE_MS << _pinLockoutLevel; // 1, 2, 4, 8... хв
    _pinLockedUntilMs = millis() + blockMs;
    Serial.print("[KEYPAD] locked out for ");
    Serial.print(blockMs / 1000);
    Serial.println("s after 3 failed PIN attempts");

    _pinFailCount = 0;
    if (_pinLockoutLevel < 7) _pinLockoutLevel++; // стеля ~128 хв, щоб зсув не переповнив uint32_t
}

void OuterController::startAuthRound(uint8_t id, const uint8_t* pin) {
    _pendingId = id;
    if (pin) memcpy(_pendingPin, pin, PIN_LEN);
    else memset(_pendingPin, 0, PIN_LEN);

    sendMessage(MSG_REQ, nullptr, 0);
    _deadline = millis() + NONCE_WAIT_TIMEOUT_MS;
    _state = State::WAIT_NONCE;
}

void OuterController::tickWaitNonce() {
    uint8_t type, payload[LINK_MAX_PAYLOAD], len;
    if (recvMessage(type, payload, len, sizeof(payload)) && type == MSG_NONCE && len == NONCE_LEN) {
        memcpy(_nonce, payload, NONCE_LEN);

        _pendingCounter = _nvs.counter() + 1;
        _nvs.setCounter(_pendingCounter); // персистимо одразу - гарантує строге зростання і після збою

        uint8_t macIn[NONCE_LEN + AUTH_PAYLOAD_LEN_PIN - TAG_LEN];
        uint8_t macInLen = buildAuthMacInput(_nonce, _pendingId, _pendingCounter, _pendingPin, macIn);
        uint8_t digest[32];
        _crypto.hmacSha256(_nvs.keyCurrent(), KEY_LEN, macIn, macInLen, digest);

        AuthPayload auth{};
        auth.id = _pendingId;
        auth.counter = _pendingCounter;
        memcpy(auth.tag, digest, TAG_LEN);
        memcpy(auth.pin, _pendingPin, PIN_LEN);

        uint8_t out[AUTH_PAYLOAD_LEN_PIN];
        uint8_t outLen = encodeAuthPayload(auth, out);
        sendMessage(MSG_AUTH, out, outLen);

        _deadline = millis() + UNLOCK_ACK_TIMEOUT_MS;
        _state = State::WAIT_UNLOCK_ACK;
        return;
    }
    if (millis() >= _deadline) {
        Serial.println("[AUTH] no NONCE from INNER (unreachable, rate-limited, or locked out)");
        _led.setBlinkRed();
        _state = State::IDLE;
    }
}

void OuterController::tickWaitUnlockAck() {
    uint8_t type, payload[LINK_MAX_PAYLOAD], len;
    if (recvMessage(type, payload, len, sizeof(payload)) && type == MSG_UNLOCK_OK) {
        Serial.println("[AUTH] unlock granted - rotating key");
        uint8_t newKey[KEY_LEN];
        deriveNextKey(_crypto, _nvs.keyCurrent(), _pendingCounter, newKey);
        _nvs.rotateKey(newKey);

        _faceFailCount = 0;
        _pinFailCount = 0;
        _pinLockoutLevel = 0;
        _led.setRunningGreen();
        _face.triggerReset(); // вимога ТЗ: скидати модуль після кожного успішного unlock
        _state = State::POST_UNLOCK_RESET;
        return;
    }
    if (millis() >= _deadline) {
        Serial.println("[AUTH] denied or no ack from INNER");
        _led.setBlinkRed();
        if (_pendingId == PIN_SENTINEL_ID) registerPinFailure();
        _state = State::IDLE;
    }
}

void OuterController::tickPostUnlockReset() {
    if (_face.lastEvent() == Frm1213Event::RESET_DONE) {
        _state = State::IDLE;
    }
}
