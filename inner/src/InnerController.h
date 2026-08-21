#pragma once
#include <Arduino.h>
#include "Link.h"
#include "Crypto.h"
#include "NvsStore.h"
#include "LockDriver.h"
#include "ToFPresenceSensor.h"
#include "RTCModule.h"
#include "Protocol.h"

// Головна неблокуюча FSM внутрішнього блока. Єдиний авторитет, що керує
// замком: перевіряє REQ/NONCE/AUTH від OUTER (обличчя або PIN - однаково через
// один і той самий протокол), і незалежно відмикає без жодної авторизації при
// спрацюванні внутрішнього TOF400C (egress-безпека).
class InnerController {
public:
    InnerController(Link& link, Crypto& crypto, NvsStore& nvs, LockDriver& lock,
                     ToFPresenceSensor& tof, RTCModule& rtc, uint8_t maintButtonPin);

    void begin();
    void update(); // викликати щотік з loop()

private:
    enum class State { PAIRING, IDLE, AWAITING_AUTH, LOCKED_OUT };

    Link& _link;
    Crypto& _crypto;
    NvsStore& _nvs;
    LockDriver& _lock;
    ToFPresenceSensor& _tof;
    RTCModule& _rtc;
    uint8_t _maintButtonPin;

    State _state = State::IDLE;
    uint32_t _bootMs = 0;

    uint8_t _nonce[NONCE_LEN];
    uint32_t _nonceIssuedMs = 0;

    uint8_t _consecutiveFails = 0;
    uint32_t _rateLimitBlockedUntilMs = 0; // RAM-only, per spec

    bool _tofWasPresent = false;
    bool _heartbeatMissingLogged = false;

    bool _maintHeld = false;
    uint32_t _maintHoldStartMs = 0;

    uint8_t _pairingKey[KEY_LEN];
    uint32_t _pairingDeadline = 0;

    void handleReq();
    void handleAuth(const uint8_t* payload, uint8_t len);
    void handleTamper();
    void registerFailure(const char* reason);
    void tickPairing(bool got, uint8_t type, const uint8_t* payload, uint8_t len);
    void checkTof();
    void checkMaintButton();
    void checkHeartbeat();
    void logEvent(const char* msg);
};
