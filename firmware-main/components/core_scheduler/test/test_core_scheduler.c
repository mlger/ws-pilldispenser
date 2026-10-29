/*
 * Unity tests for core_scheduler. Run on the host (CTest) and on ESP32-S3
 * under QEMU. DST edge dates: EU spring 2026-03-29 (02:00 -> 03:00 CET->CEST),
 * US fall 2026-11-01 (02:00 occurs twice, EDT->EST).
 */
#include <stdio.h>
#include <string.h>

#include "core_scheduler.h"
#include "unity.h"

static time_t local_mktime(int year, int mon, int day, int hour, int min) {
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    return mktime(&t);
}

static void assert_next_iso(const pd_sched_slot_t *slot, time_t from, const char *expect) {
    const time_t due = pd_sched_next_due(slot, from);
    char iso[24];
    pd_sched_format_iso(due, iso, sizeof(iso));
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expect, iso, "unexpected next-due wall time");
}

TEST_CASE("daily next due same day", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.id = 1;
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    assert_next_iso(&s, local_mktime(2026, 1, 15, 6, 0), "2026-01-15T08:00:00");
}

TEST_CASE("daily next due rolls to tomorrow", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.id = 1;
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    assert_next_iso(&s, local_mktime(2026, 1, 15, 8, 1), "2026-01-16T08:00:00");
}

TEST_CASE("eu dst spring forward keeps wall clock", "[core_scheduler]") {
    /* 2026-03-29 02:00 CET -> 03:00 CEST. A dose at 08:00 must still fire at
     * 08:00 local (07:00 UTC before the changeover day, 06:00 UTC after). */
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("CET-1CEST,M3.5.0,M10.5.0/3"));

    pd_sched_slot_t s = {0};
    s.id = 2;
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    /* From 2026-03-28 20:00 local the next due is 2026-03-29 08:00 CEST. */
    assert_next_iso(&s, local_mktime(2026, 3, 28, 20, 0), "2026-03-29T08:00:00");
}

TEST_CASE("eu dst fall back keeps wall clock", "[core_scheduler]") {
    /* 2026-10-25 03:00 CEST -> 02:00 CET. */
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("CET-1CEST,M3.5.0,M10.5.0/3"));

    pd_sched_slot_t s = {0};
    s.id = 2;
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    assert_next_iso(&s, local_mktime(2026, 10, 24, 20, 0), "2026-10-25T08:00:00");
}

TEST_CASE("us dst fall back weekly dose", "[core_scheduler]") {
    /* 2026-11-01: 01:00-02:00 EDT/EST overlap; 08:00 local is unambiguous EST. */
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("EST5EDT,M3.2.0,M11.1.0"));

    pd_sched_slot_t s = {0};
    s.id = 3;
    s.enabled = true;
    s.recurrence = PD_SCHED_WEEKLY;
    s.hour = 8;
    s.minute = 0;
    s.days_mask = 0x40; /* Sunday */

    /* From Saturday 2026-10-31 12:00 local the next Sunday dose is 2026-11-01. */
    assert_next_iso(&s, local_mktime(2026, 10, 31, 12, 0), "2026-11-01T08:00:00");
}

TEST_CASE("weekly mask skips non matching days", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.id = 4;
    s.enabled = true;
    s.recurrence = PD_SCHED_WEEKLY;
    s.hour = 9;
    s.minute = 30;
    s.days_mask = 0x40; /* Sunday only */

    /* 2026-01-15 is a Thursday -> next due Sunday 2026-01-18. */
    assert_next_iso(&s, local_mktime(2026, 1, 15, 12, 0), "2026-01-18T09:30:00");
}

TEST_CASE("is due matches minute granularity", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    const time_t due = pd_sched_next_due(&s, 0);
    TEST_ASSERT_TRUE(due > 0);
    TEST_ASSERT_TRUE(pd_sched_is_due(&s, due));
    TEST_ASSERT_FALSE(pd_sched_is_due(&s, due + 60));
    TEST_ASSERT_FALSE(pd_sched_is_due(&s, due - 1));
}

TEST_CASE("disabled slot returns error", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.enabled = false;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    TEST_ASSERT_EQUAL_INT(-1, (int)pd_sched_next_due(&s, 1000));
    TEST_ASSERT_FALSE(pd_sched_is_due(&s, 1000));
}

TEST_CASE("once recurrence fires once then moves on", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.enabled = true;
    s.recurrence = PD_SCHED_ONCE;
    s.hour = 15;
    s.minute = 45;

    assert_next_iso(&s, local_mktime(2026, 6, 1, 1, 0), "2026-06-01T15:45:00");
}

TEST_CASE("invalid tz rejected", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(-1, pd_sched_set_tz(NULL));
    TEST_ASSERT_EQUAL_INT(-1, pd_sched_set_tz(""));
}

/* ------------------------------------------------------------------ */
/* Phase 2: dose keys and catch-up window scan                         */
/* ------------------------------------------------------------------ */

TEST_CASE("dose key packs local wall minute", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    /* 2026-03-29 03:00 UTC -> 202603290300 */
    const time_t t = local_mktime(2026, 3, 29, 3, 0);
    char msg[64];
    (void)snprintf(msg, sizeof(msg), "key=%llu", (unsigned long long)pd_sched_dose_key(t));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(202603290300ull, pd_sched_dose_key(t), msg);

    /* Same wall minute, different second -> identical key. */
    TEST_ASSERT_EQUAL_UINT64(pd_sched_dose_key(t), pd_sched_dose_key(t + 30));

    /* Next minute -> next key. */
    TEST_ASSERT_EQUAL_UINT64(202603290301ull, pd_sched_dose_key(t + 60));
}

