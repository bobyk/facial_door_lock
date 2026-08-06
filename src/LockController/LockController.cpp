#include "LockController.h"

void LockController::update() {
    _led.update();

    switch (_state) {
        case State::IDLE:
            if (_inner.isPresent()) {          // зсередини - відкриваємо без питань
                unlock();
            } else if (_outer.isPresent()) {   // зовні - запускаємо скан обличчя
                _faceAttempts = 0;
                _state = State::SCANNING;
            }
            break;

        case State::SCANNING:
            handleScanning();
            break;

        case State::AWAIT_CODE:
            handleAwaitCode();
            break;

        case State::LOCKOUT:
            handleLockout();
            break;
    }
}

void LockController::handleScanning() {
    FaceResult r = _face.tryRecognize(FACE_TIMEOUT_MS);

    if (r == FaceResult::RECOGNIZED) {
        unlock();
        return;
    }

    _faceAttempts++;
    if (_faceAttempts >= MAX_FACE_ATTEMPTS) {
        _state = State::AWAIT_CODE;
        _entered = "";
        _led.setBlinkRed();
        return;
    }

    if (!_outer.isPresent()) {
        _state = State::IDLE; // людина відійшла - не продовжуємо скан
    }
    // інакше лишаємось в SCANNING -> наступний tick спробує знову
}

void LockController::handleAwaitCode() {
    char k = _keypad.scan();
    if (!k) return;

    if (k == '*') {
        _entered = "";
        _led.setProgress(0, _code.length());
        return;
    }
    if (k == '#') {
        forceLock();
        return;
    }

    _entered += k;
    _led.setProgress(_entered.length(), _code.length());

    if (_entered.length() < _code.length()) return;

    if (_entered == _code) {
        _codeFails = 0;
        _lockoutStage = 0;
        unlock();
    } else {
        _entered = "";
        _codeFails++;
        if (_codeFails >= MAX_CODE_FAILS) {
            enterLockout();
        }
    }
}

void LockController::handleLockout() {
    if (millis() >= _lockoutUntil) {
        _codeFails = 0;
        _entered = "";
        _state = State::AWAIT_CODE;
        _led.setOff();
    }
    // led.update() вже викликано в update(); RUNNING_RED анімується сам
}

void LockController::enterLockout() {
    uint32_t duration = BASE_LOCKOUT_MS << _lockoutStage; // експоненційне зростання: 3, 6, 12, 24хв...
    _lockoutStage++;
    _lockoutUntil = millis() + duration;
    _state = State::LOCKOUT;
    _led.setRunningRed();
}

void LockController::unlock() {
    _led.setRunningGreen();
    _actuator.open();
    _state = State::IDLE;
}

void LockController::forceLock() {
    _actuator.close();
    _led.setOff();
    _state = State::IDLE;
    _entered = "";
}
