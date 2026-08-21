#include "EspNowLink.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

EspNowLink* EspNowLink::s_instance = nullptr;

bool EspNowLink::begin() {
    s_instance = this;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // STA увімкнено, але НЕ підключено до жодної точки доступу
    esp_wifi_set_channel(_wifiChannel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) return false;
    esp_now_register_recv_cb(onRecvStatic);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, _broadcastMode ? BROADCAST_MAC : _peerMac, 6);
    peer.channel = _wifiChannel;
    peer.ifidx = WIFI_IF_STA;
    // encrypt=false: HMAC на рівні застосунку обов'язковий і незмінний незалежно
    // від цього; PMK/LMK-шифрування ESP-NOW можна додати пізніше як додатковий шар.
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    return true;
}

bool EspNowLink::send(const uint8_t* data, size_t len) {
    if (len > LINK_FRAME_MAX) return false;
    const uint8_t* dest = _broadcastMode ? BROADCAST_MAC : _peerMac;
    return esp_now_send(dest, data, len) == ESP_OK;
}

bool EspNowLink::receive(uint8_t* buf, size_t& len) {
    bool got = false;
    portENTER_CRITICAL(&_mux);
    if (_rxPending) {
        len = _rxLen;
        memcpy(buf, _rxBuf, _rxLen);
        _rxPending = false;
        got = true;
    }
    portEXIT_CRITICAL(&_mux);
    return got;
}

void EspNowLink::onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!s_instance) return;
    // Поза broadcast-режимом (тобто вже спаровано) - приймаємо ЛИШЕ від сполученого MAC.
    if (!s_instance->_broadcastMode && memcmp(info->src_addr, s_instance->_peerMac, 6) != 0) return;
    if (len <= 0 || (size_t)len > sizeof(s_instance->_rxBuf)) return;

    portENTER_CRITICAL_ISR(&s_instance->_mux);
    memcpy(s_instance->_rxBuf, data, len);
    s_instance->_rxLen = (size_t)len;
    s_instance->_rxPending = true;
    s_instance->_lastRxMillis = millis();
    portEXIT_CRITICAL_ISR(&s_instance->_mux);
}
