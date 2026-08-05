#pragma once
#include <Arduino.h>
#include "PresenceSensor.h"
#include "FaceRecognizer.h"
#include "KeypadPCF8574.h"
#include "LedStrip.h"
#include "ActuatorDriver.h"

class LockController {
public:
    LockController(IPresenceSensor& outerPresence, IPresenceSensor& innerPresence,
                    IFaceRecognizer& face, KeypadPCF8574& keypad,
                    LedStrip& led, ActuatorDriver& actuator,
                    const String& code)
        : _outer(outerPresence), _inner(innerPresence), _face(face),
          _keypad(keypad), _led(led), _actuator(actuator), _code(code) {}

    void update();          // викликати кожен tick з loop()
    void forceLock();       // кнопка решітки

private:
    enum class State { IDLE, SCANNING, AWAIT_CODE, LOCKOUT };

    IPresenceSensor& _outer;
    IPresenceSensor& _inner;
    IFaceRecognizer& _face;
    KeypadPCF8574& _keypad;
    LedStrip& _led;
    ActuatorDriver& _actuator;
    String _code;

    State _state = State::IDLE;
    uint8_t _faceAttempts = 0;
    uint8_t _codeFails = 0;
    uint8_t _lockoutStage = 0;
    uint32_t _lockoutUntil = 0;
    String _entered;

    static constexpr uint8_t MAX_FACE_ATTEMPTS = 3;
    static constexpr uint32_t FACE_TIMEOUT_MS = 1500;
    static constexpr uint8_t MAX_CODE_FAILS = 3;
    static constexpr uint32_t BASE_LOCKOUT_MS = 3UL * 60 * 1000; // 3 хв

    void unlock();
    void enterLockout();
    void handleScanning();
    void handleAwaitCode();
    void handleLockout();
};
