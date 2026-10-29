#ifndef PD_SCHED_SVC_H
#define PD_SCHED_SVC_H

#include <stddef.h>
#include <stdint.h>

#include "core_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Scheduler service (Phase 2): a 1 s tick task that reads the RTC, detects
 * due occurrences through core_scheduler and fires them once each, with the
 * per-slot last-fired dose key (persisted via the storage component) as the
 * dedup guard across reboots and DST overlaps (FMEA-04).
 *
 * Catch-up semantics: every tick scans the wall-clock window since the last
 * tick (clamped to PD_SCHED_CATCHUP_MAX_S) so doses missed while the task was
 * delayed still fire late; occurrences older than the clamp are skipped and
 * logged, never re-dispensed after a large clock jump. At start the window
 * opens PD_SCHED_BOOT_GRACE_S in the past so a dose scheduled just before a
 * reboot still fires once.
 */

/* Occurrences older than this (relative to the tick) are not caught up. */
#define PD_SCHED_CATCHUP_MAX_S (3 * 60 * 60)
/* At start, doses scheduled this many seconds before boot still fire. */
#define PD_SCHED_BOOT_GRACE_S   (30 * 60)
#define PD_SCHED_TICK_PERIOD_MS 1000
#define PD_SCHED_TASK_STACK     4096
#define PD_SCHED_DEFAULT_SLOTS  1

typedef void (*pd_sched_on_due_fn_t)(uint8_t slot_id, uint64_t dose_key, void *user);

/* Start the tick task. `on_due` is invoked from the scheduler task context.
 * Loads persisted slots (falls back to one daily 08:00 slot) and applies the
 * persisted TZ is the caller's business (see pd_sched_svc_set_tz). */
int pd_sched_svc_start(pd_sched_on_due_fn_t on_due, void *user);

/* Replace the slot table and persist it. */
int pd_sched_svc_set_slots(const pd_sched_slot_t *slots, size_t count);

/* Copy the active slots into out (capacity cap); returns the count written. */
size_t pd_sched_svc_get_slots(pd_sched_slot_t *out, size_t cap);

/* Apply a POSIX TZ string to the RTC HAL and the scheduler math. */
int pd_sched_svc_set_tz(const char *posix_tz);

/* Run one scheduler tick synchronously (used by the task and by tests). */
void pd_sched_svc_tick(void);

/* Dose key of the wall-clock minute containing `now` for diagnostics. */
uint64_t pd_sched_svc_current_key(time_t now);

#ifdef __cplusplus
}
#endif

#endif /* PD_SCHED_SVC_H */
