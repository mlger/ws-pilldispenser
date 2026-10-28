#ifndef CORE_FSM_H
#define CORE_FSM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dispense state machine — pure C11, deterministic, test-first.
 *
 * IDLE --DOSE_DUE--> DOSE_DUE --START_ACK--> DISPENSING --COMPLETE--> DISPENSED
 *                       |  ^--(retry<=3)--|        |
 *                       +--TIMEOUT--> FAILED      +--JAM--> FAULT
 *                                                 +--EMPTY--> FAILED
 * FAILED/FAULT --RESET--> IDLE
 *
 * The double-dose guard (FMEA-01): a DOSE_DUE for the slot that has already
 * completed the current dose cycle is ignored. See docs/fmea.md.
 */

typedef enum {
    PD_FSM_IDLE = 0,
    PD_FSM_DOSE_DUE,
    PD_FSM_DISPENSING,
    PD_FSM_DISPENSED,
    PD_FSM_FAILED,
    PD_FSM_FAULT,
} pd_fsm_state_t;

typedef enum {
    PD_EV_DOSE_DUE = 0,   /* scheduler fired for ctx->slot_id */
    PD_EV_START_ACK,      /* co-processor accepted the dispense request */
    PD_EV_COMPLETE,       /* co-processor reports pills delivered */
    PD_EV_JAM,            /* co-processor jam event */
    PD_EV_EMPTY,          /* hopper/slot empty event */
    PD_EV_TIMEOUT,        /* ACK or completion timeout */
    PD_EV_RETRY,          /* retry requested while in FAILED */
    PD_EV_RESET,          /* operator/system reset of FAILED or FAULT */
} pd_fsm_event_t;

/* Action bitmask returned by pd_fsm_step(); the caller executes them. */
#define PD_ACT_NONE 0u
#define PD_ACT_SEND_DISPENSE_REQ (1u << 0)
#define PD_ACT_SIGNAL_USER (1u << 1)
#define PD_ACT_MARK_JOURNAL (1u << 2)
#define PD_ACT_ALARM (1u << 3)
#define PD_ACT_LOG (1u << 4)

#define PD_FSM_MAX_ATTEMPTS 3u

typedef struct {
    pd_fsm_state_t state;
    uint8_t slot_id;             /* current dose slot (set by caller before DOSE_DUE) */
    uint8_t attempts;            /* dispense attempts for the current dose */
    bool dispensed_flag;         /* current dose cycle completed */
    uint8_t last_dispensed_slot; /* valid when dispensed_flag is true */
} pd_fsm_ctx_t;

const char *pd_fsm_state_name(pd_fsm_state_t s);
const char *pd_fsm_event_name(pd_fsm_event_t e);

/* Advance the FSM by one event. Never blocks; returns the action bitmask and
 * updates ctx in place. Unhandled event/state combinations are logged and
 * ignored. */
uint32_t pd_fsm_step(pd_fsm_ctx_t *ctx, pd_fsm_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* CORE_FSM_H */
