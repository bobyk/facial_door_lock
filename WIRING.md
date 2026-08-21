# Wiring

Two ESP32-S3 Super Mini boards, ~5 m apart, linked by one cable. Both boards
require **Tools > USB CDC On Boot: Enabled** so the debug `Serial` console runs
over native USB (GPIO19/20) and all three hardware UART peripherals stay free
for peripherals.

## OUTER board

| Signal | Pin | Notes |
|---|---|---|
| FRM1213 UART RX | GPIO17 | Serial1, 115200 8N1 |
| FRM1213 UART TX | GPIO18 | Serial1 |
| Inter-board Link RX | GPIO15 | Serial2, 115200 8N1 |
| Inter-board Link TX | GPIO16 | Serial2 |
| LD2420 presence UART RX | GPIO44 | Serial0 (default UART0 pins, freed by native-USB console) |
| LD2420 presence UART TX | GPIO43 | Serial0 |
| I2C SDA (PCF8574 keypad) | GPIO8 | |
| I2C SCL (PCF8574 keypad) | GPIO9 | PCF8574 address 0x20 |
| Scan / pairing button | GPIO4 | INPUT_PULLUP, active low. Press = scan trigger. Held at boot = pairing window. |
| Tamper switch (NC, lid) | GPIO5 | INPUT_PULLUP. Lid closed = contact closed = LOW. Lid opened = HIGH. |
| WS2812 LED data | GPIO6 | FastLED, 12 LEDs |

Avoided: GPIO0/45/46 (strapping), GPIO19/20 (native USB D-/D+).

## INNER board

| Signal | Pin | Notes |
|---|---|---|
| Inter-board Link RX | GPIO15 | Serial2, 115200 8N1 |
| Inter-board Link TX | GPIO16 | Serial2 |
| I2C SDA (DS3231 + VL53L1X) | GPIO8 | Same bus, different addresses (0x68 / 0x29) - no conflict |
| I2C SCL (DS3231 + VL53L1X) | GPIO9 | |
| Motor driver IN1 | GPIO5 | 2-channel H-bridge, impulse control |
| Motor driver IN2 | GPIO6 | |
| Maintenance button | GPIO4 | INPUT_PULLUP. Held at boot = pairing window / clears paired flag. Held 3s at runtime = clears tamper LOCKED_OUT. |

Avoided: GPIO0/45/46 (strapping), GPIO19/20 (native USB D-/D+).

**Hardware warning (not solved in software):** the motor is an inductive load
sharing the INNER enclosure with the MCU. The board must provide galvanic
isolation (opto-isolator on IN1/IN2) and a flyback diode/snubber across the
motor winding - see the comment in `inner/LockDriver.h`.

## Inter-board cable (~5 m)

4 cores minimum, twisted pair + shield recommended given UART at 115200 baud
over 5 m (no error correction beyond the CRC8 frame check in `UartLink` - if
this proves unreliable in practice, swap in a CAN/TWAI backend behind the
same `Link` interface without touching any controller logic):

| Core | Connects | Notes |
|---|---|---|
| 1 | OUTER GPIO16 (TX) -> INNER GPIO15 (RX) | Link data |
| 2 | OUTER GPIO15 (RX) <- INNER GPIO16 (TX) | Link data |
| 3 | GND <-> GND | Common reference - mandatory for single-ended UART |
| 4 | V+ -> OUTER local regulator input | Power feed, see below |

Shield/drain wire (if using shielded cable): tie to GND at the INNER end only,
leave floating at OUTER, to avoid ground loops.

**Power budget:** the FRM1213 draws up to **1 A** on its IR flash, on top of
the keypad, LED strip, and LD2420 radar - OUTER's peak draw can exceed 1.2 A.
Over 5 m, a 5 V feed at that current loses a non-trivial fraction of a volt to
wire resistance alone. Feed **12 V** down core 4 and regulate to 5 V/3.3V
locally on the OUTER board (buck converter), rather than sending regulated 5 V
over the cable. Size core 4 (and the GND return) for the peak current -
20 AWG or thicker, or paralleled conductors.

