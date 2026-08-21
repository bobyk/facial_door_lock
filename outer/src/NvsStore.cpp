#include "NvsStore.h"
#include <string.h>

bool NvsStore::begin(const char* namespaceName) {
    if (!_prefs.begin(namespaceName, false)) return false;
    load();
    return true;
}

void NvsStore::load() {
    _paired = _prefs.getBool("paired", false);
    _gen = _prefs.getUInt("gen", 0);
    _counter = _prefs.getUInt("ctr", 0);
    _lockedOut = _prefs.getBool("lockout", false);

    size_t kcurLen = _prefs.getBytes("kcur", _kcur, KEY_LEN);
    if (kcurLen != KEY_LEN) memset(_kcur, 0, KEY_LEN);
    size_t kprevLen = _prefs.getBytes("kprev", _kprev, KEY_LEN);
    if (kprevLen != KEY_LEN) memset(_kprev, 0, KEY_LEN);

    _hasPinHash = _prefs.isKey("pinhash");
    if (_hasPinHash) {
        size_t n = _prefs.getBytes("pinhash", _pinHash, sizeof(_pinHash));
        if (n != sizeof(_pinHash)) _hasPinHash = false;
    }
}

void NvsStore::commitPairing(const uint8_t key[KEY_LEN]) {
    // paired записується останнім - незавершене живлення до цього моменту лишає
    // paired=false, тож наступна спроба паринга почнеться з чистого стану, а не "напівпарного".
    memcpy(_kcur, key, KEY_LEN);
    memset(_kprev, 0, KEY_LEN);
    _gen = 0;
    _counter = 0;

    _prefs.putBytes("kcur", _kcur, KEY_LEN);
    _prefs.putBytes("kprev", _kprev, KEY_LEN);
    _prefs.putUInt("gen", _gen);
    _prefs.putUInt("ctr", _counter);
    _prefs.putBool("lockout", false);
    _lockedOut = false;
    _prefs.putBool("paired", true);
    _paired = true;
}

void NvsStore::rotateKey(const uint8_t newKey[KEY_LEN]) {
    // Порядок kprev -> kcur -> gen: переривання живлення в будь-якій точці
    // лишає kcur читовним як дійсний ключ одного з двох послідовних поколінь.
    memcpy(_kprev, _kcur, KEY_LEN);
    _prefs.putBytes("kprev", _kprev, KEY_LEN);

    memcpy(_kcur, newKey, KEY_LEN);
    _prefs.putBytes("kcur", _kcur, KEY_LEN);

    _gen += 1;
    _prefs.putUInt("gen", _gen);
}

void NvsStore::setCounter(uint32_t value) {
    _counter = value;
    _prefs.putUInt("ctr", _counter);
}

void NvsStore::setLockedOut(bool locked) {
    _lockedOut = locked;
    _prefs.putBool("lockout", locked);
}

void NvsStore::setPinHash(const uint8_t hash[32]) {
    memcpy(_pinHash, hash, sizeof(_pinHash));
    _hasPinHash = true;
    _prefs.putBytes("pinhash", _pinHash, sizeof(_pinHash));
}
