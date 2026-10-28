/*
 * hal_real — board bring-up is Phase 1 (spec Section F). Until the real
 * drivers land, every implementation returns PD_HAL_ERR_NOT_IMPLEMENTED and
 * logs a one-shot deterministic marker so a REAL-leg boot is never silent.
 */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_log.h"

#include "hal_buzzer.h"
#include "hal_coproc.h"
#include "hal_display.h"
#include "hal_ioexp.h"
#include "hal_net.h"
#include "hal_power.h"
#include "hal_rtc.h"
#include "hal_storage_ext.h"
#include "hal_touch.h"

static const char *TAG = "hal_real";

#define STUB()                                                                                     \
    do {                                                                                           \
        static bool logged;                                                                        \
        if (!logged) {                                                                             \
            ESP_LOGW(TAG, "HAL_REAL: stub interfaces (board bring-up lands in Phase 1)");          \
            logged = true;                                                                         \
        }                                                                                          \
    } while (0)

pd_hal_status_t pd_hal_display_init(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_display_backlight(bool on) {
    (void)on;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

int pd_hal_display_width(void) {
    return PD_HAL_DISPLAY_WIDTH;
}

int pd_hal_display_height(void) {
    return PD_HAL_DISPLAY_HEIGHT;
}

pd_hal_status_t pd_hal_touch_init(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_touch_read(pd_hal_touch_event_t *out) {
    (void)out;
    return PD_HAL_ERR_TIMEOUT;
}

pd_hal_status_t pd_hal_ioexp_init(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_ioexp_write(pd_ioexp_pin_t pin, bool level) {
    (void)pin;
    (void)level;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_ioexp_read(pd_ioexp_pin_t pin, bool *out_level) {
    (void)pin;
    (void)out_level;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_ioexp_dump(uint8_t out_regs[8]) {
    (void)out_regs;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_rtc_get(time_t *out_epoch) {
    (void)out_epoch;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_rtc_set(time_t epoch) {
    (void)epoch;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_rtc_set_posix_tz(const char *posix_tz) {
    (void)posix_tz;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_rtc_valid(bool *out_valid) {
    (void)out_valid;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_coproc_init(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_coproc_send(const uint8_t *buf, size_t len) {
    (void)buf;
    (void)len;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_coproc_recv(uint8_t *buf, size_t cap, size_t timeout_ms, size_t *out_len) {
    (void)buf;
    (void)cap;
    (void)timeout_ms;
    (void)out_len;
    return PD_HAL_ERR_TIMEOUT;
}

pd_hal_status_t pd_hal_net_connect(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_net_status(pd_net_status_t *out_status) {
    if (out_status != NULL) {
        *out_status = PD_NET_DISCONNECTED;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_storage_ext_mount(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_storage_ext_unmount(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_storage_ext_present(bool *out_present) {
    if (out_present != NULL) {
        *out_present = false;
    }
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_power_source(pd_power_source_t *out_source) {
    if (out_source != NULL) {
        *out_source = PD_POWER_SOURCE_UNKNOWN;
    }
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_power_battery_mv(uint32_t *out_mv) {
    (void)out_mv;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_buzzer_init(void) {
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_buzzer_beep(uint8_t times, uint32_t on_ms, uint32_t off_ms) {
    (void)times;
    (void)on_ms;
    (void)off_ms;
    STUB();
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}
