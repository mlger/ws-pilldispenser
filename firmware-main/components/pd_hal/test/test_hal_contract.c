/*
 * HAL contract tests (spec A4): every pd_hal interface gets a shared Unity
 * suite so the SIM (QEMU) and REAL (HIL) backends cannot drift.
 *
 * Runs on:
 *   - qemu: firmware-main/test/apps/unit with CONFIG_PD_HAL_SIM=y (CI leg)
 *   - hil:  same app rebuilt with sdkconfig.ci.real (self-hosted runner)
 *
 * The suite asserts only the interface contract, never backend internals:
 * which calls succeed, which error codes come back, and which observable
 * state (e.g. ioexp dump, rtc roundtrip) is consistent. Backend-specific
 * behavior (e.g. sim fault injection) is covered by scenario tests.
 */
#include <string.h>
#include <time.h>

#include "unity.h"

#include "core_protocol.h" /* PD_PROTO_MAX_FRAME for the coproc size contract */

#include "hal_buzzer.h"
#include "hal_coproc.h"
#include "hal_display.h"
#include "hal_ioexp.h"
#include "hal_net.h"
#include "hal_power.h"
#include "hal_rtc.h"
#include "hal_storage_ext.h"
#include "hal_touch.h"

/* ---- display ---- */

TEST_CASE("display dimensions match the 4.3B panel", "[hal_contract]") {
    TEST_ASSERT_EQUAL_INT(800, pd_hal_display_width());
    TEST_ASSERT_EQUAL_INT(480, pd_hal_display_height());
}

TEST_CASE("display backlight toggles without error", "[hal_contract]") {
    /* SIM backend returns NOT_IMPLEMENTED for panel init (spike b pending);
     * backlight itself is contract-OK on both backends. */
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_display_backlight(true));
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_display_backlight(false));
}

/* ---- ioexp ---- */

TEST_CASE("ioexp write/read/dump roundtrip", "[hal_contract]") {
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_init());

    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_write(PD_IOEXP_EXIO2, true));
    bool level = false;
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_read(PD_IOEXP_EXIO2, &level));

    uint8_t regs[8] = {0};
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_dump(regs));
    TEST_ASSERT_EQUAL_HEX8(0x04, regs[0] & 0x04); /* EXIO2 = bit 2 */

    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_write(PD_IOEXP_EXIO2, false));
    regs[0] = 0xFF;
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_dump(regs));
    TEST_ASSERT_EQUAL_HEX8(0x00, regs[0] & 0x04);
}

TEST_CASE("ioexp rejects invalid pins and null out", "[hal_contract]") {
    bool level = false;
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_ioexp_write((pd_ioexp_pin_t)9, true));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_ioexp_read(PD_IOEXP_EXIO2, NULL));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_ioexp_dump(NULL));
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_ioexp_read(PD_IOEXP_EXIO0, &level));
}

/* ---- rtc ---- */

TEST_CASE("rtc set/get roundtrip within one second", "[hal_contract]") {
    const time_t want = (time_t)1769904000; /* 2026-02-01T00:00:00Z, DST-neutral */
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_rtc_set(want));

    time_t got = 0;
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_rtc_get(&got));
    const long long delta = (long long)got - (long long)want;
    TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
}

TEST_CASE("rtc reports valid after a fresh set", "[hal_contract]") {
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_rtc_set((time_t)1769904000));
    bool valid = false;
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_rtc_valid(&valid));
    TEST_ASSERT_TRUE(valid);
}

TEST_CASE("rtc rejects empty TZ and null args", "[hal_contract]") {
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_rtc_set_posix_tz(NULL));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_rtc_set_posix_tz(""));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_rtc_get(NULL));
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_rtc_set_posix_tz("UTC0"));
}

/* ---- touch ---- */

TEST_CASE("touch read with no contact times out", "[hal_contract]") {
    /* Contract: no contact / empty queue == PD_HAL_ERR_TIMEOUT. The SIM
     * backend guarantees an empty queue right after init. */
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_touch_init());
    pd_hal_touch_event_t ev = {0};
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_TIMEOUT, pd_hal_touch_read(&ev));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_touch_read(NULL));
}

/* ---- coproc ---- */

TEST_CASE("coproc send rejects null and oversize", "[hal_contract]") {
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_coproc_init());
    uint8_t big[PD_PROTO_MAX_FRAME + 1] = {0};
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_coproc_send(NULL, 4));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_coproc_send(big, sizeof(big)));
    TEST_ASSERT_EQUAL_INT(PD_HAL_ERR_INVALID_ARG, pd_hal_coproc_recv(NULL, 64, 10, NULL));
}

/* ---- net ---- */

TEST_CASE("net status reports a defined state", "[hal_contract]") {
    pd_net_status_t status = PD_NET_ERROR;
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_net_status(&status));
    TEST_ASSERT_TRUE(status == PD_NET_DISCONNECTED || status == PD_NET_CONNECTED ||
                     status == PD_NET_CONNECTING);
}

/* ---- storage_ext ---- */

TEST_CASE("storage present answers without error", "[hal_contract]") {
    bool present = true;
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_storage_ext_present(&present));
}

/* ---- power ---- */

TEST_CASE("power source returns a defined enum", "[hal_contract]") {
    /* SIM is deterministic (DC); REAL may return NOT_IMPLEMENTED until the
     * sense net is wired (Phase 3) — both are contract-legal when out is
     * untouched or filled with a defined enum value. */
    pd_power_source_t src = PD_POWER_SOURCE_BATTERY;
    const pd_hal_status_t st = pd_hal_power_source(&src);
    TEST_ASSERT_TRUE(st == PD_HAL_OK || st == PD_HAL_ERR_NOT_IMPLEMENTED);
    if (st == PD_HAL_OK) {
        TEST_ASSERT_TRUE(src == PD_POWER_SOURCE_DC || src == PD_POWER_SOURCE_BATTERY ||
                         src == PD_POWER_SOURCE_UNKNOWN);
    }
}

/* ---- buzzer ---- */

TEST_CASE("buzzer init succeeds on both backends", "[hal_contract]") {
    TEST_ASSERT_EQUAL_INT(PD_HAL_OK, pd_hal_buzzer_init());
}
