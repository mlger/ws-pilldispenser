#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RGB display (4.3" 800x480, driven by the ESP32-S3 LCD_CAM peripheral).
 * hal_real: esp_lcd RGB panel (ST7262 panel — VERIFY panel driver IC).
 * hal_sim:  esp_lcd_qemu_rgb virtual framebuffer (Phase 1, spike b).
 */

#define PD_HAL_DISPLAY_WIDTH 800
#define PD_HAL_DISPLAY_HEIGHT 480

pd_hal_status_t pd_hal_display_init(void);
pd_hal_status_t pd_hal_display_backlight(bool on);
int pd_hal_display_width(void);
int pd_hal_display_height(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DISPLAY_H */
