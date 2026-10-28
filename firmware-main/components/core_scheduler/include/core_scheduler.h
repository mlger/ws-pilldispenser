#ifndef CORE_SCHEDULER_H
#define CORE_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dose scheduling math — pure C11, no OS dependencies.
 *
 * Recurrence and DST handling use POSIX TZ semantics: the process-wide TZ is
 * set with pd_sched_set_tz() (e.g. "CET-1CEST,M3.5.0,M10.5.0/3"), and all
 * wall-clock arithmetic goes through mktime/localtime_r, which is what makes
 * the DST edge cases (spring-forward gaps, fall-back overlaps) behave like a
 * normal clock. See docs/protocol.md (FSM) and docs/fmea.md (FMEA-03/04).
 */

#define PD_SCHED_MAX_SLOTS 32

typedef enum {
    PD_SCHED_ONCE = 0,
    PD_SCHED_DAILY = 1,
    PD_SCHED_WEEKLY = 2,
} pd_sched_recurrence_t;

typedef struct {
    uint8_t id;                  /* stable slot id */
    pd_sched_recurrence_t recurrence;
    uint8_t hour;                /* 0..23, local wall time */
    uint8_t minute;              /* 0..59 */
    uint8_t days_mask;           /* WEEKLY: bit0=Mon .. bit6=Sun; ignored otherwise */
    bool enabled;
} pd_sched_slot_t;

/* Install the POSIX TZ string used by all scheduler math. Returns 0 on success. */
int pd_sched_set_tz(const char *posix_tz);

/* Next due UTC epoch strictly after `from`, honoring recurrence, days_mask and
 * DST transitions of the active TZ. Returns (time_t)-1 on error/disabled slot. */
time_t pd_sched_next_due(const pd_sched_slot_t *slot, time_t from);

/* True when the slot is due at epoch `t` (minute granularity, local wall time). */
bool pd_sched_is_due(const pd_sched_slot_t *slot, time_t t);

/* Render `t` in the active local time as "YYYY-MM-DDTHH:MM:SS" (needs >= 20 bytes). */
void pd_sched_format_iso(time_t t, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* CORE_SCHEDULER_H */
