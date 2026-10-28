/*
 * CH422G IO expander driver — I2C function-register interface (see ch422g.h).
 * Ported from the Waveshare ESP32-S3-Touch-LCD-4.3B demo to the IDF v5.5
 * i2c_master API. HIL contract: mode switch + register write must ACK.
 */
#include "ch422g.h"

#include "esp_log.h"

static const char *TAG = "ch422g";

#define CH422G_I2C_ADDR   0x24u
#define CH422G_TIMEOUT_MS 100u

esp_err_t ch422g_create(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out_dev) {
    if (out_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(bus, &cfg, out_dev);
}

esp_err_t ch422g_io_output(i2c_master_dev_handle_t dev, uint8_t io_mask) {
    /* Mode first (IO output enable), then the level register. */
    uint8_t mode_buf[2] = {CH422G_REG_MODE, CH422G_MODE_IO_OE};
    esp_err_t err = i2c_master_transmit(dev, mode_buf, sizeof(mode_buf), CH422G_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mode IO_OE failed: %s", esp_err_to_name(err));
        return err;
    }
    const uint8_t out_buf[2] = {CH422G_REG_IO_OUT, io_mask};
    err = i2c_master_transmit(dev, out_buf, sizeof(out_buf), CH422G_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "io_out %02x failed: %s", io_mask, esp_err_to_name(err));
    }
    return err;
}

esp_err_t ch422g_io_input(i2c_master_dev_handle_t dev, uint8_t *out_mask) {
    if (out_mask == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t mode_buf[2] = {CH422G_REG_MODE, CH422G_MODE_INPUT};
    esp_err_t err = i2c_master_transmit(dev, mode_buf, sizeof(mode_buf), CH422G_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mode INPUT failed: %s", esp_err_to_name(err));
        return err;
    }
    const uint8_t in_reg = CH422G_REG_IO_IN;
    uint8_t value = 0;
    err = i2c_master_transmit_receive(dev, &in_reg, 1, &value, 1, CH422G_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "io_in failed: %s", esp_err_to_name(err));
        return err;
    }
    *out_mask = value;
    return ESP_OK;
}

esp_err_t ch422g_od_output(i2c_master_dev_handle_t dev, uint8_t od_mask) {
    /* OD output mode: mode register with OD_EN cleared (Waveshare demo), then
     * the OC register. */
    const uint8_t mode_buf[2] = {CH422G_REG_MODE, CH422G_MODE_INPUT};
    esp_err_t err = i2c_master_transmit(dev, mode_buf, sizeof(mode_buf), CH422G_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mode OD failed: %s", esp_err_to_name(err));
        return err;
    }
    const uint8_t out_buf[2] = {CH422G_REG_OD_OUT, od_mask};
    err = i2c_master_transmit(dev, out_buf, sizeof(out_buf), CH422G_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "od_out %02x failed: %s", od_mask, esp_err_to_name(err));
    }
    return err;
}
