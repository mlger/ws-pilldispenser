/*
 * sim_touch — scripted touch-event queue for the QEMU/host backend (spec A1).
 * Events are injected by the `sim touch x y [down|up]` console command or by
 * a scenario file; pd_hal_touch_read pops them FIFO. Deterministic: no
 * timing, no noise, empty queue == PD_HAL_ERR_TIMEOUT.
 */
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"

#include "hal_touch.h"

#include "sim_private.h"

static const char *TAG = "sim_touch";

#define SIM_TOUCH_QUEUE_LEN 32

static pd_hal_touch_event_t s_queue[SIM_TOUCH_QUEUE_LEN];
static size_t s_head;
static size_t s_count;

void sim_touch_inject(int x, int y, bool pressed) {
    if (s_count >= SIM_TOUCH_QUEUE_LEN) {
        ESP_LOGW(TAG, "SIM_TOUCH: queue full, dropping (%d,%d %s)", x, y, pressed ? "down" : "up");
        return;
    }
    const size_t idx = (s_head + s_count) % SIM_TOUCH_QUEUE_LEN;
    s_queue[idx].x = x;
    s_queue[idx].y = y;
    s_queue[idx].pressed = pressed;
    s_count++;
    ESP_LOGI(TAG, "SIM_TOUCH: inject x=%d y=%d %s (queued=%d)", x, y, pressed ? "down" : "up",
             (int)s_count);
}

pd_hal_status_t pd_hal_touch_init(void) {
    s_head = 0;
    s_count = 0;
    ESP_LOGI(TAG, "SIM_TOUCH: init (scripted injection via `sim touch`)");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_touch_read(pd_hal_touch_event_t *out) {
    if (out == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    if (s_count == 0) {
        return PD_HAL_ERR_TIMEOUT;
    }
    *out = s_queue[s_head];
    s_head = (s_head + 1) % SIM_TOUCH_QUEUE_LEN;
    s_count--;
    return PD_HAL_OK;
}
