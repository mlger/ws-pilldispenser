#ifndef PD_STORAGE_H
#define PD_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Offline persistence (spec Phase 2): NVS-backed dose journal, dose-slot
 * configuration and per-slot "last fired key" table.
 *
 * Journal is write-ahead (FMEA-01/04): pd_storage_journal_begin() commits
 * PENDING *before* the dispense request goes out on the wire, so a power loss
 * mid-dispense leaves a durable trace. On boot the FSM service replays the
 * pending entry, resolves it as dispensed-unknown and never re-dispenses the
 * same dose key (no double dose). The per-slot last-key table is the second
 * dedup guard (the first is core_fsm's dispensed_flag, the third the journal
 * itself).
 */

#define PD_STORAGE_NS        "pdstore"
#define PD_STORAGE_MAX_SLOTS PD_SCHED_MAX_SLOTS

typedef struct {
    uint8_t slot_id;
    uint64_t dose_key;
} pd_journal_entry_t;

/* Initialize NVS (standard no-free-pages recovery) and open the namespace.
 * Idempotent; safe to call again after a restart. */
int pd_storage_init(void);

/* --- Dose journal (single pending occurrence) --- */

/* Durably mark (slot_id, dose_key) as pending. Commits before returning. */
int pd_storage_journal_begin(uint8_t slot_id, uint64_t dose_key);

/* Mark the pending occurrence completed (or resolved as dispensed-unknown by
 * the boot replay). Clears the pending state. */
int pd_storage_journal_commit(void);

/* Read the pending entry, if any. Returns false when no entry is pending. */
bool pd_storage_journal_pending(pd_journal_entry_t *out);

/* Erase every key of the storage namespace (factory reset / test isolation). */
int pd_storage_erase_all(void);

/* --- Dose slot configuration --- */

/* Persist up to count slots. Returns -1 on storage errors or count out of
 * range (0 < count <= PD_STORAGE_MAX_SLOTS). */
int pd_storage_slots_save(const pd_sched_slot_t *slots, size_t count);

/* Load persisted slots into out (capacity *inout_count); sets *inout_count to
 * the number loaded. Returns 0 with *inout_count == 0 when nothing is stored. */
int pd_storage_slots_load(pd_sched_slot_t *out, size_t *inout_count);

/* --- Per-slot last fired dose key (dedup across reboots) --- */

int pd_storage_last_key_set(uint8_t slot_id, uint64_t key);
/* *out_key == 0 when the slot never fired. */
int pd_storage_last_key_get(uint8_t slot_id, uint64_t *out_key);

#ifdef __cplusplus
}
#endif

#endif /* PD_STORAGE_H */
