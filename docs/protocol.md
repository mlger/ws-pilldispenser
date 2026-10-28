# PillDispenser — RS485 Co-Processor Protocol (PD-Link v1)

Status: Phase 0 draft. The codec (`core_protocol`) is implemented in Phase 0.5; full
co-processor integration lands in Phase 3.

## 1. Scope and physical layer

PD-Link is the half-duplex RS485 link between the ESP32-S3 main controller and the
co-processor MCU that drives the dispense mechanism. Both sides compile the same
`core_protocol` codec sources, and `ci-firmware-main` / `ci-coproc` run identical codec
tests on both sides.

| Parameter | Value |
| --- | --- |
| Physical layer | RS485, half-duplex |
| Baud / format | 115200 baud, 8N1 |
| Direction control | Automatic transceiver direction switching (board feature) |
| Termination | Onboard termination resistor switches |
| Max frame size | 135 bytes (5 header + 128 payload + 2 CRC) |

## 2. Frame format

```text
Byte:    0       1       2       3       4       5 .. 5+LEN-1   5+LEN    6+LEN
      +-------+-------+-------+-------+-------+----------------+--------+--------+
      |  SOF  |  VER  |  TYPE |  SEQ  |  LEN  |    PAYLOAD     | CRC_HI | CRC_LO |
      | 0xA5  | 0x01  |       |       | 0..128|   LEN bytes    |        |        |
      +-------+-------+-------+-------+-------+----------------+--------+--------+
      |<-------------- CRC16-CCITT-FALSE over bytes 0 .. 4+LEN --------------->|
```

| Field | Offset | Size | Meaning |
| --- | --- | --- | --- |
| `SOF` | 0 | 1 | Start of frame, fixed `0xA5` |
| `VER` | 1 | 1 | Protocol version, `0x01` |
| `TYPE` | 2 | 1 | Message type (Section 4) |
| `SEQ` | 3 | 1 | uint8 rolling counter, wraps at 255 |
| `LEN` | 4 | 1 | Payload length in bytes, 0..128 |
| `PAYLOAD` | 5 | `LEN` | Type-specific fields; multi-byte fields are big-endian (network order), matching the CRC byte order |
| `CRC` | 5+LEN | 2 | CRC16-CCITT-FALSE over header + payload, transmitted big-endian (high byte first) |

## 3. CRC-16 definition

CRC16-CCITT-FALSE: polynomial `0x1021`, init `0xFFFF`, no input/output reflection, no
final XOR, MSB-first. It covers bytes `0 .. 4+LEN` (header through payload); the CRC
bytes themselves are not covered.

```c
uint16_t pd_crc16(const uint8_t *data, size_t len)   /* core_protocol */
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
```

Worked example — `DISPENSE_REQ` for slot 1, 2 pills, `SEQ = 0x2A`. The CRC state after
each byte of `A5 01 01 2A 02 01 02`:

| Byte fed | `0xA5` | `0x01` | `0x01` | `0x2A` | `0x02` | `0x01` | `0x02` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CRC after | `0x04BF` | `0xEFA5` | `0xB9E0` | `0x53DA` | `0x90D4` | `0x4798` | `0x8061` |

Final CRC = `0x8061`; transmitted big-endian as `80 61`. Complete frame on the wire:

```text
A5 01 01 2A 02 01 02 80 61
```

Self-test vector for the codec suite: `pd_crc16("123456789")` must equal `0x29B1`.

## 4. Message types

| TYPE | Name | Direction | Payload |
| --- | --- | --- | --- |
| `0x01` | `DISPENSE_REQ` | main → coproc | slot, pills |
| `0x02` | `ACK` | coproc → main | seq_acked |
| `0x03` | `NACK` | coproc → main | seq_acked, reason |
| `0x04` | `STATUS_REQ` | main → coproc | (empty) |
| `0x05` | `STATUS_RESP` | coproc → main | state, jam, empty, reserved |
| `0x06` | `JAM_EVENT` | coproc → main (async) | slot |
| `0x07` | `EMPTY_EVENT` | coproc → main (async) | slot |
| `0x08` | `HEARTBEAT` | both | (empty) |
| `0x09` | `MOTOR_CMD` | main → coproc | steps, dir |
| `0x0A` | `MOTOR_STOP` | main → coproc | (empty) |

