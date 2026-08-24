#pragma once
#include <Arduino.h>
#include "Link.h"
#include "Crypto.h"
#include "NvsStore.h"
#include "Frm1213Driver.h"
#include "PresenceSensor.h"
#include "KeypadPCF8574.h"
#include "LedStrip.h"
#include "Protocol.h"

// Головна неблокуюча FSM зовнішнього блока: датчик присутності/кнопка запускають
// скан обличчя (FRM1213); після MAX_FACE_ATTEMPTS невдач - перемикання на ввід PIN
// з клавіатури. Обидва шляхи (face_id або PIN) йдуть через один і той самий
// протокол виклик-відповідь із INNER (REQ/NONCE/AUTH/UNLOCK_OK). Ключ ротується
// лише після підтвердженого UNLOCK_OK - ніколи заздалегідь.
class OuterController {
public:
    OuterController(Link& link, Crypto& crypto, NvsStore& nvs, Frm1213Driver& face,
                     IPresenceSensor& presence, KeypadPCF8574& keypad, LedStrip& led,
                     uint8_t scanButtonPin, uint8_t tamperPin);

    void begin(); // ініціалізує підмодулі
    void update(); // викликати щотік з loop()
    // Викликати з serial-команди "pair" АБО автоматично при 2с утриманні
    // SCAN_BUTTON_PIN під час роботи (checkPairingHold). НЕ через утримання
    // кнопки САМЕ ПРИ СТАРТІ - GPIO0 одночасно є BOOT-strapping піном ESP32:
    // якщо тримати його в LOW у момент reset, чіп йде в download-режим і
    // взагалі не запускає прошивку. Але після старту, поки застосунок вже
    // працює, GPIO0 - звичайний GPIO без жодних обмежень, тож утримання під
    // час роботи (не при reset) безпечне і не конфліктує зі strapping.
    void enterPairingNow();

private:
    enum class State {
        PAIRING,
        IDLE,
        FACE_SCANNING,
        PIN_ENTRY,
        WAIT_NONCE,
        WAIT_UNLOCK_ACK,
        POST_UNLOCK_RESET,
    };

    Link& _link;
    Crypto& _crypto;
    NvsStore& _nvs;
    Frm1213Driver& _face;
    IPresenceSensor& _presence;
    KeypadPCF8574& _keypad;
    LedStrip& _led;
    uint8_t _scanButtonPin, _tamperPin;

    State _state = State::IDLE;
    uint32_t _deadline = 0;
    uint32_t _pairingDeadline = 0;

    uint8_t _faceFailCount = 0;
    uint8_t _pinBuf[PIN_LEN];
    uint8_t _pinEntryLen = 0;

    uint8_t _pinFailCount = 0;      // послідовні невдалі спроби PIN (окремо від _faceFailCount)
    uint8_t _pinLockoutLevel = 0;   // множник ескалації: блок = PIN_LOCKOUT_BASE_MS << level
    uint32_t _pinLockedUntilMs = 0; // 0/минуле = клавіатура не заблокована

    uint8_t _pendingId = 0; // face_id або PIN_SENTINEL_ID для поточного раунду авторизації
    uint8_t _pendingPin[PIN_LEN];
    uint8_t _nonce[NONCE_LEN];
    uint32_t _pendingCounter = 0;

    int _lastTamperLevel = LOW; // LOW = кришка закрита (NC-контакт замкнений на GND)
    uint32_t _lastHeartbeatMs = 0;

    bool _scanButtonHeld = false;
    uint32_t _scanButtonHoldStartMs = 0;

    void tickPairing();
    void tickIdle();
    void tickFaceScanning();
    void tickWaitNonce();
    void tickWaitUnlockAck();
    void tickPostUnlockReset();

    void checkTamper();
    void checkPairingHold(); // 2с утримання SCAN_BUTTON_PIN (поза PAIRING) - без reboot
    void sendHeartbeatIfDue();
    void handleKeypad();      // клавіатура активна в будь-якому стані, крім PAIRING (в якої свій режим введення)
    void registerPinFailure();
    void startAuthRound(uint8_t id, const uint8_t* pin);

    void sendMessage(uint8_t type, const uint8_t* payload, uint8_t len);
    bool recvMessage(uint8_t& type, uint8_t* payload, uint8_t& len, uint8_t maxLen);
};
