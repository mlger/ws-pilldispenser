#include "core_fsm.h"

#include <stddef.h>

const char *pd_fsm_state_name(pd_fsm_state_t s)
{
    switch (s) {
    case PD_FSM_IDLE:
        return "IDLE";
    case PD_FSM_DOSE_DUE:
        return "DOSE_DUE";
    case PD_FSM_DISPENSING:
        return "DISPENSING";
    case PD_FSM_DISPENSED:
        return "DISPENSED";
    case PD_FSM_FAILED:
        return "FAILED";
    case PD_FSM_FAULT:
        return "FAULT";
    default:
        return "?";
    }
}

const char *pd_fsm_event_name(pd_fsm_event_t e)
{
    switch (e) {
    case PD_EV_DOSE_DUE:
        return "DOSE_DUE";
    case PD_EV_START_ACK:
        return "START_ACK";
    case PD_EV_COMPLETE:
        return "COMPLETE";
    case PD_EV_JAM:
        return "JAM";
    case PD_EV_EMPTY:
        return "EMPTY";
    case PD_EV_TIMEOUT:
        return "TIMEOUT";
    case PD_EV_RETRY:
        return "RETRY";
    case PD_EV_RESET:
        return "RESET";
    default:
        return "?";
    }
}

static uint32_t begin_dose(pd_fsm_ctx_t *ctx)
{
    ctx->state = PD_FSM_DOSE_DUE;
    ctx->attempts = 1;
    ctx->dispensed_flag = false;
    return PD_ACT_SEND_DISPENSE_REQ | PD_ACT_LOG;
}

static uint32_t retry_or_fail(pd_fsm_ctx_t *ctx)
{
    if (ctx->attempts < PD_FSM_MAX_ATTEMPTS) {
        ctx->attempts++;
        ctx->state = PD_FSM_DOSE_DUE;
        return PD_ACT_SEND_DISPENSE_REQ | PD_ACT_LOG;
    }
    ctx->state = PD_FSM_FAILED;
    return PD_ACT_ALARM | PD_ACT_LOG;
}

uint32_t pd_fsm_step(pd_fsm_ctx_t *ctx, pd_fsm_event_t ev)
{
    if (ctx == NULL) {
        return PD_ACT_NONE;
    }

    switch (ctx->state) {
    case PD_FSM_IDLE:
        if (ev == PD_EV_DOSE_DUE) {
            /* Double-dose guard: ignore a re-trigger for the slot that already
             * completed the current dose cycle. */
            if (ctx->dispensed_flag && ctx->slot_id == ctx->last_dispensed_slot) {
                return PD_ACT_LOG;
            }
            return begin_dose(ctx);
        }
        break;

    case PD_FSM_DOSE_DUE:
        switch (ev) {
        case PD_EV_START_ACK:
            ctx->state = PD_FSM_DISPENSING;
            return PD_ACT_LOG;
        case PD_EV_TIMEOUT:
            return retry_or_fail(ctx);
        case PD_EV_JAM:
            ctx->state = PD_FSM_FAULT;
            return PD_ACT_ALARM | PD_ACT_LOG;
        case PD_EV_EMPTY:
            ctx->state = PD_FSM_FAILED;
            return PD_ACT_ALARM | PD_ACT_LOG;
        default:
            break;
        }
        break;

    case PD_FSM_DISPENSING:
        switch (ev) {
        case PD_EV_COMPLETE:
            ctx->state = PD_FSM_DISPENSED;
            ctx->dispensed_flag = true;
            ctx->last_dispensed_slot = ctx->slot_id;
            return PD_ACT_MARK_JOURNAL | PD_ACT_SIGNAL_USER | PD_ACT_LOG;
        case PD_EV_TIMEOUT:
            return retry_or_fail(ctx);
        case PD_EV_JAM:
            ctx->state = PD_FSM_FAULT;
            return PD_ACT_ALARM | PD_ACT_LOG;
        case PD_EV_EMPTY:
            ctx->state = PD_FSM_FAILED;
            return PD_ACT_ALARM | PD_ACT_LOG;
        default:
            break;
        }
        break;

    case PD_FSM_DISPENSED:
        if (ev == PD_EV_DOSE_DUE) {
            /* New dose cycle: caller has updated ctx->slot_id for the new slot.
             * A guard re-trigger for the same slot is impossible here because
             * IDLE handles it; entering a new cycle clears the flag. */
            return begin_dose(ctx);
        }
        break;

    case PD_FSM_FAILED:
        if (ev == PD_EV_RETRY) {
            ctx->attempts = 0;
            return begin_dose(ctx);
        }
        if (ev == PD_EV_RESET) {
            ctx->state = PD_FSM_IDLE;
            return PD_ACT_LOG;
        }
        break;

    case PD_FSM_FAULT:
        if (ev == PD_EV_RESET) {
            ctx->state = PD_FSM_IDLE;
            return PD_ACT_LOG;
        }
        break;

    default:
        break;
    }

    return PD_ACT_LOG;
}
