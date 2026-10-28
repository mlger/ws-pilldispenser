/*
 * PCF85063 RTC driver (I2C 0x51) — see pcf85063.h.
 * Register map per NXP PCF85063 datasheet rev. 7; read/write sequences ported
 * from the Waveshare ESP32-S3-Touch-LCD-4.3B demo (BCD time fields, masks).
 */
#include "pcf85063.h"

#include <sys/time.h>

#include "esp_log.h"

static const char *TAG = "pcf85063";

#define PCF85063_I2C_ADDR   0x51u
#define PCF85063_TIMEOUT_MS 100u

/* Register addresses. */
#define PCF85063_REG_CTRL1   0x00u
#define PCF85063_REG_CTRL2   0x01u
#define PCF85063_REG_SECONDS 0x04u /* seconds .. year are 0x04..0x0A contiguous */
#define PCF85063_REG_YEAR    0x0Au

/* The board maps the chip's 0-99 year counter onto 1970-2069 (Waveshare demo
 * convention, YEAR_OFFSET 1970). Keep in sync with HIL fixtures. */
#define PCF85063_YEAR_BASE 1970

static uint8_t bcd_to_dec(uint8_t v) {
    return (uint8_t)((v & 0x0Fu) + ((v >> 4) * 10u));
}

static uint8_t dec_to_bcd(uint8_t v) {
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/* Days from 1970-01-01 (Howard Hinnant's days_from_civil; newlib has no
 * timegm). Valid for the year range the chip can hold (1970-2069). */
static time_t utc_days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (time_t)era * 146097 + (time_t)doe - 719468;
}

/* struct tm (UTC fields) -> Unix epoch, no TZ involvement. */
static time_t utc_mktime(const struct tm *t) {
    const time_t days =
        utc_days_from_civil(t->tm_year + 1900, (unsigned)t->tm_mon + 1u, (unsigned)t->tm_mday);
    return days * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
}

esp_err_t pcf85063_create(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out_dev) {
    if (out_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(bus, &cfg, out_dev);
}

esp_err_t pcf85063_init(i2c_master_dev_handle_t dev) {
    /* CTRL_1: normal mode, 12.5 pF load, clock running (Waveshare demo). */
    const uint8_t buf[2] = {PCF85063_REG_CTRL1, PCF85063_CTRL1_CAP_SEL};
    esp_err_t err = i2c_master_transmit(dev, buf, sizeof(buf), PCF85063_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t pcf85063_get_epoch(i2c_master_dev_handle_t dev, time_t *out_epoch) {
    if (out_epoch == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t start = PCF85063_REG_SECONDS;
    uint8_t buf[7] = {0}; /* VL+sec, min, hour, day, weekday, month, year */
    esp_err_t err =
        i2c_master_transmit_receive(dev, &start, 1, buf, sizeof(buf), PCF85063_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
        return err;
    }

    struct tm t = {0};
    t.tm_sec = bcd_to_dec(buf[0] & 0x7Fu);
    t.tm_min = bcd_to_dec(buf[1] & 0x7Fu);
    t.tm_hour = bcd_to_dec(buf[2] & 0x3Fu);
    t.tm_mday = bcd_to_dec(buf[3] & 0x3Fu);
    t.tm_wday = bcd_to_dec(buf[4] & 0x07u);
    t.tm_mon = bcd_to_dec(buf[5] & 0x1Fu) - 1; /* chip: 1-12, tm: 0-11 */
    t.tm_year = (bcd_to_dec(buf[6]) + PCF85063_YEAR_BASE) - 1900;
    t.tm_isdst = 0; /* the RTC stores the plain wall time of the last set; the
                       scheduler applies TZ (core_scheduler) */

    *out_epoch = utc_mktime(&t);
    return ESP_OK;
}

esp_err_t pcf85063_set_epoch(i2c_master_dev_handle_t dev, time_t epoch) {
    struct tm t;
    gmtime_r(&epoch, &t);

    /* Burst-write seconds..year in one transaction (auto-increment). Writing
     * the seconds register clears the VL flag. */
    const uint8_t buf[8] = {
        PCF85063_REG_SECONDS,
        dec_to_bcd((uint8_t)t.tm_sec),
        dec_to_bcd((uint8_t)t.tm_min),
        dec_to_bcd((uint8_t)t.tm_hour),
        dec_to_bcd((uint8_t)t.tm_mday),
        dec_to_bcd((uint8_t)t.tm_wday),
        dec_to_bcd((uint8_t)(t.tm_mon + 1)),
        dec_to_bcd((uint8_t)((t.tm_year + 1900) - PCF85063_YEAR_BASE)),
    };
    esp_err_t err = i2c_master_transmit(dev, buf, sizeof(buf), PCF85063_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t pcf85063_valid(i2c_master_dev_handle_t dev, bool *out_valid) {
    if (out_valid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t reg = PCF85063_REG_SECONDS;
    uint8_t sec = 0;
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, &sec, 1, PCF85063_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "valid read failed: %s", esp_err_to_name(err));
        return err;
    }
    *out_valid = (sec & PCF85063_SECONDS_VL) == 0;
    return ESP_OK;
}
