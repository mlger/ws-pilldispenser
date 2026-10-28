/*
 * sim_ioexp — in-RAM CH422G register model for the QEMU/host backend
 * (spec A1). Every write is logged with a deterministic marker so scenario
 * tests can observe HAL activity; the model mirrors the chip's write-only
 * output register with a software cache, matching hal_real's cache contract.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "hal_ioexp.h"

static const char *TAG = "sim_ioexp";

static uint8_t s_regs[8];

pd_hal_status_t pd_hal_ioexp_init(void) {
    memset(s_regs, 0, sizeof(s_regs));
    ESP_LOGI(TAG, "SIM_IOEXP: reset");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_write(pd_ioexp_pin_t pin, bool level) {
    if (pin > PD_IOEXP_EXIO5) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    const uint8_t bit = (uint8_t)(1u << (int)pin);
    if (level) {
        s_regs[0] = (uint8_t)(s_regs[0] | bit);
    } else {
        s_regs[0] = (uint8_t)(s_regs[0] & (uint8_t)~bit);
    }
    ESP_LOGI(TAG, "SIM_IOEXP: write exio=%d level=%d regs=%02x", (int)pin, (int)level, s_regs[0]);
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_read(pd_ioexp_pin_t pin, bool *out_level) {
    if (out_level == NULL || pin > PD_IOEXP_EXIO5) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    *out_level = (s_regs[0] >> (int)pin) & 0x1u;
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_dump(uint8_t out_regs[8]) {
    if (out_regs == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    memcpy(out_regs, s_regs, sizeof(s_regs));
    ESP_LOGI(TAG, "SIM_IOEXP: dump=%02x%02x%02x%02x%02x%02x%02x%02x", s_regs[0], s_regs[1],
             s_regs[2], s_regs[3], s_regs[4], s_regs[5], s_regs[6], s_regs[7]);
    return PD_HAL_OK;
}
