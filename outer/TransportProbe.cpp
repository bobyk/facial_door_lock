#include "TransportProbe.h"
#include "LinkFrame.h"
#include "Protocol.h"

bool probeUartPeer(UartLink& uart) {
    uart.begin();

    uint8_t pingFrame[LINK_FRAME_MAX];
    uint8_t pingLen = encodeLinkFrame(MSG_PING, nullptr, 0, pingFrame);

    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        uart.send(pingFrame, pingLen);
        uint32_t deadline = millis() + 500;

        while (millis() < deadline) {
            uint8_t buf[LINK_FRAME_MAX];
            size_t len = sizeof(buf);
            if (!uart.receive(buf, len)) continue;

            uint8_t type, payload[LINK_MAX_PAYLOAD], payloadLen;
            if (!decodeLinkFrame(buf, len, type, payload, payloadLen, sizeof(payload))) continue;

            if (type == MSG_PONG) return true;
            if (type == MSG_PING) {
                uint8_t pongFrame[LINK_FRAME_MAX];
                uint8_t pongLen = encodeLinkFrame(MSG_PONG, nullptr, 0, pongFrame);
                uart.send(pongFrame, pongLen);
                return true; // отримали валідний кадр від напарника - UART однозначно живий
            }
        }
    }
    return false;
}
