#include "OuterController.h"
#include "config.h"
#include <string.h>

OuterController::OuterController(Link& link, Crypto& crypto, NvsStore& nvs, Frm1213Driver& face,
                                  IPresenceSensor& presence, KeypadPCF8574& keypad, LedStrip& led,
                                  uint8_t scanButtonPin, uint8_t tamperPin)
    : _link(link), _crypto(crypto), _nvs(nvs), _face(face), _presence(presence),
      _keypad(keypad), _led(led), _scanButtonPin(scanButtonPin), _tamperPin(tamperPin) {}

void OuterController::begin() {
    pinMode(_scanButtonPin, INPUT_PULLUP);
    pinMode(_tamperPin, INPUT_PULLUP);
    _lastTamperLevel = digitalRead(_tamperPin);

    _link.begin();
    _face.begin();
    _presence.begin();
    _led.begin();

    // Перевірка "кнопка утримана на старті" - одноразово, до входу в loop(),
    // тому короткий busy-wait тут не порушує вимогу "без delay() в головному циклі".
    if (digitalRead(_scanButtonPin) == LOW) {
        uint32_t start = millis();
        while (digitalRead(_scanButtonPin) == LOW && millis() - start < PAIRING_BOOT_HOLD_MS) {}
        if (digitalRead(_scanButtonPin) == LOW && millis() - start >= PAIRING_BOOT_HOLD_MS) {
            Serial.println("[PAIR] boot button held - entering pairing window");
            _state = State::PAIRING;
            _pairingDeadline = millis() + PAIRING_WINDOW_MS;
            return;
        }
    }
    _state = State::IDLE;
}

void OuterController::update() {
    checkTamper();
    sendHeartbeatIfDue();
    _face.update();

    switch (_state) {
        case State::PAIRING: tickPairing(); break;
        case State::IDLE: tickIdle(); break;
        case State::FACE_SCANNING: tickFaceScanning(); break;
        case State::PIN_ENTRY: tickPinEntry(); break;
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
        _link.send(MSG_TAMPER, nullptr, 0);
        lastResendMs = now;
    } else if (level == HIGH && now - lastResendMs >= 2000) {
        // UART без ACK для TAMPER - періодично повторюємо, поки кришка відкрита,
        // на випадок якщо перший кадр загубився.
        _link.send(MSG_TAMPER, nullptr, 0);
        lastResendMs = now;
    }
    _lastTamperLevel = level;
}

void OuterController::sendHeartbeatIfDue() {
    uint32_t now = millis();
    if (now - _lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        _link.send(MSG_HEARTBEAT, nullptr, 0);
        _lastHeartbeatMs = now;
    }
}

void OuterController::tickPairing() {
    uint8_t type, payload[LINK_MAX_PAYLOAD], len;
    if (_link.poll(type, payload, len, sizeof(payload)) && type == MSG_PAIR_KEY && len == KEY_LEN) {
        _nvs.commitPairing(payload);
        for (uint8_t i = 0; i < 3; ++i) _link.send(MSG_PAIR_ACK, nullptr, 0); // невелика надлишковість, UART без ack
        Serial.println("[PAIR] key received and stored");
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
            _link.send(MSG_PAIR_PIN, hash, sizeof(hash));
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

void OuterController::tickPinEntry() {
    char k = _keypad.scan();
    if (!k) return;

    if (k == '*') {
        _pinEntryLen = 0;
        _led.setProgress(0, PIN_LEN);
        return;
    }
    if (k == '#') return; // не цифра в цьому контексті - ігноруємо

    if (_pinEntryLen < PIN_LEN) {
        _pinBuf[_pinEntryLen++] = (uint8_t)(k - '0');
        _led.setProgress(_pinEntryLen, PIN_LEN);
    }
    if (_pinEntryLen == PIN_LEN) {
        startAuthRound(PIN_SENTINEL_ID, _pinBuf);
        _pinEntryLen = 0;
    }
}

void OuterController::startAuthRound(uint8_t id, const uint8_t* pin) {
    _pendingId = id;
    if (pin) memcpy(_pendingPin, pin, PIN_LEN);
    else memset(_pendingPin, 0, PIN_LEN);

    _link.send(MSG_REQ, nullptr, 0);
    _deadline = millis() + NONCE_WAIT_TIMEOUT_MS;
    _state = State::WAIT_NONCE;
}

void OuterController::tickWaitNonce() {
    uint8_t type, payload[LINK_MAX_PAYLOAD], len;
    if (_link.poll(type, payload, len, sizeof(payload)) && type == MSG_NONCE && len == NONCE_LEN) {
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
        _link.send(MSG_AUTH, out, outLen);

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
    if (_link.poll(type, payload, len, sizeof(payload)) && type == MSG_UNLOCK_OK) {
        Serial.println("[AUTH] unlock granted - rotating key");
        uint8_t newKey[KEY_LEN];
        deriveNextKey(_crypto, _nvs.keyCurrent(), _pendingCounter, newKey);
        _nvs.rotateKey(newKey);

        _faceFailCount = 0;
        _led.setRunningGreen();
        _face.triggerReset(); // вимога ТЗ: скидати модуль після кожного успішного unlock
        _state = State::POST_UNLOCK_RESET;
        return;
    }
    if (millis() >= _deadline) {
        Serial.println("[AUTH] denied or no ack from INNER");
        _led.setBlinkRed();
        _state = State::IDLE;
    }
}

void OuterController::tickPostUnlockReset() {
    if (_face.lastEvent() == Frm1213Event::RESET_DONE) {
        _state = State::IDLE;
    }
}
