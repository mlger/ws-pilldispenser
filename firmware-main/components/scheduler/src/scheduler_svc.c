#include "scheduler_svc.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "hal_rtc.h"
#include "storage.h"

static const char *TAG = "pd_sched";

void pd_sched_task(void *arg);

static struct {
    pd_sched_on_due_fn_t on_due;
    void *user;
    pd_sched_slot_t slots[PD_SCHED_MAX_SLOTS];
    size_t slot_count;
    time_t last_tick;
    SemaphoreHandle_t lock;
    bool started;
} s_svc;

static bool rtc_now(time_t *out) {
    return pd_hal_rtc_get(out) == PD_HAL_OK && *out >= 0;
}

static void fire(uint8_t slot_id, uint64_t key) {
    if (pd_storage_last_key_set(slot_id, key) != 0) {
        ESP_LOGE(TAG, "SCHED: persist last-key failed slot=%u", (unsigned)slot_id);
        /* Still fire: the FSM journal guard prevents double dispense even if
         * the dedup key was not persisted (fail-safe direction is to fire, the
         * journal replay handles the crash case). */
    }
    ESP_LOGI(TAG, "SCHED: due slot=%u key=%llu", (unsigned)slot_id, (unsigned long long)key);
    s_svc.on_due(slot_id, key, s_svc.user);
}

static void load_slots(void) {
    size_t n = PD_SCHED_MAX_SLOTS;
    if (pd_storage_slots_load(s_svc.slots, &n) == 0 && n > 0) {
        s_svc.slot_count = n;
        ESP_LOGI(TAG, "SCHED: loaded %u persisted slots", (unsigned)n);
        return;
    }
    /* Factory default: one daily 08:00 local slot, DISABLED. The first-boot
     * user configures real dose times; keeping it disabled also makes QEMU
     * scenario tests deterministic (they always set their own slots). */
    s_svc.slot_count = PD_SCHED_DEFAULT_SLOTS;
    memset(s_svc.slots, 0, sizeof(s_svc.slots));
    s_svc.slots[0].id = 1;
    s_svc.slots[0].enabled = false;
    s_svc.slots[0].recurrence = PD_SCHED_DAILY;
    s_svc.slots[0].hour = 8;
    s_svc.slots[0].minute = 0;
    ESP_LOGI(TAG, "SCHED: factory default slots (1: daily 08:00, disabled)");
}

int pd_sched_svc_start(pd_sched_on_due_fn_t on_due, void *user) {
    if (on_due == NULL) {
        return -1;
    }
    if (s_svc.started) {
        return 0;
    }
    s_svc.on_due = on_due;
    s_svc.user = user;
    s_svc.lock = xSemaphoreCreateMutex();
    if (s_svc.lock == NULL) {
        return -1;
    }

    load_slots();

    /* Boot grace: catch doses scheduled shortly before the (re)boot. */
    time_t now = 0;
    if (!rtc_now(&now)) {
        ESP_LOGW(TAG, "SCHED: rtc unavailable at start; ticks will retry");
        s_svc.last_tick = 0;
    } else {
        s_svc.last_tick = now - PD_SCHED_BOOT_GRACE_S;
    }

    if (xTaskCreate(pd_sched_task, "pd_sched", PD_SCHED_TASK_STACK, NULL, 4, NULL) != pdPASS) {
        return -1;
    }
    s_svc.started = true;
    return 0;
}

void pd_sched_task(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(PD_SCHED_TICK_PERIOD_MS));
        pd_sched_svc_tick();
    }
}

void pd_sched_svc_tick(void) {
    if (!s_svc.started || s_svc.on_due == NULL) {
        return;
    }
    if (xSemaphoreTake(s_svc.lock, pdMS_TO_TICKS(500)) != pdTRUE) {
        return;
    }

    time_t now = 0;
    if (!rtc_now(&now)) {
        ESP_LOGW(TAG, "SCHED: tick without valid rtc time");
        xSemaphoreGive(s_svc.lock);
        return;
    }
    if (now < s_svc.last_tick) {
        /* Clock moved backward (operator set time): do not re-fire anything. */
        ESP_LOGI(TAG, "SCHED: clock moved backward, resetting tick window");
        s_svc.last_tick = now;
        xSemaphoreGive(s_svc.lock);
        return;
    }

    /* Catch-up clamp: never scan further back than the catch-up maximum, so a
     * large jump (month-long, DST test sets) fires at most recent doses. */
    time_t scan_start = s_svc.last_tick;
    if (now - scan_start > PD_SCHED_CATCHUP_MAX_S) {
        scan_start = now - PD_SCHED_CATCHUP_MAX_S;
    }

    for (size_t i = 0; i < s_svc.slot_count; i++) {
        const pd_sched_slot_t *slot = &s_svc.slots[i];
        if (!slot->enabled) {
            continue;
        }
        const time_t occ = pd_sched_first_due_in_window(slot, scan_start, now);
        if (occ < 0) {
            continue;
        }
        const uint64_t key = pd_sched_dose_key(occ);
        uint64_t last_key = 0;
        if (pd_storage_last_key_get(slot->id, &last_key) == 0 && key <= last_key) {
            ESP_LOGI(TAG, "SCHED: dedup slot=%u key=%llu (already fired)", (unsigned)slot->id,
                     (unsigned long long)key);
            continue;
        }
        fire(slot->id, key);
    }

    s_svc.last_tick = now;
    xSemaphoreGive(s_svc.lock);
}

int pd_sched_svc_set_slots(const pd_sched_slot_t *slots, size_t count) {
    if (slots == NULL || count == 0 || count > PD_SCHED_MAX_SLOTS) {
        return -1;
    }
    if (xSemaphoreTake(s_svc.lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return -1;
    }
    memcpy(s_svc.slots, slots, count * sizeof(pd_sched_slot_t));
    s_svc.slot_count = count;
    const int rc = pd_storage_slots_save(s_svc.slots, count);
    xSemaphoreGive(s_svc.lock);
    if (rc != 0) {
        ESP_LOGE(TAG, "SCHED: slot persist failed rc=%d", rc);
    }
    return rc;
}

size_t pd_sched_svc_get_slots(pd_sched_slot_t *out, size_t cap) {
    if (out == NULL || cap == 0) {
        return 0;
    }
    if (xSemaphoreTake(s_svc.lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return 0;
    }
    const size_t n = (s_svc.slot_count < cap) ? s_svc.slot_count : cap;
    memcpy(out, s_svc.slots, n * sizeof(pd_sched_slot_t));
    xSemaphoreGive(s_svc.lock);
    return n;
}

int pd_sched_svc_set_tz(const char *posix_tz) {
    if (posix_tz == NULL || posix_tz[0] == '\0') {
        return -1;
    }
    const pd_hal_status_t st = pd_hal_rtc_set_posix_tz(posix_tz);
    if (st != PD_HAL_OK) {
        return -1;
    }
    /* core_scheduler math uses the process TZ (localtime_r/mktime). */
    return pd_sched_set_tz(posix_tz);
}

uint64_t pd_sched_svc_current_key(time_t now) {
    return pd_sched_dose_key(now);
}
