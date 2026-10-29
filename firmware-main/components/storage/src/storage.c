#include "storage.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "pd_store";

/* Journal keys are written in this order: the pending marker goes last, so a
 * torn write can only ever lose the PENDING flag (entry read as "none"), never
 * leave a half-written slot/key pair flagged as pending. */
#define J_KEY_SLOT      "j_slot"
#define J_KEY_DOSE      "j_dose"
#define J_KEY_STATE     "j_state"
#define J_STATE_PENDING 1u

#define K_SLOTS    "slots"
#define K_SLOTS_N  "slots_n"
#define K_LASTKEYS "lastkeys"

static nvs_handle_t s_handle;
static bool s_ready;

/* Strict error mapping for writes: any non-OK result is an error. */
static int nvs_to_pd_strict(esp_err_t err) {
    if (err == ESP_OK) {
        return 0;
    }
    ESP_LOGE(TAG, "nvs error: %s", esp_err_to_name(err));
    return -1;
}

int pd_storage_init(void) {
    if (s_ready) {
        return 0;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs partition needs erase (rc=%s), retrying", esp_err_to_name(err));
        (void)nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_open(PD_STORAGE_NS, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return -1;
    }
    s_ready = true;
    return 0;
}

int pd_storage_journal_begin(uint8_t slot_id, uint64_t dose_key) {
    if (!s_ready) {
        return -1;
    }
    esp_err_t err = nvs_set_u8(s_handle, J_KEY_SLOT, slot_id);
    if (err == ESP_OK) {
        err = nvs_set_u64(s_handle, J_KEY_DOSE, dose_key);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, J_KEY_STATE, J_STATE_PENDING);
    }
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "journal begin failed: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

int pd_storage_journal_commit(void) {
    if (!s_ready) {
        return -1;
    }
    /* Erase the pending marker first: a crash mid-commit leaves the entry
     * readable as pending (replayed again on next boot) rather than cleared
     * while the mechanism may still have dispensed. */
    esp_err_t err = nvs_erase_key(s_handle, J_KEY_STATE);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(s_handle, J_KEY_SLOT);
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(s_handle, J_KEY_DOSE);
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "journal commit failed: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

bool pd_storage_journal_pending(pd_journal_entry_t *out) {
    if (!s_ready || out == NULL) {
        return false;
    }
    uint8_t state = 0;
    if (nvs_get_u8(s_handle, J_KEY_STATE, &state) != ESP_OK || state != J_STATE_PENDING) {
        return false;
    }
    uint8_t slot = 0;
    uint64_t key = 0;
    if (nvs_get_u8(s_handle, J_KEY_SLOT, &slot) != ESP_OK ||
        nvs_get_u64(s_handle, J_KEY_DOSE, &key) != ESP_OK) {
        ESP_LOGW(TAG, "journal pending marker present but entry incomplete");
        return false;
    }
    out->slot_id = slot;
    out->dose_key = key;
    return true;
}

int pd_storage_erase_all(void) {
    if (!s_ready) {
        return -1;
    }
    esp_err_t err = nvs_erase_all(s_handle);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    return nvs_to_pd_strict(err);
}

int pd_storage_slots_save(const pd_sched_slot_t *slots, size_t count) {
    if (!s_ready || slots == NULL || count == 0 || count > PD_STORAGE_MAX_SLOTS) {
        return -1;
    }
    esp_err_t err = nvs_set_blob(s_handle, K_SLOTS, slots, count * sizeof(pd_sched_slot_t));
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, K_SLOTS_N, (uint8_t)count);
    }
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    return nvs_to_pd_strict(err);
}

int pd_storage_slots_load(pd_sched_slot_t *out, size_t *inout_count) {
    if (!s_ready || out == NULL || inout_count == NULL || *inout_count == 0) {
        return -1;
    }
    size_t needed = 0;
    esp_err_t err = nvs_get_blob(s_handle, K_SLOTS, NULL, &needed);
    if (err == ESP_ERR_NVS_NOT_FOUND || needed == 0) {
        *inout_count = 0;
        return 0;
    }
    if (err != ESP_OK || needed > *inout_count * sizeof(pd_sched_slot_t)) {
        return -1;
    }
    err = nvs_get_blob(s_handle, K_SLOTS, out, &needed);
    if (err != ESP_OK) {
        return -1;
    }
    *inout_count = needed / sizeof(pd_sched_slot_t);
    return 0;
}

int pd_storage_last_key_set(uint8_t slot_id, uint64_t key) {
    if (!s_ready || slot_id == 0 || slot_id > PD_STORAGE_MAX_SLOTS) {
        return -1;
    }
    uint64_t keys[PD_STORAGE_MAX_SLOTS] = {0};
    size_t len = sizeof(keys);
    esp_err_t err = nvs_get_blob(s_handle, K_LASTKEYS, keys, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return -1;
    }
    keys[slot_id - 1] = key;
    err = nvs_set_blob(s_handle, K_LASTKEYS, keys, sizeof(keys));
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    return nvs_to_pd_strict(err);
}

int pd_storage_last_key_get(uint8_t slot_id, uint64_t *out_key) {
    if (!s_ready || out_key == NULL || slot_id == 0 || slot_id > PD_STORAGE_MAX_SLOTS) {
        return -1;
    }
    *out_key = 0;
    uint64_t keys[PD_STORAGE_MAX_SLOTS] = {0};
    size_t len = sizeof(keys);
    const esp_err_t err = nvs_get_blob(s_handle, K_LASTKEYS, keys, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 0; /* never fired */
    }
    if (err != ESP_OK) {
        return -1;
    }
    *out_key = keys[slot_id - 1];
    return 0;
}