## Transport: UART (default) vs ESP-NOW (fallback)

The inter-board `Link` is transport-agnostic - `UartLink` (the cable, above)
or `EspNowLink` (Wi-Fi radio, no AP, ESP-NOW protocol). Same frame format,
same HMAC, same nonce/counter rules on both; **the transport carries no
security responsibility of its own** beyond an ESP-NOW source-MAC allowlist,
which exists to keep noise out, not as a substitute for the HMAC.

Selection is `transport_mode`, persisted in NVS on each board independently:
- `UART` - cable only, never falls back to radio. **This is the required
  commissioning setting for a deployed lock** - anything else means cutting
  the cable silently downgrades the system to radio, which defeats the
  tamper/physical-access model the whole design rests on.
- `ESPNOW` - radio only.
- `AUTO` (factory default before commissioning) - at boot, sends a PING over
  UART and waits up to 500 ms for a PONG, 3 attempts (~1.5 s worst case). If
  a peer answers, stays on UART for the entire session - no runtime fallback
  to radio after that point, since that would be the exact same downgrade
  just delayed. Otherwise falls back to `EspNowLink`. **Bench/debug only** -
  pin to `UART` before installing.

**Commissioning step:** after verifying the system works, pin the transport
over the USB serial console on each board:

```
transport uart
```

(also accepts `transport espnow` or `transport auto`). Persists to NVS
immediately; takes effect on the next boot - reboot both boards after
sending it.

**Wi-Fi coexistence:** ESP-NOW requires the Wi-Fi radio initialized in STA
mode (no AP connection). This only happens when a board is actually running
`EspNowLink` (AUTO-fallback or pinned `ESPNOW`) - a board pinned to `UART`
never touches Wi-Fi, keeping current draw and attack surface at the UART-only
baseline. If you do run ESP-NOW, budget the extra STA-mode current draw
(radio active even without an AP connection) on top of the FRM1213's 1 A IR
flash peak on OUTER - re-check the 12 V feed sizing above if ESP-NOW is
intended to be more than a bench fallback.

## Pairing procedure

1. Power off both boards.
2. Power on **both** boards while physically holding their button:
   OUTER's scan/pairing button (GPIO4) and INNER's maintenance button (GPIO4).
   Only needs to be held through boot (~50 ms debounce) - release once the
   board is up.
3. Both boards enter a 30 s pairing window, over whichever transport was
   already selected at boot (see above). INNER generates a new random 32-byte
   key plus its own Wi-Fi channel choice, and repeatedly broadcasts
   key + its own STA MAC + that channel. OUTER stores the first valid bundle
   it receives (so it can fall back to ESP-NOW later even if this pairing
   session ran over UART) and immediately replies with its own STA MAC.
   INNER finalizes (sets `paired=true`, resets counter/generation to 0) only
   after receiving that reply - if it never arrives, nothing is persisted on
   either side and the previous key (if any) stays in effect.
4. Optional, same window: on OUTER's keypad, enter a 6-digit PIN and press
   `#`. Its SHA-256 hash is sent to INNER and stored, enabling the PIN
   fallback path (used after 3 failed face-recognition attempts).
5. If the window closes without a completed exchange, repeat from step 1.
6. Re-pairing later (e.g. replacing a board) uses the same procedure - the
   `paired` flag never reopens on its own; only a deliberate boot-time button
   hold on both boards can trigger it again.

**Not covered by this pairing flow:** enrolling faces into the FRM1213
itself. That's a separate, module-specific operation (not implemented here -
only VERIFY/RESET are wired up per the driver requirements) and would need
its own tool/flow before face-based unlock works end to end. PIN-based unlock
works immediately after pairing if a PIN was set in step 4.
