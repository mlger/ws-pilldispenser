#ifndef PCF85063_H
#define PCF85063_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PCF85063 RTC on the Waveshare ESP32-S3-Touch-LCD-4.3B (I2C 0x51).
 *
 * Time is exposed as a Unix epoch. The chip's year counter is 0-99; the board
 * convention (Waveshare demo) maps it onto 1970-2069 (YEAR_BASE). The
 * oscillator-stopped / voltage-low flag (VL, seconds register bit 7) is the
 * power-loss indicator behind pd_hal_rtc_valid (FMEA-06).
 */

/* Control register 1 bits. */
#define PCF85063_CTRL1_CAP_SEL 0x01u /* 12.5 pF oscillator load */
#define PCF85063_CTRL1_STOP    0x20u
#define PCF85063_CTRL1_SR      0x10u /* software reset */

/* Seconds register bit 7: voltage-low / oscillator-stopped indicator. */
#define PCF85063_SECONDS_VL 0x80u

esp_err_t pcf85063_create(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out_dev);

/* Configure control register 1 (12.5 pF load, clock running). */
esp_err_t pcf85063_init(i2c_master_dev_handle_t dev);

/* Read the current time as a Unix epoch (gmtime semantics). */
esp_err_t pcf85063_get_epoch(i2c_master_dev_handle_t dev, time_t *out_epoch);

/* Write a new time (also clears the VL power-loss flag). */
esp_err_t pcf85063_set_epoch(i2c_master_dev_handle_t dev, time_t epoch);

/* True when the VL flag is clear (clock was never powered down). */
esp_err_t pcf85063_valid(i2c_master_dev_handle_t dev, bool *out_valid);

#ifdef __cplusplus
}
#endif

#endif /* PCF85063_H */