Payload field tables:

| Message | Field | Type | Meaning |
| --- | --- | --- | --- |
| `DISPENSE_REQ` | `slot` | uint8 | Dispense slot index |
| `DISPENSE_REQ` | `pills` | uint8 | Number of pills to dispense |
| `ACK` | `seq_acked` | uint8 | `SEQ` of the request being acknowledged |
| `NACK` | `seq_acked` | uint8 | `SEQ` of the rejected request |
| `NACK` | `reason` | uint8 | 1 = CRC, 2 = UNKNOWN_TYPE, 3 = BUSY, 4 = INVALID_PARAM |
| `STATUS_RESP` | `state` | uint8 | Co-processor mechanism state |
| `STATUS_RESP` | `jam` | uint8 | 1 = jam sensor active |
| `STATUS_RESP` | `empty` | uint8 | 1 = hopper/slot empty |
| `STATUS_RESP` | `reserved` | uint8 | 0; reserved (error counter exposure is future work) |
| `JAM_EVENT` / `EMPTY_EVENT` | `slot` | uint8 | Slot that jammed / ran empty |

`MOTOR_CMD` (`steps`, uint16 big-endian; `dir`, uint8) / `MOTOR_STOP` are reserved for
Phase 3 diagnostics and the HIL test jig; the normal dispense path uses `DISPENSE_REQ`
only. `JAM_EVENT` / `EMPTY_EVENT` are asynchronous, co-processor-initiated events.

## 5. Timing, retries, and sequencing

| Rule | Value |
| --- | --- |
| ACK deadline | Every request must be ACKed within 500 ms |
| Retries | Up to 3 per request |
| Retry backoff | 250 ms |
| `SEQ` handling | uint8 rolling counter, wraps at 255; sender increments per new request, keeps `SEQ` across retries |
| STATUS poll interval during `DISPENSING` | design parameter, default 250 ms (tunable) |
| Dispense completion timeout | VERIFY: characterize against the real mechanism in Phase 3; configured per mechanism in `coproc_link` |

## 6. Error handling

- **Silent drop.** A receiver that sees a bad `SOF` or a CRC mismatch silently drops the
  frame and increments an error counter. Exposing the counter in the `STATUS_RESP`
  `reserved` byte is marked as future work (Section 9).
- **Recovery of corrupted requests** is by timeout: no ACK within 500 ms → retransmit,
  up to 3 attempts, then the FSM `TIMEOUT` event fires. A late ACK arriving after the
  sender has retransmitted is duplicate-suppression input (below), never a new
  completion.
- **NACK semantics.** `NACK(BUSY)` may be retried within the same retry budget.
  `NACK(INVALID_PARAM)` is never retried — the request is defective and maps to FSM
  `FAILED` with the reason logged. `NACK(UNKNOWN_TYPE)` indicates a firmware-version
  mismatch and raises a firmware alarm. Reason `1` (CRC) is defined for completeness but
  is not emitted in v1 because bad frames are dropped silently; it is reserved for
  diagnostic use.
- **Duplicate suppression (design rule).** A retransmitted request (same `SEQ`, same
  payload, within the retry window) must be re-ACKed without repeating the mechanical
  action. This makes retries idempotent and is required for double-dose safety
  (`docs/fmea.md`, FMEA-01).

## 7. State machine interaction

The FSM (`core_fsm`) consumes codec outcomes as events: `DOSE_DUE`, `START_ACK`,
`COMPLETE`, `JAM`, `EMPTY`, `TIMEOUT`.

```text
   +------------------------------------------------------------------+
   | IDLE                                                             |
   +------------------------------------------------------------------+
     | DOSE_DUE(slot,pills)                 ^ DISPENSED -> IDLE
     | (a DOSE_DUE for an already-          | (next cycle)
     |  dispensed occurrence is IGNORED:    |
     |  double-dose guard)                  |
     v                                      |
   +------------------------------------------------------------------+
   | DOSE_DUE                                                         |
   +------------------------------------------------------------------+
     | ACK(seq) = START_ACK           ^ NACK, or TIMEOUT
     |                                | (no ACK after 3 retries)
     v                                |
   +------------------------------------------------------------------+
   | DISPENSING                                                       |
   +------------------------------------------------------------------+
     | COMPLETE                       ^ JAM / EMPTY / TIMEOUT
     | (STATUS_RESP: state=IDLE,      | (JAM_EVENT or jam=1,
     |  jam=0, empty=0)               |  EMPTY_EVENT or empty=1, or
     v                                |  completion timeout)
   +------------------------------------------------------------------+
   | DISPENSED  (journal marks the occurrence dispensed)              |
   +------------------------------------------------------------------+

   Every failure edge (NACK, TIMEOUT, JAM, EMPTY) lands in FAILED:
   user alarm, occurrence left undispensed in the journal, FSM returns
   to IDLE at the next cycle.
```

