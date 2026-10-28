#include "core_inventory.h"

#include <string.h>

static pd_inv_slot_t *find_slot(pd_inventory_t *inv, uint8_t slot_id) {
    for (uint8_t i = 0; i < inv->slot_count; i++) {
        if (inv->slots[i].slot_id == slot_id) {
            return &inv->slots[i];
        }
    }
    return NULL;
}

static const pd_inv_slot_t *find_slot_const(const pd_inventory_t *inv, uint8_t slot_id) {
    for (uint8_t i = 0; i < inv->slot_count; i++) {
        if (inv->slots[i].slot_id == slot_id) {
            return &inv->slots[i];
        }
    }
    return NULL;
}

bool pd_inv_init(pd_inventory_t *inv, uint8_t slot_count) {
    if (inv == NULL || slot_count == 0 || slot_count > PD_INV_MAX_SLOTS) {
        return false;
    }
    memset(inv, 0, sizeof(*inv));
    inv->slot_count = slot_count;
    for (uint8_t i = 0; i < slot_count; i++) {
        inv->slots[i].slot_id = (uint8_t)(i + 1);
    }
    return true;
}

bool pd_inv_load_slot(pd_inventory_t *inv, uint8_t slot_id, uint16_t capacity, uint16_t count,
                      const char *pill_name) {
    if (inv == NULL || count > capacity) {
        return false;
    }
    pd_inv_slot_t *slot = find_slot(inv, slot_id);
    if (slot == NULL) {
        return false;
    }
    slot->capacity = capacity;
    slot->remaining = count;
    slot->loaded = true;
    slot->pill_name[0] = '\0';
    if (pill_name != NULL) {
        strncpy(slot->pill_name, pill_name, PD_INV_NAME_MAX - 1);
        slot->pill_name[PD_INV_NAME_MAX - 1] = '\0';
    }
    return true;
}

bool pd_inv_take(pd_inventory_t *inv, uint8_t slot_id, uint8_t pills) {
    if (inv == NULL || pills == 0) {
        return false;
    }
    pd_inv_slot_t *slot = find_slot(inv, slot_id);
    if (slot == NULL || !slot->loaded || slot->remaining < pills) {
        return false;
    }
    slot->remaining = (uint16_t)(slot->remaining - pills);
    return true;
}

uint16_t pd_inv_remaining(const pd_inventory_t *inv, uint8_t slot_id) {
    if (inv == NULL) {
        return 0;
    }
    const pd_inv_slot_t *slot = find_slot_const(inv, slot_id);
    return (slot != NULL) ? slot->remaining : 0;
}

uint8_t pd_inv_low_slots(const pd_inventory_t *inv, uint16_t threshold, uint8_t *out,
                         uint8_t out_cap) {
    if (inv == NULL || out == NULL) {
        return 0;
    }
    uint8_t n = 0;
    for (uint8_t i = 0; i < inv->slot_count && n < out_cap; i++) {
        if (inv->slots[i].loaded && inv->slots[i].remaining <= threshold) {
            out[n++] = inv->slots[i].slot_id;
        }
    }
    return n;
}
