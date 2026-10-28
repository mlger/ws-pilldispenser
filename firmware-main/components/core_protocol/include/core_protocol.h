#ifndef CORE_PROTOCOL_H
#define CORE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PD-Link v1 — RS485 frame codec shared by firmware-main and firmware-coproc.
 *
 * Pure C11: no OS headers, no ESP-IDF headers. The same sources build on the
 * host (CTest) and as an ESP-IDF component. Full wire contract: docs/protocol.md.
 *
 * Frame layout (bytes):
 *   [0] SOF  = 0xA5
 *   [1] VER  = 0x01
 *   [2] TYPE
 *   [3] SEQ  (uint8 rolling counter)
 *   [4] LEN  (payload length, 0..128)
 *   [5..5+LEN) payload
 *   [..+2)   CRC16-CCITT-FALSE over bytes [0..5+LEN), big-endian on the wire
 */

#define PD_PROTO_SOF         0xA5u
#define PD_PROTO_VER         0x01u
#define PD_PROTO_HEADER_LEN  5u
#define PD_PROTO_CRC_LEN     2u
#define PD_PROTO_MAX_PAYLOAD 128u
#define PD_PROTO_MAX_FRAME   (PD_PROTO_HEADER_LEN + PD_PROTO_MAX_PAYLOAD + PD_PROTO_CRC_LEN)

typedef enum {
    PD_FRAME_DISPENSE_REQ = 0x01,
    PD_FRAME_ACK = 0x02,
    PD_FRAME_NACK = 0x03,
    PD_FRAME_STATUS_REQ = 0x04,
    PD_FRAME_STATUS_RESP = 0x05,
    PD_FRAME_JAM_EVENT = 0x06,
    PD_FRAME_EMPTY_EVENT = 0x07,
    PD_FRAME_HEARTBEAT = 0x08,
    PD_FRAME_MOTOR_CMD = 0x09,
    PD_FRAME_MOTOR_STOP = 0x0A,
} pd_frame_type_t;

typedef enum {
    PD_NACK_CRC = 1,
    PD_NACK_UNKNOWN_TYPE = 2,
    PD_NACK_BUSY = 3,
    PD_NACK_INVALID_PARAM = 4,
} pd_nack_reason_t;

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t payload_len;
    uint8_t payload[PD_PROTO_MAX_PAYLOAD];
} pd_frame_t;

/* CRC16-CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR.
 * Check value for ASCII "123456789" is 0x29B1. */
uint16_t pd_crc16(const uint8_t *data, size_t len);

/* Encode a frame into out. Returns the number of bytes written (header +
 * payload + CRC), or 0 when the frame is invalid or out_cap is too small. */
size_t pd_proto_encode(const pd_frame_t *f, uint8_t *out, size_t out_cap);

/* Decode a frame starting at buf.
 * Returns the total frame length consumed (>0) when a complete, CRC-valid
 * frame begins at buf; 0 when more bytes are needed; -1 on unrecoverable
 * framing or CRC error (caller should resynchronize to the next SOF). */
int pd_proto_try_decode(const uint8_t *buf, size_t len, pd_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CORE_PROTOCOL_H */
