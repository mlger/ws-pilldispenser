/*
 * Deterministic stubs for the QEMU/SIM backend (spec A1) for interfaces whose
 * real emulation lands in later phases. sim_ioexp, sim_touch, sim_rtc and
 * sim_coproc live in their own modules. Everything here logs a deterministic
 * marker so scenario tests can observe HAL activity.
 */
#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"

#include "hal_buzzer.h"
#include "hal_display.h"
#include "hal_net.h"
#include "hal_power.h"
#include "hal_storage_ext.h"

static const char *TAG = "sim_hal";

/* ---- display: esp_lcd_qemu_rgb arrives with Phase 1 spike b ---- */

pd_hal_status_t pd_hal_display_init(void) {
    ESP_LOGI(TAG, "SIM_DISPLAY: stub (esp_lcd_qemu_rgb lands with spike b)");
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_display_backlight(bool on) {
    ESP_LOGI(TAG, "SIM_DISPLAY: backlight=%d", (int)on);
    return PD_HAL_OK;
}

int pd_hal_display_width(void) {
    return PD_HAL_DISPLAY_WIDTH;
}

int pd_hal_display_height(void) {
    return PD_HAL_DISPLAY_HEIGHT;
}

/* ---- net / storage / power / buzzer ---- */

pd_hal_status_t pd_hal_net_connect(void) {
    ESP_LOGI(TAG, "SIM_NET: esp_wifi/BT never linked under QEMU; open_eth Phase 4 (spike c)");
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_net_status(pd_net_status_t *out_status) {
    if (out_status != NULL) {
        *out_status = PD_NET_DISCONNECTED;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_storage_ext_mount(void) {
    ESP_LOGI(TAG, "SIM_STORAGE_EXT: TF card is HIL-only (SD_CS via CH422G EXIO4)");
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_storage_ext_unmount(void) {
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_storage_ext_present(bool *out_present) {
    if (out_present != NULL) {
        *out_present = false;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_power_source(pd_power_source_t *out_source) {
    if (out_source != NULL) {
        *out_source = PD_POWER_SOURCE_DC; /* deterministic for tests */
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_power_battery_mv(uint32_t *out_mv) {
    if (out_mv != NULL) {
        *out_mv = 3700u; /* deterministic for tests */
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_buzzer_init(void) {
    ESP_LOGI(TAG, "SIM_BUZZER: init");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_buzzer_beep(uint8_t times, uint32_t on_ms, uint32_t off_ms) {
    ESP_LOGI(TAG, "SIM_BUZZER: beep times=%d on=%lu off=%lu", (int)times, (unsigned long)on_ms,
             (unsigned long)off_ms);
    return PD_HAL_OK;
}
