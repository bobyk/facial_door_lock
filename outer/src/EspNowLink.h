#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <string.h>
#include "Link.h"
#include "LinkFrame.h"

// ESP-NOW транспорт - резервний/бенчовий, активний лише коли явно обрано
// (transport_mode != UART). Приймає кадри ЛИШЕ від сполученого при парингу
// MAC - будь-який інший відправник відкидається на рівні транспорту. Це єдина
// "безпека", доречна тут (allowlist джерела), не заміна HMAC на рівні застосунку.
//
// Виняток: якщо peerMac ще невідомий (усі нулі - до першого паринга), працюємо
// в broadcast-режимі (стандартний патерн ESP-NOW auto-pairing), інакше MAC
// напарника ще ніде не звідки взяти для самого обміну PAIR_KEY/PAIR_ACK.
// Довіра тут - фізичний доступ (кнопка утримана на старті), не MAC.
class EspNowLink : public Link {
public:
    EspNowLink(const uint8_t peerMac[6], uint8_t wifiChannel, uint32_t aliveTimeoutMs)
        : _wifiChannel(wifiChannel), _aliveTimeoutMs(aliveTimeoutMs) {
        memcpy(_peerMac, peerMac, 6);
        _broadcastMode = true;
        for (uint8_t i = 0; i < 6; ++i) {
            if (_peerMac[i] != 0) { _broadcastMode = false; break; }
        }
    }

    bool begin() override;
    bool send(const uint8_t* data, size_t len) override;
    bool receive(uint8_t* buf, size_t& len) override;
    bool isAlive() override { return millis() - _lastRxMillis < _aliveTimeoutMs; }
    const char* name() override { return "ESP-NOW"; }

private:
    uint8_t _peerMac[6];
    uint8_t _wifiChannel;
    uint32_t _aliveTimeoutMs;
    uint32_t _lastRxMillis = 0;
    bool _broadcastMode; // true = не спаровано ще, приймаємо/шлемо на broadcast MAC

    // Один буферизований кадр між колбеком (контекст Wi-Fi/LWIP таска) і
    // receive() (контекст loop()) - захищено спінлоком, а не звичайним bool.
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t _rxBuf[LINK_FRAME_MAX];
    size_t _rxLen = 0;
    bool _rxPending = false;

    static EspNowLink* s_instance; // esp_now колбеки - C-функції, потребують глобального форварда
    static void onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
};
