#include "core_scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool day_matches(const pd_sched_slot_t *slot, const struct tm *lt) {
    switch (slot->recurrence) {
    case PD_SCHED_ONCE:
    case PD_SCHED_DAILY:
        return true;
    case PD_SCHED_WEEKLY: {
        /* tm_wday: 0=Sunday..6=Saturday. days_mask: bit0=Mon..bit6=Sun. */
        const int bit = (lt->tm_wday == 0) ? 6 : (lt->tm_wday - 1);
        return (int)((slot->days_mask >> bit) & 0x1u) != 0;
    }
    default:
        return false;
    }
}

int pd_sched_set_tz(const char *posix_tz) {
    if (posix_tz == NULL || posix_tz[0] == '\0') {
        return -1;
    }
    if (setenv("TZ", posix_tz, 1) != 0) {
        return -1;
    }
    tzset();
    return 0;
}

static struct tm local_from_utc(time_t t) {
    struct tm lt;
    (void)localtime_r(&t, &lt);
    return lt;
}

time_t pd_sched_next_due(const pd_sched_slot_t *slot, time_t from) {
    if (slot == NULL || !slot->enabled || from < 0) {
        return (time_t)-1;
    }

    /* Candidate days 0..7 always contain the next occurrence of a weekly mask.
     * mktime() with tm_isdst=-1 applies the active TZ including DST shifts, so
     * wall-clock times are reconstructed correctly across spring/fall edges. */
    for (int day = 0; day < 8; day++) {
        struct tm cand = local_from_utc(from);
        cand.tm_mday += day;
        cand.tm_hour = slot->hour;
        cand.tm_min = slot->minute;
        cand.tm_sec = 0;
        cand.tm_isdst = -1;

        const time_t due = mktime(&cand);
        if (due == (time_t)-1 || due <= from) {
            continue;
        }

        /* Re-derive the effective day: a spring-forward jump can move the wall
         * clock past midnight-relative expectations; the weekly mask must be
         * checked against the day the candidate actually lands on. */
        const struct tm eff = local_from_utc(due);
        if (!day_matches(slot, &eff)) {
            continue;
        }
        return due;
    }
    return (time_t)-1;
}

bool pd_sched_is_due(const pd_sched_slot_t *slot, time_t t) {
    if (slot == NULL || !slot->enabled) {
        return false;
    }
    const struct tm lt = local_from_utc(t);
    if (lt.tm_hour != slot->hour || lt.tm_min != slot->minute) {
        return false;
    }
    return day_matches(slot, &lt);
}

void pd_sched_format_iso(time_t t, char *buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return;
    }
    if (buf_len < 20) {
        buf[0] = '\0';
        return;
    }
    const struct tm lt = local_from_utc(t);
    (void)strftime(buf, buf_len, "%Y-%m-%dT%H:%M:%S", &lt);
}

uint64_t pd_sched_dose_key(time_t t) {
    if (t < 0) {
        return 0;
    }
    const struct tm lt = local_from_utc(t);
    const int year = lt.tm_year + 1900;
    if (year < 0) {
        return 0;
    }
    const uint64_t y = (uint64_t)year;
    const uint64_t mo = (uint64_t)(lt.tm_mon + 1);
    const uint64_t d = (uint64_t)lt.tm_mday;
    const uint64_t h = (uint64_t)lt.tm_hour;
    const uint64_t mi = (uint64_t)lt.tm_min;
    return (((((y * 100u + mo) * 100u + d) * 100u + h) * 100u) + mi);
}

time_t pd_sched_first_due_in_window(const pd_sched_slot_t *slot, time_t start_excl,
                                    time_t end_incl) {
    if (slot == NULL || !slot->enabled || end_incl <= start_excl || start_excl < 0) {
        return (time_t)-1;
    }
    /* Clamp: occurrences older than the window maximum are skipped, so a large
     * clock jump cannot fire doses from days ago (late-dose policy is decided
     * by the caller, which logs the skip). */
    if (end_incl - start_excl > PD_SCHED_WINDOW_MAX) {
        start_excl = end_incl - PD_SCHED_WINDOW_MAX;
    }

    /* Walk full local minute boundaries: first one strictly after start_excl. */
    time_t t = start_excl + 1;
    const long rem = (long)(t % 60);
    if (rem != 0) {
        t += 60 - rem;
    }

    for (; t <= end_incl; t += 60) {
        if (pd_sched_is_due(slot, t)) {
            return t;
        }
    }
    return (time_t)-1;
}
