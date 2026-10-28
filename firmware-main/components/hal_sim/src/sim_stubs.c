/*
 * Deterministic stubs for the QEMU/SIM backend (spec A1). Display/touch gain
 * real emulation in Phase 1 (spike b: esp_lcd_qemu_rgb); everything here logs
 * a deterministic marker so scenario tests can observe HAL activity.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "hal_display.h"
#include "hal_touch.h"
#include "hal_ioexp.h"
#include "hal_coproc.h"
#include "hal_net.h"
#include "hal_storage_ext.h"
#include "hal_power.h"
#include "hal_buzzer.h"

static const char *TAG = "sim_hal";

/* ---- sim_ioexp: in-RAM CH422G register model, every write is logged ---- */

static uint8_t s_ioexp_regs[8];

pd_hal_status_t pd_hal_ioexp_init(void)
{
    memset(s_ioexp_regs, 0, sizeof(s_ioexp_regs));
    ESP_LOGI(TAG, "SIM_IOEXP: reset");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_write(pd_ioexp_pin_t pin, bool level)
{
    if (pin > PD_IOEXP_EXIO5) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    const uint8_t bit = (uint8_t)(1u << (int)pin);
    if (level) {
        s_ioexp_regs[0] = (uint8_t)(s_ioexp_regs[0] | bit);
    } else {
        s_ioexp_regs[0] = (uint8_t)(s_ioexp_regs[0] & (uint8_t)~bit);
    }
    ESP_LOGI(TAG, "SIM_IOEXP: write exio=%d level=%d regs=%02x", (int)pin, (int)level,
             s_ioexp_regs[0]);
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_read(pd_ioexp_pin_t pin, bool *out_level)
{
    if (out_level == NULL || pin > PD_IOEXP_EXIO5) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    *out_level = (s_ioexp_regs[0] >> (int)pin) & 0x1u;
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_dump(uint8_t out_regs[8])
{
    if (out_regs == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    memcpy(out_regs, s_ioexp_regs, sizeof(s_ioexp_regs));
    ESP_LOGI(TAG, "SIM_IOEXP: dump=%02x%02x%02x%02x%02x%02x%02x%02x", s_ioexp_regs[0],
             s_ioexp_regs[1], s_ioexp_regs[2], s_ioexp_regs[3], s_ioexp_regs[4], s_ioexp_regs[5],
             s_ioexp_regs[6], s_ioexp_regs[7]);
    return PD_HAL_OK;
}

/* ---- display / touch: real emulation lands in Phase 1 (spike b) ---- */

pd_hal_status_t pd_hal_display_init(void)
{
    ESP_LOGI(TAG, "SIM_DISPLAY: stub (esp_lcd_qemu_rgb arrives with Phase 1, spike b)");
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_display_backlight(bool on)
{
    ESP_LOGI(TAG, "SIM_DISPLAY: backlight=%d", (int)on);
    return PD_HAL_OK;
}

int pd_hal_display_width(void)
{
    return PD_HAL_DISPLAY_WIDTH;
}

int pd_hal_display_height(void)
{
    return PD_HAL_DISPLAY_HEIGHT;
}

pd_hal_status_t pd_hal_touch_init(void)
{
    ESP_LOGI(TAG, "SIM_TOUCH: init (scripted injection via `sim touch`)");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_touch_read(pd_hal_touch_event_t *out)
{
    (void)out;
    return PD_HAL_ERR_TIMEOUT;
}

/* ---- co-processor model: real frame-level sim arrives in Phase 3 ---- */

pd_hal_status_t pd_hal_coproc_init(void)
{
    ESP_LOGI(TAG, "SIM_COPROC: init (auto-ack off, no faults)");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_coproc_send(const uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0 || len > 256) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "SIM_COPROC: send len=%d first=%02x", (int)len, buf[0]);
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_coproc_recv(uint8_t *buf, size_t cap, size_t timeout_ms, size_t *out_len)
{
    (void)buf;
    (void)cap;
    (void)timeout_ms;
    (void)out_len;
    return PD_HAL_ERR_TIMEOUT;
}

/* ---- net / storage / power / buzzer ---- */

pd_hal_status_t pd_hal_net_connect(void)
{
    ESP_LOGI(TAG, "SIM_NET: esp_wifi/BT never linked under QEMU; open_eth Phase 4 (spike c)");
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_net_status(pd_net_status_t *out_status)
{
    if (out_status != NULL) {
        *out_status = PD_NET_DISCONNECTED;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_storage_ext_mount(void)
{
    ESP_LOGI(TAG, "SIM_STORAGE_EXT: TF card is HIL-only (SD_CS via CH422G EXIO4)");
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_storage_ext_unmount(void)
{
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_storage_ext_present(bool *out_present)
{
    if (out_present != NULL) {
        *out_present = false;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_power_source(pd_power_source_t *out_source)
{
    if (out_source != NULL) {
        *out_source = PD_POWER_SOURCE_DC; /* deterministic for tests */
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_power_battery_mv(uint32_t *out_mv)
{
    if (out_mv != NULL) {
        *out_mv = 3700u; /* deterministic for tests */
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_buzzer_init(void)
{
    ESP_LOGI(TAG, "SIM_BUZZER: init");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_buzzer_beep(uint8_t times, uint32_t on_ms, uint32_t off_ms)
{
    ESP_LOGI(TAG, "SIM_BUZZER: beep times=%d on=%lu off=%lu", (int)times, (unsigned long)on_ms,
             (unsigned long)off_ms);
    return PD_HAL_OK;
}
