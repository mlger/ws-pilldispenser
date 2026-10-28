/*
 * GT911 capacitive touch driver — polling implementation over the shared I2C
 * bus (see gt911.h). Coordinate buffer protocol per GT911 Programming Guide:
 * read 0x814E, if bit7 set and points > 0 read point 1 at 0x8150, then write
 * 0x00 to 0x814E to release the buffer.
 */
#include "gt911.h"

#include "esp_log.h"

static const char *TAG = "gt911";

#define GT911_TIMEOUT_MS 20u

static i2c_master_dev_handle_t s_dev;

static esp_err_t gt911_write_reg(uint16_t reg, const uint8_t *data, size_t len) {
    uint8_t buf[2 + 8];
    if (2 + len > sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    buf[0] = (uint8_t)(reg >> 8);
    buf[1] = (uint8_t)(reg & 0xFFu);
    for (size_t i = 0; i < len; i++) {
        buf[2 + i] = data[i];
    }
    return i2c_master_transmit(s_dev, buf, 2 + len, GT911_TIMEOUT_MS);
}

static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *data, size_t len) {
    const uint8_t addr[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFFu)};
    return i2c_master_transmit_receive(s_dev, addr, sizeof(addr), data, len, GT911_TIMEOUT_MS);
}

esp_err_t gt911_create(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out_dev) {
    if (out_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = GT911_ADDR_PRIMARY,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, out_dev);
    if (err == ESP_OK) {
        s_dev = *out_dev;
    }
    return err;
}

esp_err_t gt911_probe(i2c_master_dev_handle_t dev) {
    uint8_t product[4] = {0};
    esp_err_t err = gt911_read_reg(0x8140u, product, 4); /* "9xx" product id */
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "product id: %c%c%c%c", product[0], product[1], product[2], product[3]);
    return ESP_OK;
}

esp_err_t gt911_read_touch(i2c_master_dev_handle_t dev, gt911_touch_t *out) {
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    out->pressed = false;

    uint8_t status = 0;
    esp_err_t err = gt911_read_reg(GT911_REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        return err;
    }
    if ((status & GT911_STATUS_READY) == 0) {
        return ESP_OK; /* no new sample */
    }

    const uint8_t points = status & GT911_STATUS_POINTS_MASK;
    if (points > 0) {
        uint8_t p[8] = {0};
        err = gt911_read_reg(GT911_REG_POINT1, p, sizeof(p));
        if (err == ESP_OK) {
            out->x = (uint16_t)((p[2] << 8) | p[1]);
            out->y = (uint16_t)((p[4] << 8) | p[3]);
            out->pressed = true;
        }
    }

    /* Clear the buffer status flag so the controller can report new data. */
    const uint8_t zero = 0;
    (void)gt911_write_reg(GT911_REG_STATUS, &zero, 1);
    return err;
}
