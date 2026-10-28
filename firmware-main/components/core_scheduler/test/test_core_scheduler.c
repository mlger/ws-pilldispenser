/*
 * Unity tests for core_scheduler. Run on the host (CTest) and on ESP32-S3
 * under QEMU. DST edge dates: EU spring 2026-03-29 (02:00 -> 03:00 CET->CEST),
 * US fall 2026-11-01 (02:00 occurs twice, EDT->EST).
 */
#include <string.h>

#include "core_scheduler.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

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
