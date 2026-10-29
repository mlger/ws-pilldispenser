/*
 * Unity tests for core_fsm. Run on the host (CTest) and on ESP32-S3 under QEMU.
 */
#include "core_fsm.h"
#include "unity.h"

static pd_fsm_ctx_t fresh_ctx(uint8_t slot) {
    pd_fsm_ctx_t ctx = {0};
    ctx.state = PD_FSM_IDLE;
    ctx.slot_id = slot;
    return ctx;
}

TEST_CASE("happy path idle to dispensed", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(2);

    uint32_t a = pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DOSE_DUE, ctx.state);
    TEST_ASSERT_TRUE(a & PD_ACT_SEND_DISPENSE_REQ);
    TEST_ASSERT_EQUAL_UINT8(1, ctx.attempts);

    a = pd_fsm_step(&ctx, PD_EV_START_ACK);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DISPENSING, ctx.state);

    a = pd_fsm_step(&ctx, PD_EV_COMPLETE);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DISPENSED, ctx.state);
    TEST_ASSERT_TRUE(a & PD_ACT_MARK_JOURNAL);
    TEST_ASSERT_TRUE(ctx.dispensed_flag);
    TEST_ASSERT_EQUAL_UINT8(2, ctx.last_dispensed_slot);
}

TEST_CASE("double dose guard ignores retrigger of same slot", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(5);
    (void)pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    (void)pd_fsm_step(&ctx, PD_EV_START_ACK);
    (void)pd_fsm_step(&ctx, PD_EV_COMPLETE);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DISPENSED, ctx.state);

    /* Simulate the post-dose return to IDLE by the application layer, then a
     * duplicate scheduler trigger for the same slot must be ignored. */
    ctx.state = PD_FSM_IDLE;
    const uint32_t a = pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_IDLE, ctx.state);
    TEST_ASSERT_EQUAL_UINT32(PD_ACT_LOG, a);
}

TEST_CASE("new slot after dispense starts a new cycle", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(1);
    (void)pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    (void)pd_fsm_step(&ctx, PD_EV_START_ACK);
    (void)pd_fsm_step(&ctx, PD_EV_COMPLETE);
    TEST_ASSERT_TRUE(ctx.dispensed_flag);

    ctx.state = PD_FSM_IDLE;
    ctx.slot_id = 3;
    const uint32_t a = pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DOSE_DUE, ctx.state);
    TEST_ASSERT_TRUE(a & PD_ACT_SEND_DISPENSE_REQ);
    TEST_ASSERT_FALSE(ctx.dispensed_flag);
}

TEST_CASE("timeout retries up to limit then fails", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(4);
    (void)pd_fsm_step(&ctx, PD_EV_DOSE_DUE); /* attempts = 1 */

    (void)pd_fsm_step(&ctx, PD_EV_TIMEOUT); /* attempts = 2, resend */
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DOSE_DUE, ctx.state);
    TEST_ASSERT_EQUAL_UINT8(2, ctx.attempts);

    (void)pd_fsm_step(&ctx, PD_EV_TIMEOUT); /* attempts = 3, resend */
    TEST_ASSERT_EQUAL_UINT8(3, ctx.attempts);

    const uint32_t a = pd_fsm_step(&ctx, PD_EV_TIMEOUT); /* exhausted */
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_FAILED, ctx.state);
    TEST_ASSERT_TRUE(a & PD_ACT_ALARM);
}

TEST_CASE("jam in dispensing raises fault and resets", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(6);
    (void)pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    (void)pd_fsm_step(&ctx, PD_EV_START_ACK);

    const uint32_t a = pd_fsm_step(&ctx, PD_EV_JAM);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_FAULT, ctx.state);
    TEST_ASSERT_TRUE(a & PD_ACT_ALARM);

    (void)pd_fsm_step(&ctx, PD_EV_RESET);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_IDLE, ctx.state);
}

TEST_CASE("empty in dispensing fails and retry restarts cycle", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(7);
    (void)pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    (void)pd_fsm_step(&ctx, PD_EV_START_ACK);

    const uint32_t a = pd_fsm_step(&ctx, PD_EV_EMPTY);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_FAILED, ctx.state);
    TEST_ASSERT_TRUE(a & PD_ACT_ALARM);

    const uint32_t b = pd_fsm_step(&ctx, PD_EV_RETRY);
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DOSE_DUE, ctx.state);
    TEST_ASSERT_TRUE(b & PD_ACT_SEND_DISPENSE_REQ);
}

TEST_CASE("invalid transitions are logged and ignored", "[core_fsm]") {
    pd_fsm_ctx_t ctx = fresh_ctx(1);

    TEST_ASSERT_EQUAL_UINT32(PD_ACT_LOG, pd_fsm_step(&ctx, PD_EV_COMPLETE));
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_IDLE, ctx.state);

    (void)pd_fsm_step(&ctx, PD_EV_DOSE_DUE);
    TEST_ASSERT_EQUAL_UINT32(PD_ACT_LOG, pd_fsm_step(&ctx, PD_EV_COMPLETE));
    TEST_ASSERT_EQUAL_UINT32(PD_FSM_DOSE_DUE, ctx.state);
}

TEST_CASE("null context is safe", "[core_fsm]") {
    TEST_ASSERT_EQUAL_UINT32(PD_ACT_NONE, pd_fsm_step(NULL, PD_EV_DOSE_DUE));
}

TEST_CASE("state and event names", "[core_fsm]") {
    TEST_ASSERT_EQUAL_STRING("IDLE", pd_fsm_state_name(PD_FSM_IDLE));
    TEST_ASSERT_EQUAL_STRING("DISPENSED", pd_fsm_state_name(PD_FSM_DISPENSED));
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", pd_fsm_event_name(PD_EV_TIMEOUT));
}
