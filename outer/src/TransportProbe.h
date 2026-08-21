#pragma once
#include "UartLink.h"

// Пробує UART при transport_mode==AUTO: надсилає PING, чекає PONG до 500мс,
// 3 спроби (загалом до ~1.5с при старті). Симетрично відповідає PONG на
// вхідний PING - обидві плати можуть пробувати одночасно без deadlock.
// true = знайдено напарника на UART (лишатись на ньому всю сесію - без
// подальшого runtime fallback на радіо, інакше це той самий downgrade, від
// якого AUTO нібито захищає лише "на старті").
bool probeUartPeer(UartLink& uart);
