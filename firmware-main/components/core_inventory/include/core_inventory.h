#ifndef CORE_INVENTORY_H
#define CORE_INVENTORY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pill inventory bookkeeping — pure C11. Persistence (NVS/LittleFS journal) is
 * layered above this component in Phase 2; here it is an in-memory model.
 */

#define PD_INV_MAX_SLOTS 32
#define PD_INV_NAME_MAX 24

typedef struct {
    uint8_t slot_id;
    uint16_t capacity;
    uint16_t remaining;
    char pill_name[PD_INV_NAME_MAX];
    bool loaded;
} pd_inv_slot_t;

typedef struct {
    uint8_t slot_count;
    pd_inv_slot_t slots[PD_INV_MAX_SLOTS];
} pd_inventory_t;

/* Reset the inventory to `slot_count` empty slots (0 < slot_count <= PD_INV_MAX_SLOTS).
 * Returns false when slot_count is out of range. */
bool pd_inv_init(pd_inventory_t *inv, uint8_t slot_count);

/* Load/count a slot. Returns false on invalid arguments. */
bool pd_inv_load_slot(pd_inventory_t *inv, uint8_t slot_id, uint16_t capacity, uint16_t count,
                      const char *pill_name);

/* Take `pills` from a slot. Returns false when the slot is not loaded or has
 * insufficient pills (no partial takes). */
bool pd_inv_take(pd_inventory_t *inv, uint8_t slot_id, uint8_t pills);

/* Remaining pills for a slot (0 for unknown/empty slots). */
uint16_t pd_inv_remaining(const pd_inventory_t *inv, uint8_t slot_id);

/* Write slot ids with remaining <= threshold into out (at most out_cap).
 * Returns the number written. */
uint8_t pd_inv_low_slots(const pd_inventory_t *inv, uint16_t threshold, uint8_t *out,
                         uint8_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INVENTORY_H */
