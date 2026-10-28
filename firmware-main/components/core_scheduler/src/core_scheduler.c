#include "core_scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool day_matches(const pd_sched_slot_t *slot, const struct tm *lt)
{
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

int pd_sched_set_tz(const char *posix_tz)
{
    if (posix_tz == NULL || posix_tz[0] == '\0') {
        return -1;
    }
    if (setenv("TZ", posix_tz, 1) != 0) {
        return -1;
    }
    tzset();
    return 0;
}

static struct tm local_from_utc(time_t t)
{
    struct tm lt;
    (void)localtime_r(&t, &lt);
    return lt;
}

time_t pd_sched_next_due(const pd_sched_slot_t *slot, time_t from)
{
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

bool pd_sched_is_due(const pd_sched_slot_t *slot, time_t t)
{
    if (slot == NULL || !slot->enabled) {
        return false;
    }
    const struct tm lt = local_from_utc(t);
    if (lt.tm_hour != slot->hour || lt.tm_min != slot->minute) {
        return false;
    }
    return day_matches(slot, &lt);
}

void pd_sched_format_iso(time_t t, char *buf, size_t buf_len)
{
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
