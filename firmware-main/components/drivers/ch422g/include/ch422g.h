#ifndef CH422G_H
#define CH422G_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CH422G I2C IO expander as wired on the Waveshare ESP32-S3-Touch-LCD-4.3B.
 *
 * The chip has no slave address: the "address" byte selects one of four
 * function registers (Waveshare demo + CH422 datasheet):
 *   0x24 MODE   — write: 0x01 IO output enable, 0x02 auto scan, 0x04 OD enable,
 *                 0x08 sleep; 0x00 puts IO pins into input mode
 *   0x26 IO_IN  — read: IO0..IO7 input states
 *   0x38 IO_OUT — write: IO0..IO7 output levels (IO output mode)
 *   0x23 OD_OUT — write: OC0..OC3 open-drain output levels
 *
 * Board net map (IO output register bits):
 *   bit0 DI0 (optocoupler input monitor), bit1 TP_RST, bit2 DISP (backlight),
 *   bit3 LCD_RST, bit4 SD_CS, bit5 DI1.
 */

#define CH422G_REG_MODE   0x24u
#define CH422G_REG_OD_OUT 0x23u
#define CH422G_REG_IO_OUT 0x38u
#define CH422G_REG_IO_IN  0x26u

/* MODE register values. */
#define CH422G_MODE_INPUT 0x00u
#define CH422G_MODE_IO_OE 0x01u /* IO0..IO7 push-pull output mode */

/* IO output register bits (board net map). */
#define CH422G_IO_DI0     (1u << 0)
#define CH422G_IO_TP_RST  (1u << 1)
#define CH422G_IO_DISP    (1u << 2)
#define CH422G_IO_LCD_RST (1u << 3)
#define CH422G_IO_SD_CS   (1u << 4)
#define CH422G_IO_DI1     (1u << 5)

/* Create the device handle for the function-register interface. One handle
 * serves all registers (they share the address space); every transfer starts
 * with the register selector byte. */
esp_err_t ch422g_create(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out_dev);

/* Select IO output mode and drive the IO0..IO7 register to io_mask. */
esp_err_t ch422g_io_output(i2c_master_dev_handle_t dev, uint8_t io_mask);

/* Switch IO pins to input and read the IO0..IO7 states into out_mask. */
esp_err_t ch422g_io_input(i2c_master_dev_handle_t dev, uint8_t *out_mask);

/* Drive the OC0..OC3 open-drain register to od_mask. */
esp_err_t ch422g_od_output(i2c_master_dev_handle_t dev, uint8_t od_mask);

#ifdef __cplusplus
}
#endif

#endif /* CH422G_H */