TEST_CASE("dose key is equal across fall back overlap", "[core_scheduler]") {
    /* 2026-10-25: 02:30 happens twice (02:30 CEST = 00:30Z, 02:30 CET = 01:30Z).
     * Both must map to the same dose key so the scheduler dedup treats the
     * repeated wall clock as one dose (FMEA-04). glibc mktime resolves the
     * ambiguous 02:30 to the standard (CET, second) pass; the first pass is
     * one real hour earlier. */
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("CET-1CEST,M3.5.0,M10.5.0/3"));

    const time_t second_pass = local_mktime(2026, 10, 25, 2, 30); /* 02:30 CET */
    const time_t first_pass = second_pass - 3600;                 /* 02:30 CEST */
    TEST_ASSERT_EQUAL_UINT64(202610250230ull, pd_sched_dose_key(first_pass));
    TEST_ASSERT_EQUAL_UINT64(pd_sched_dose_key(first_pass), pd_sched_dose_key(second_pass));
}

TEST_CASE("window scan catches missed dose", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    /* Ticks at 07:00, then a jump to 09:30: the 08:00 occurrence is in the
     * (07:00, 09:30] window and must be found. */
    const time_t start = local_mktime(2026, 1, 15, 7, 0);
    const time_t end = local_mktime(2026, 1, 15, 9, 30);
    const time_t occ = pd_sched_first_due_in_window(&s, start, end);
    char iso[24];
    pd_sched_format_iso(occ, iso, sizeof(iso));
    TEST_ASSERT_EQUAL_STRING("2026-01-15T08:00:00", iso);
}

TEST_CASE("window scan excludes start boundary and past end", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    /* end == 08:00 exactly: the 08:00 occurrence is included (end is inclusive). */
    const time_t day = local_mktime(2026, 1, 15, 0, 0);
    TEST_ASSERT_EQUAL_INT((int)day + 8 * 3600,
                          (int)pd_sched_first_due_in_window(&s, day, day + 8 * 3600));

    /* start == 08:00 exactly: strictly after -> next day. */
    const time_t next = pd_sched_first_due_in_window(&s, day + 8 * 3600, day + 40 * 3600);
    char iso[24];
    pd_sched_format_iso(next, iso, sizeof(iso));
    TEST_ASSERT_EQUAL_STRING("2026-01-16T08:00:00", iso);

    /* Nothing due in the window. */
    TEST_ASSERT_EQUAL_INT(-1,
                          (int)pd_sched_first_due_in_window(&s, day + 9 * 3600, day + 10 * 3600));
}

TEST_CASE("window scan clamps large jumps", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t s = {0};
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = 8;
    s.minute = 0;

    /* A months-long jump: only the last PD_SCHED_WINDOW_MAX seconds are
     * scanned, so the 2025-12-01 occurrence must NOT be returned; the clamp
     * starts 48 h before end (= 2026-01-13 07:00) and the first hit inside is
     * that same day's 08:00. */
    const time_t end = local_mktime(2026, 1, 15, 7, 0);
    const time_t occ = pd_sched_first_due_in_window(&s, local_mktime(2025, 12, 1, 8, 0), end);
    TEST_ASSERT_TRUE(occ > end - PD_SCHED_WINDOW_MAX);
    char iso[24];
    pd_sched_format_iso(occ, iso, sizeof(iso));
    TEST_ASSERT_EQUAL_STRING("2026-01-13T08:00:00", iso);
}

TEST_CASE("window scan respects weekly mask and disabled slot", "[core_scheduler]") {
    TEST_ASSERT_EQUAL_INT(0, pd_sched_set_tz("UTC0"));

    pd_sched_slot_t weekly = {0};
    weekly.enabled = true;
    weekly.recurrence = PD_SCHED_WEEKLY;
    weekly.hour = 9;
    weekly.minute = 30;
    weekly.days_mask = 0x40; /* Sunday only */

    /* Window covers Thu 2026-01-15 .. Sun 2026-01-18 10:00 -> finds Sunday. */
    const time_t start = local_mktime(2026, 1, 15, 12, 0);
    const time_t end = local_mktime(2026, 1, 18, 10, 0);
    const time_t occ = pd_sched_first_due_in_window(&weekly, start, end);
    char iso[24];
    pd_sched_format_iso(occ, iso, sizeof(iso));
    TEST_ASSERT_EQUAL_STRING("2026-01-18T09:30:00", iso);

    weekly.enabled = false;
    TEST_ASSERT_EQUAL_INT(-1, (int)pd_sched_first_due_in_window(&weekly, start, end));

    /* Disabled slot and empty window are rejected. */
    pd_sched_slot_t daily = {0};
    daily.enabled = true;
    daily.recurrence = PD_SCHED_DAILY;
    daily.hour = 8;
    TEST_ASSERT_EQUAL_INT(-1, (int)pd_sched_first_due_in_window(NULL, start, end));
    TEST_ASSERT_EQUAL_INT(-1, (int)pd_sched_first_due_in_window(&daily, end, start));
}
