# Wiring

Two **Ozobot DRVKit** boards (`esp32:esp32:ozobot_drvkit`, ESP32-S3), ~5 m
apart, linked by one cable. This is a purpose-built robotics carrier board,
not a bare/generic ESP32-S3 breakout - it has fixed onboard peripherals (dual
motor driver, one button on GPIO0/BOOT, one addressable RGB LED on GPIO42,
labelled I2C/SPI/UART header groups). Pin choices below work around those
fixed functions rather than assuming free choice of any GPIO.

**Both boards require Tools > Pin Numbering: "By GPIO number (legacy)".**
The default "By Arduino pin" mode activates `io_pin_remap.h` macro
redefinitions of `pinMode`/`digitalWrite`/`digitalRead`/`analogRead`/
`analogWrite` in the ESP32 core, which collide with FastLED's own internal
pin-handling code (`fl/system/pin.h`) and fail to compile. Switching to raw
GPIO numbering avoids the redefinition entirely and matches every pin number
in `config.h` being a literal GPIO, not a remapped logical "Dx" pin.

I don't have confirmed specs for this board's own power input/connector (its
public documentation is sparse - it's a very recently added board). Each
DRVKit is assumed to be powered independently (its own supply/battery), since
it's a complete product with onboard motor drivers, not a bare sensor board -
verify this against your actual hardware before wiring, since it changes
whether the inter-board cable needs to carry power at all (see below).

## OUTER board (Ozobot DRVKit)

Onboard, not repurposed: dual motor driver (GPIO18/17, GPIO21/33 - unused on
this board), SPI header (GPIO34/12/11/13 - unused).

| Signal | Pin | Notes |
|---|---|---|
| Scan / pairing button | GPIO0 | Onboard `BUTTON`, shares BOOT. Reusing it as a runtime input after boot is the standard pattern - it only affects boot-mode selection during power-on/reset. Press = scan trigger. Held at boot = pairing window. |
| Status LED | GPIO42 | Onboard `RGB_LED`, single WS2812-compatible pixel (FastLED, 1 LED - not a strip). |
| I2C SDA (PCF8574 keypad) | GPIO47 | Onboard labelled I2C header |
| I2C SCL (PCF8574 keypad) | GPIO48 | PCF8574 address 0x20 |
| FRM1213 UART RX | GPIO1 | Serial1, 115200 8N1, free general-purpose pin |
| FRM1213 UART TX | GPIO2 | Serial1 |
| Inter-board Link RX | GPIO3 | Serial2, 115200 8N1 |
| Inter-board Link TX | GPIO4 | Serial2 |
| LD2420 presence UART RX | GPIO5 | Serial0 |
| LD2420 presence UART TX | GPIO6 | Serial0 |
| Tamper switch (NC, lid) | GPIO7 | INPUT_PULLUP, external switch. Lid closed = contact closed = LOW. Lid opened = HIGH. |

## INNER board (Ozobot DRVKit)

Onboard, deliberately not used: the built-in dual motor driver (GPIO18/17,
GPIO21/33) - the lock actuator uses its own separate external H-bridge
instead (see `inner/LockDriver.h`), so those onboard channels are left
unconfigured/unused. Also unused: the onboard button and RGB LED (INNER has
no user-facing UI in this design).

| Signal | Pin | Notes |
|---|---|---|
| I2C SDA (DS3231 + VL53L1X) | GPIO47 | Onboard labelled I2C header. Same bus, different addresses (0x68 / 0x29) - no conflict |
| I2C SCL (DS3231 + VL53L1X) | GPIO48 | |
| Inter-board Link RX | GPIO1 | Serial2, 115200 8N1, free general-purpose pin |
| Inter-board Link TX | GPIO2 | Serial2 |
| Motor driver IN1 (lock) | GPIO3 | Separate external 2-channel H-bridge, impulse control - NOT the onboard motor driver |
| Motor driver IN2 (lock) | GPIO4 | |
| Maintenance button | GPIO5 | External button, INPUT_PULLUP. Held at boot = pairing window / clears paired flag. Held 3s at runtime = clears tamper LOCKED_OUT. |

**Hardware warning (not solved in software):** the lock motor is an
inductive load sharing the INNER enclosure with the MCU. The external H-bridge
must provide galvanic isolation (opto-isolator on IN1/IN2) and a flyback
diode/snubber across the motor winding - see the comment in
`inner/LockDriver.h`.

## Inter-board cable (~5 m)

If each DRVKit is independently powered (see note above), the cable only
needs to carry the Link signal pair plus a common ground reference - 3 cores,
twisted pair + shield recommended given UART at 115200 baud over 5 m (no
error correction beyond the CRC8 frame check in `UartLink` - if this proves
unreliable in practice, pin the transport to ESP-NOW, or swap in a CAN/TWAI
backend behind the same `Link` interface without touching controller logic):

| Core | Connects | Notes |
|---|---|---|
| 1 | OUTER GPIO4 (TX) -> INNER GPIO1 (RX) | Link data |
| 2 | OUTER GPIO3 (RX) <- INNER GPIO2 (TX) | Link data |
| 3 | GND <-> GND | Common reference - mandatory for single-ended UART |

Shield/drain wire (if using shielded cable): tie to GND at the INNER end only,
leave floating at OUTER, to avoid ground loops.

**Power note:** the FRM1213 draws up to **1 A** on its IR flash, on top of
the DRVKit's own consumption, keypad, and LD2420 radar - make sure OUTER's
own power source can cover that peak in addition to whatever the DRVKit
itself normally draws. If it turns out the two boards do need to share a
single power source after all (i.e. the "independently powered" assumption
above doesn't hold for your actual hardware), that changes this cable to 4
cores with a dedicated feed sized for that combined peak - re-derive before
wiring.

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
flash peak on OUTER.

## Pairing procedure

1. Power off both boards.
2. Power on **both** boards while physically holding their button:
   OUTER's onboard BUTTON (GPIO0) and INNER's external maintenance button
   (GPIO5). Only needs to be held through boot (~50 ms debounce) - release
   once the board is up.
3. Both boards enter a 30 s pairing window, over whichever transport was
   already selected at boot (see above). INNER generates a new random 32-byte
   key plus its own Wi-Fi channel choice, and repeatedly broadcasts
   key + its own STA MAC + that channel. OUTER stores the first valid bundle
   it receives (so it can fall back to ESP-NOW later even if this pairing
   session ran over UART) and immediately replies with its own STA MAC.
   INNER finalizes (sets `paired=true`, resets counter/generation to 0) only
   after receiving that reply - if it never arrives, nothing is persisted on
   either side and the previous key (if any) stays in effect.
4. Optional, same window: on OUTER's keypad, enter a 4-digit PIN and press
   `#`. Its SHA-256 hash is sent to INNER and stored, enabling the PIN
   fallback path (used after 3 failed face-recognition attempts, or usable
   directly - see "Keypad behaviour" below).
5. If the window closes without a completed exchange, repeat from step 1.
6. Re-pairing later (e.g. replacing a board) uses the same procedure - the
   `paired` flag never reopens on its own; only a deliberate boot-time button
   hold on both boards can trigger it again.

**Not covered by this pairing flow:** enrolling faces into the FRM1213
itself. That's a separate, module-specific operation (not implemented here -
only VERIFY/RESET are wired up per the driver requirements) and would need
its own tool/flow before face-based unlock works end to end. PIN-based unlock
works immediately after pairing if a PIN was set in step 4.

## Keypad behaviour

Outside the pairing window, OUTER's keypad is polled continuously regardless
of FSM state (`OuterController::handleKeypad()`), not just after face-scan
failures:

- Entering 4 digits auto-submits an authorization round (`PIN_SENTINEL_ID`)
  the moment the 4th digit is typed - no `#` needed to confirm.
- `*` clears whatever has been typed so far, so a mistyped digit doesn't
  require restarting from the physical device.
- `#` sends an unauthenticated `MSG_LOCK_CLOSE` to INNER, which drives
  `LockDriver::close()` immediately. It works in every state (even mid-lockout)
  since relocking only tightens security and needs no challenge/response.
- Two independent PINs unlock: the static one set during pairing (step 4),
  and a daily backup code derived from INNER's RTC with no persisted state -
  `4` + day-of-month (2 digits, zero-padded) + `9` (e.g. the 9th -> `4099`,
  the 21st -> `4219`). Either one succeeds. This backup code is guessable by
  anyone who knows the scheme and today's date (~31 possibilities) - it's a
  convenience fallback, not a substitute for the static PIN or face auth.
- Escalating lockout: after 3 consecutive failed PIN attempts, OUTER blocks
  further keypad PIN entry (RAM-only, resets on reboot) for 1 minute; if 3
  more fail once it reopens, the block doubles (2 min, then 4, 8...). It
  resets to the base 1-minute tier after any successful unlock. This is
  separate from INNER's own unauthenticated-request rate limiter
  (`RATE_LIMIT_MAX_FAILS`/`RATE_LIMIT_BLOCK_MS` in `inner/config.h`), which
  still applies underneath regardless of source (face or PIN).
