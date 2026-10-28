#ifndef GT911_H
#define GT911_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GT911 capacitive touch controller (I2C GPIO8/9 shared bus; IRQ GPIO4;
 * reset sequenced by hal_real through the CH422G TP_RST line).
 *
 * Minimal polling driver: read the coordinate buffer status register, pull
 * point 1 when a touch is present, clear the status flag. Config download is
 * NOT implemented — the panel ships with a valid config in its NV memory
 * (VERIFY on HIL: if a board needs reconfig, add a config blob upload here).
 */

/* GT911 responds at 0x5D when INT is low during reset (board wiring), 0x14
 * when high. The driver probes both. */
#define GT911_ADDR_PRIMARY 0x5Du
#define GT911_ADDR_ALT     0x14u

/* Register map (subset). */
#define GT911_REG_STATUS 0x814Eu /* bit7 buffer ready, bits 3:0 point count */
#define GT911_REG_POINT1 0x8150u /* 8 bytes: track, x_lo, x_hi, y_lo, y_hi, ... */

#define GT911_STATUS_READY       0x80u
#define GT911_STATUS_POINTS_MASK 0x0Fu

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} gt911_touch_t;

esp_err_t gt911_create(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out_dev);

/* Probe both addresses; returns ESP_OK when the controller ACKs. */
esp_err_t gt911_probe(i2c_master_dev_handle_t dev);

/* Poll one touch sample. pressed=false when the panel reports no contact. */
esp_err_t gt911_read_touch(i2c_master_dev_handle_t dev, gt911_touch_t *out);

#ifdef __cplusplus
}
#endif

#endif /* GT911_H */
