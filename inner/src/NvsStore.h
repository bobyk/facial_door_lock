#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Protocol.h"

// Постійне сховище ключа/лічильників у NVS (через Preferences - обгортка над
// nvs_flash, вже частина ESP32 Arduino core, нова залежність не потрібна).
// Ключ НІКОЛИ не передається по Link після паринга - обидві плати виводять
// його незалежно через ratchet(); тут лише зберігання локальної копії.
class NvsStore {
public:
    bool begin(const char* namespaceName);

    bool isPaired() const { return _paired; }
    const uint8_t* keyCurrent() const { return _kcur; }
    const uint8_t* keyPrevious() const { return _kprev; }
    bool hasPreviousKey() const { return _gen > 0; } // kprev валідний лише після першої ротації
    uint32_t generation() const { return _gen; }
    uint32_t counter() const { return _counter; }
    bool lockedOut() const { return _lockedOut; }
    const uint8_t* pinHash() const { return _pinHash; } // валідний лише якщо hasPinHash()
    bool hasPinHash() const { return _hasPinHash; }

    // Викликається після успішного паринга: записує все одразу (paired - останнім,
    // як фінальний маркер завершення - переривання живлення до цього моменту
    // лишає paired=false, тож наступна спроба паринга почнеться "з чистого").
    void commitPairing(const uint8_t key[KEY_LEN]);

    // Атомарний (наскільки дозволяє NVS) ratchet: kprev=kcur, kcur=newKey, gen++.
    // Порядок запису kprev -> kcur -> gen обраний так, щоб переривання живлення
    // в будь-якій точці лишало пристрій або в старому, або в новому валідному стані
    // (ніколи - в стані з "дірою": kcur завжди читовний як дійсний ключ якогось покоління).
    void rotateKey(const uint8_t newKey[KEY_LEN]);

    // Персиститься лише при успішній авторизації (вимога ТЗ - "persist on success").
    void setCounter(uint32_t value);

    void setLockedOut(bool locked);
    // Викликається лише на INNER, у вікні паринга (SHA-256(PIN) від OUTER).
    void setPinHash(const uint8_t hash[32]);

private:
    Preferences _prefs;
    uint8_t _kcur[KEY_LEN] = {0};
    uint8_t _kprev[KEY_LEN] = {0};
    uint32_t _gen = 0;
    uint32_t _counter = 0;
    bool _paired = false;
    bool _lockedOut = false;
    uint8_t _pinHash[32] = {0};
    bool _hasPinHash = false;

    void load();
};
