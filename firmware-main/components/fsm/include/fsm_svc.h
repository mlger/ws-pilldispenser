#ifndef PD_FSM_SVC_H
#define PD_FSM_SVC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * FSM service (Phase 2): owns the core_fsm context and drives it from real
 * inputs — scheduler dose-due callbacks, PD-Link frames from the co-processor
 * HAL, and ACK/completion timeouts. Executes the actions core_fsm returns:
 * write-ahead journal (storage), DISPENSE_REQ via core_protocol, inventory
 * take and deterministic log markers the QEMU scenario suite expects.
 *
 * Safety contract (docs/fmea.md, docs/protocol.md):
 *  - journal PENDING is committed BEFORE the dispense request is sent;
 *  - on boot a pending journal entry is replayed, resolved as
 *    dispensed-unknown and its dose key persisted as fired — a power loss
 *    mid-dispense never causes a second dispense (FMEA-04);
 *  - retries reuse the same protocol SEQ within the retry budget; the
 *    co-processor must re-ACK duplicates without repeating the action.
 */

#define PD_FSM_SVC_ACK_TIMEOUT_MS 500
#define PD_FSM_SVC_COMPLETE_TIMEOUT_MS 10000
#define PD_FSM_SVC_STATUS_POLL_MS 250
#define PD_FSM_SVC_TASK_STACK 4096

/* Start the service (idempotent). Replays a pending journal entry if present. */
int pd_fsm_svc_start(void);

/* Seed the in-memory inventory (slot_count slots, each loaded capacity/count).
 * Must be called before start(); defaults are applied by start() otherwise. */
int pd_fsm_svc_inventory_init(uint8_t slot_count);

/* Post a dose-due occurrence (from the scheduler or the console). Applies the
 * service-level double-dose guard and defers while a cycle is running. */
int pd_fsm_svc_dose_due(uint8_t slot_id, uint64_t dose_key);

/* Operator actions (console). */
int pd_fsm_svc_reset(void);
int pd_fsm_svc_retry(void);

/* Test hook: restart the device right after the journal write-ahead of the
 * next dispense (deterministic power-loss-mid-dispense emulation). */
void pd_fsm_svc_arm_power_loss(void);

/* Diagnostics (console `pd status`). */
const char *pd_fsm_svc_state_name(void);
uint8_t pd_fsm_svc_slot(void);
uint64_t pd_fsm_svc_key(void);
bool pd_fsm_svc_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* PD_FSM_SVC_H */