| From | Event | To | Notes |
| --- | --- | --- | --- |
| IDLE | `DOSE_DUE` (occurrence not yet dispensed) | DOSE_DUE | Scheduler emits slot + pill count |
| IDLE | `DOSE_DUE` (occurrence already dispensed) | IDLE | **Double-dose guard:** event is ignored and logged |
| DOSE_DUE | `START_ACK` (ACK to `DISPENSE_REQ`) | DISPENSING | Mechanism started |
| DOSE_DUE | `NACK` | FAILED | Reason logged; `BUSY` honored within retry budget |
| DOSE_DUE | `TIMEOUT` | FAILED | No ACK after 3 retries |
| DISPENSING | `COMPLETE` (`state=IDLE`, `jam=0`, `empty=0`) | DISPENSED | Journal marks the occurrence dispensed |
| DISPENSING | `JAM` (`JAM_EVENT` or `jam=1`) | FAILED | FAULT path, user alarm |
| DISPENSING | `EMPTY` (`EMPTY_EVENT` or `empty=1`) | FAILED | Inventory/refill flow triggered |
| DISPENSING | `TIMEOUT` (completion timeout) | FAILED | No completion observed in time |
| DISPENSED / FAILED | next scheduled cycle | IDLE | FSM returns to idle |

The double-dose guard is enforced twice: once in the FSM (a `DOSE_DUE` for an
already-dispensed occurrence is ignored) and once by the inventory journal write-ahead
(`docs/fmea.md` FMEA-01).

## 8. Fault-injection contract (`sim_coproc`)

`sim_coproc` is an in-process co-processor model that speaks real `core_protocol`
frames, available from Phase 0.5 and driven from the QEMU console
(`PD_SIM_CONSOLE=y`). The scenario tests assert the receiver-side behavior below.

| Command | Injected behavior | Expected receiver behavior |
| --- | --- | --- |
| `sim coproc auto-ack on\|off` | Enable/disable automatic ACK of every request | `off` lets scenarios stage explicit ACKs, NACKs or silence |
| `sim coproc fault crc` | Next responses carry a corrupted CRC | Receiver silently drops them; sender retries |
| `sim coproc fault timeout` | ACK delayed beyond the 500 ms deadline | Retry fires; late ACK is absorbed by duplicate suppression |
| `sim coproc fault no-ack` | No response at all | FSM `TIMEOUT` after 3 retries → `FAILED` |
| `sim coproc fault jam` | Injects `JAM_EVENT` mid-dispense / sets `jam=1` in `STATUS_RESP` | FSM `JAM` → `FAILED`, user alarm marker |
| `sim coproc fault empty` | Injects `EMPTY_EVENT` / sets `empty=1` in `STATUS_RESP` | FSM `EMPTY` → `FAILED`, refill flow marker |

## 9. Implementation status by phase

| Capability | Phase | Status at Phase 0 |
| --- | --- | --- |
| Frame format + CRC codec in `core_protocol`, with `0x29B1` check-vector tests on host CTest and QEMU Unity | 0.5 | Designed (this document); codec lands in Phase 0.5 |
| `sim_coproc` model + fault-injection console | 0.5 | Planned |
| FSM event mapping and double-dose guard | 2 | Designed (this document) |
| `DISPENSE_REQ` / `ACK` / `NACK` + retry engine, STATUS polling, `HEARTBEAT` supervision (`coproc_link`) | 3 | Designed (this document) |
| Real co-processor integration + HIL jig (`MOTOR_CMD` diagnostics) | 3 | Designed (this document) |
| Error counter exposed via `STATUS_RESP.reserved` | future | Reserved; not part of v1 behavior |
