#pragma once
#include <Arduino.h>

// HLK-FRM1213 real opcodes, verified against the vendor's "ML FRM/MRM series
// User Agreement Document" (protocol doc V1.1.2, obtained directly from the
// seller) - NOT the FM22x family. Cross-checked byte-for-byte against the
// documented GetParityCheck algorithm and multiple worked request/response
// examples before use; see git history for the derivation.
//
// Frame shape does resemble the FM22x hypothesis (EF AA header, len, XOR
// checksum) but the opcodes differ:
//   - VERIFY   is 0x12, matching the guess.
//   - There is no confirmed 0x10 RESET. The doc's command summary table lists
//     0x10 for "return to standby", but only 0x23 has a worked byte-level
//     example with a checksum that verifies. We use 0x23 (FRM1213_CMD_STANDBY)
//     as the RESET-equivalent since it's the one actually confirmed.
//   - NOTE frames (async, msgType 0x01) exist, but the doc only documents
//     NID_READY being sent on power-on, not explicitly after a reset command.
//   - There's no documented sub-type distinction between "READY" and
//     "FACE_STATE" notes - Frm1213Driver treats a REPLY ack for the standby
//     command as "reset accepted" and does not block indefinitely on a NOTE.

static constexpr uint8_t FRM1213_HDR0 = 0xEF;
static constexpr uint8_t FRM1213_HDR1 = 0xAA;

static constexpr uint8_t FRM1213_CMD_GET_STATUS = 0x11;
static constexpr uint8_t FRM1213_CMD_VERIFY = 0x12;      // face matching
static constexpr uint8_t FRM1213_CMD_STANDBY = 0x23;     // RESET-equivalent, confirmed by example

static constexpr uint8_t FRM1213_MSG_REPLY = 0x00;       // response to a host command
static constexpr uint8_t FRM1213_MSG_NOTE = 0x01;        // unsolicited push (e.g. NID_READY)

static constexpr uint8_t FRM1213_ERR_OK = 0x00;
static constexpr uint8_t FRM1213_ERR_TIMEOUT = 0x0D;      // "entry or unlock timeout"

static constexpr uint8_t FRM1213_MAX_FRAME = 64;          // margin above the largest known reply (36 bytes)
