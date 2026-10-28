/*
 * hal_real — real Waveshare ESP32-S3-Touch-LCD-4.3B backend (Phase 1, spec F).
 *
 * Wraps drivers/{ch422g,pcf85063,gt911} over the shared I2C bus, the ESP32-S3
 * LCD_CAM RGB panel (ST7262 timing), and a UART1 RS485 link. Interfaces that
 * belong to later phases (net/storage_ext/power/buzzer) stay explicit stubs
 * with one-shot markers so a REAL boot is never silent about missing pieces.
 *
 * Console note: the board's only Type-C is the native USB (GPIO19/20,
 * USB-Serial-JTAG) — Waveshare's own demos set
 * CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y. GPIO43/44 belong to the RS485
 * transceiver, so the console must NOT claim UART0 on real hardware; the QEMU
 * leg keeps UART0 (sdkconfig.qemu) because USB-Serial-JTAG is a QEMU stub.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "board_pins.h"
#include "ch422g.h"
#include "core_protocol.h"
#include "gt911.h"
#include "pcf85063.h"

#include "hal_buzzer.h"
#include "hal_coproc.h"
#include "hal_display.h"
#include "hal_ioexp.h"
#include "hal_net.h"
#include "hal_power.h"
#include "hal_rtc.h"
#include "hal_storage_ext.h"
#include "hal_touch.h"

static const char *TAG = "hal_real";

/* ---- shared state ---- */

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_ioexp_dev;
static i2c_master_dev_handle_t s_rtc_dev;
static i2c_master_dev_handle_t s_touch_dev;
static esp_lcd_panel_handle_t s_panel;
static bool s_backlight_on;

/* Last known CH422G IO output register (write-only on the chip; cached here
 * so pd_hal_ioexp_dump and read-modify-writes work — the header contract). */
static uint8_t s_ioexp_cache;

static esp_err_t i2c_bus_get(void) {
    if (s_i2c_bus != NULL) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t cfg = {
        .i2c_port = 0,
        .sda_io_num = PD_PIN_I2C_SDA,
        .scl_io_num = PD_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = ch422g_create(s_i2c_bus, &s_ioexp_dev);
    if (err == ESP_OK) {
        err = pcf85063_create(s_i2c_bus, &s_rtc_dev);
    }
    if (err == ESP_OK) {
        err = gt911_create(s_i2c_bus, &s_touch_dev);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c device create failed: %s", esp_err_to_name(err));
    }
    return err;
}

/* Read-modify-write the cached CH422G IO output register. */
static pd_hal_status_t ioexp_update(uint8_t set_mask, uint8_t clear_mask) {
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    s_ioexp_cache = (uint8_t)((s_ioexp_cache | set_mask) & (uint8_t)~clear_mask);
    err = ch422g_io_output(s_ioexp_dev, s_ioexp_cache);
    return err == ESP_OK ? PD_HAL_OK : PD_HAL_ERR_IO;
}

/* ---- display (RGB panel + CH422G LCD_RST/DISP) ---- */

pd_hal_status_t pd_hal_display_init(void) {
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    /* Power-on the panel domain: TP_RST/DISP/LCD_RST/SD_CS high (Waveshare
     * backlight-on sequence); LCD_RST release lets the ST7262 come up. */
    pd_hal_status_t st =
        ioexp_update(CH422G_IO_TP_RST | CH422G_IO_DISP | CH422G_IO_LCD_RST | CH422G_IO_SD_CS, 0);
    if (st != PD_HAL_OK) {
        return st;
    }
    esp_rom_delay_us(10 * 1000);

    static const int data_pins[16] = {
        PD_PIN_RGB_DATA0,  PD_PIN_RGB_DATA1,  PD_PIN_RGB_DATA2,  PD_PIN_RGB_DATA3,
        PD_PIN_RGB_DATA4,  PD_PIN_RGB_DATA5,  PD_PIN_RGB_DATA6,  PD_PIN_RGB_DATA7,
        PD_PIN_RGB_DATA8,  PD_PIN_RGB_DATA9,  PD_PIN_RGB_DATA10, PD_PIN_RGB_DATA11,
        PD_PIN_RGB_DATA12, PD_PIN_RGB_DATA13, PD_PIN_RGB_DATA14, PD_PIN_RGB_DATA15,
    };
    const esp_lcd_rgb_panel_config_t panel_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings =
            {
                .pclk_hz = PD_LCD_PCLK_HZ,
                .h_res = PD_HAL_DISPLAY_WIDTH,
                .v_res = PD_HAL_DISPLAY_HEIGHT,
                .hsync_pulse_width = PD_LCD_HSYNC_PW,
                .hsync_back_porch = PD_LCD_HSYNC_BP,
                .hsync_front_porch = PD_LCD_HSYNC_FP,
                .vsync_pulse_width = PD_LCD_VSYNC_PW,
                .vsync_back_porch = PD_LCD_VSYNC_BP,
                .vsync_front_porch = PD_LCD_VSYNC_FP,
                .flags.pclk_active_neg = 1,
            },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = PD_HAL_DISPLAY_WIDTH * 10,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = PD_PIN_RGB_HSYNC,
        .vsync_gpio_num = PD_PIN_RGB_VSYNC,
        .de_gpio_num = PD_PIN_RGB_DE,
        .pclk_gpio_num = PD_PIN_RGB_PCLK,
        .disp_gpio_num = -1, /* DISP is CH422G EXIO2, driven above */
        .data_gpio_nums = {data_pins[0], data_pins[1], data_pins[2], data_pins[3], data_pins[4],
                           data_pins[5], data_pins[6], data_pins[7], data_pins[8], data_pins[9],
                           data_pins[10], data_pins[11], data_pins[12], data_pins[13],
                           data_pins[14], data_pins[15]},
        .flags.fb_in_psram = 1,
    };
    err = esp_lcd_new_rgb_panel(&panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rgb panel create failed: %s", esp_err_to_name(err));
        return PD_HAL_ERR_IO;
    }
    err = esp_lcd_panel_init(s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rgb panel init failed: %s", esp_err_to_name(err));
        return PD_HAL_ERR_IO;
    }
    ESP_LOGI(TAG, "PD_DISPLAY: rgb panel ready (800x480 RGB565)");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_display_backlight(bool on) {
    pd_hal_status_t st = ioexp_update(on ? CH422G_IO_DISP : 0, on ? 0 : CH422G_IO_DISP);
    if (st == PD_HAL_OK) {
        s_backlight_on = on;
        ESP_LOGI(TAG, "PD_DISPLAY: backlight=%d", (int)on);
    }
    return st;
}

int pd_hal_display_width(void) {
    return PD_HAL_DISPLAY_WIDTH;
}

int pd_hal_display_height(void) {
    return PD_HAL_DISPLAY_HEIGHT;
}

/* ---- touch (GT911; reset sequenced via CH422G TP_RST + TP_IRQ) ---- */

pd_hal_status_t pd_hal_touch_init(void) {
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }

    /* Waveshare GT911 reset: TP_RST low with TP_IRQ low, then TP_RST high
     * (INT low during reset selects I2C address 0x5D). */
    (void)ioexp_update(0, CH422G_IO_TP_RST);
    esp_rom_delay_us(100 * 1000);
    const gpio_config_t int_out = {
        .pin_bit_mask = 1ULL << PD_PIN_TP_IRQ,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&int_out);
    (void)gpio_set_level(PD_PIN_TP_IRQ, 0);
    esp_rom_delay_us(100 * 1000);
    (void)ioexp_update(CH422G_IO_TP_RST, 0);
    esp_rom_delay_us(200 * 1000);
    /* Release TP_IRQ: GT911 drives it as interrupt output; we poll. */
    (void)gpio_reset_pin(PD_PIN_TP_IRQ);

    err = gt911_probe(s_touch_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gt911 probe failed: %s", esp_err_to_name(err));
        return PD_HAL_ERR_IO;
    }
    ESP_LOGI(TAG, "PD_TOUCH: gt911 ready");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_touch_read(pd_hal_touch_event_t *out) {
    if (out == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    gt911_touch_t t;
    esp_err_t err = gt911_read_touch(s_touch_dev, &t);
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    if (!t.pressed) {
        return PD_HAL_ERR_TIMEOUT;
    }
    out->x = t.x;
    out->y = t.y;
    out->pressed = true;
    return PD_HAL_OK;
}

/* ---- IO expander (CH422G) ---- */

pd_hal_status_t pd_hal_ioexp_init(void) {
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    s_ioexp_cache = 0;
    pd_hal_status_t st = ioexp_update(0, 0xFF); /* all pins low, output mode */
    ESP_LOGI(TAG, "PD_IOEXP: reset (cache=00)");
    return st;
}

pd_hal_status_t pd_hal_ioexp_write(pd_ioexp_pin_t pin, bool level) {
    if (pin > PD_IOEXP_EXIO5) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    const uint8_t bit = (uint8_t)(1u << (int)pin);
    return ioexp_update(level ? bit : 0, level ? 0 : bit);
}

pd_hal_status_t pd_hal_ioexp_read(pd_ioexp_pin_t pin, bool *out_level) {
    if (out_level == NULL || pin > PD_IOEXP_EXIO5) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    /* Only the DI lines (EXIO0/EXIO5) are meaningful inputs; reading them
     * switches the chip to input mode and restores the cached outputs. */
    uint8_t inputs = 0;
    err = ch422g_io_input(s_ioexp_dev, &inputs);
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    (void)ch422g_io_output(s_ioexp_dev, s_ioexp_cache);
    *out_level = ((inputs >> (int)pin) & 0x1u) != 0;
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_ioexp_dump(uint8_t out_regs[8]) {
    if (out_regs == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    memset(out_regs, 0, 8);
    out_regs[0] = s_ioexp_cache; /* last cached register state (header contract) */
    return PD_HAL_OK;
}

/* ---- RTC (PCF85063) ---- */

pd_hal_status_t pd_hal_rtc_get(time_t *out_epoch) {
    if (out_epoch == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    return pcf85063_get_epoch(s_rtc_dev, out_epoch) == ESP_OK ? PD_HAL_OK : PD_HAL_ERR_IO;
}

pd_hal_status_t pd_hal_rtc_set(time_t epoch) {
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    /* Keep the system clock in sync so strftime/mktime agree with the RTC. */
    const struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
    (void)settimeofday(&tv, NULL);
    return pcf85063_set_epoch(s_rtc_dev, epoch) == ESP_OK ? PD_HAL_OK : PD_HAL_ERR_IO;
}

pd_hal_status_t pd_hal_rtc_set_posix_tz(const char *posix_tz) {
    /* TZ persistence lands with NVS storage in Phase 2; the live value is
     * applied to the C library now (same contract as hal_sim). */
    if (posix_tz == NULL || posix_tz[0] == '\0') {
        return PD_HAL_ERR_INVALID_ARG;
    }
    if (setenv("TZ", posix_tz, 1) != 0) {
        return PD_HAL_ERR_IO;
    }
    tzset();
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_rtc_valid(bool *out_valid) {
    if (out_valid == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    esp_err_t err = i2c_bus_get();
    if (err != ESP_OK) {
        return PD_HAL_ERR_IO;
    }
    return pcf85063_valid(s_rtc_dev, out_valid) == ESP_OK ? PD_HAL_OK : PD_HAL_ERR_IO;
}

/* ---- co-processor link (RS485 UART1, PD-Link v1 frames) ---- */

pd_hal_status_t pd_hal_coproc_init(void) {
    const uart_config_t uart_cfg = {
        .baud_rate = PD_RS485_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(PD_RS485_UART_NUM, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart install failed: %s", esp_err_to_name(err));
        return PD_HAL_ERR_IO;
    }
    err = uart_param_config(PD_RS485_UART_NUM, &uart_cfg);
    if (err == ESP_OK) {
        err = uart_set_pin(PD_RS485_UART_NUM, PD_PIN_RS485_TXD, PD_PIN_RS485_RXD,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart config failed: %s", esp_err_to_name(err));
        return PD_HAL_ERR_IO;
    }
    ESP_LOGI(TAG, "PD_COPROC: rs485 uart ready (%d baud)", PD_RS485_BAUD);
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_coproc_send(const uint8_t *buf, size_t len) {
    if (buf == NULL || len == 0 || len > PD_PROTO_MAX_FRAME) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    const int written = uart_write_bytes(PD_RS485_UART_NUM, buf, (int)len);
    return written == (int)len ? PD_HAL_OK : PD_HAL_ERR_IO;
}

pd_hal_status_t pd_hal_coproc_recv(uint8_t *buf, size_t cap, size_t timeout_ms, size_t *out_len) {
    if (buf == NULL || cap == 0 || out_len == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    *out_len = 0;

    /* Byte-stream assembler: scan for SOF, decode, resync on CRC errors. */
    uint8_t acc[2 * PD_PROTO_MAX_FRAME];
    size_t acc_len = 0;
    const TickType_t step = pdMS_TO_TICKS(10);
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        uint8_t byte;
        const int n = uart_read_bytes(PD_RS485_UART_NUM, &byte, 1, step);
        if (n <= 0) {
            continue;
        }
        if (acc_len == 0 && byte != PD_PROTO_SOF) {
            continue; /* resync: skip garbage to the next SOF */
        }
        if (acc_len < sizeof(acc)) {
            acc[acc_len++] = byte;
        } else {
            acc_len = 0; /* overlong garbage — restart */
            continue;
        }

        pd_frame_t frame;
        const int decoded = pd_proto_try_decode(acc, acc_len, &frame);
        if (decoded > 0) {
            if ((size_t)decoded > cap) {
                return PD_HAL_ERR_INVALID_ARG;
            }
            memcpy(buf, acc, (size_t)decoded);
            *out_len = (size_t)decoded;
            return PD_HAL_OK;
        }
        if (decoded < 0) {
            /* Framing/CRC error: drop the first byte and resynchronize. */
            memmove(acc, acc + 1, acc_len - 1);
            acc_len--;
        }
    }
    return PD_HAL_ERR_TIMEOUT;
}

/* ---- net / storage_ext / power / buzzer: later phases ---- */

pd_hal_status_t pd_hal_net_connect(void) {
    static bool logged;
    if (!logged) {
        ESP_LOGW(TAG, "HAL_REAL: net connect arrives with Phase 4 (Wi-Fi provisioning)");
        logged = true;
    }
    return PD_HAL_ERR_UNSUPPORTED;
}

pd_hal_status_t pd_hal_net_status(pd_net_status_t *out_status) {
    if (out_status != NULL) {
        *out_status = PD_NET_DISCONNECTED;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_storage_ext_mount(void) {
    static bool logged;
    if (!logged) {
        ESP_LOGW(TAG, "HAL_REAL: TF card mount arrives with Phase 6 (HIL-only testing)");
        logged = true;
    }
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_storage_ext_unmount(void) {
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_storage_ext_present(bool *out_present) {
    if (out_present != NULL) {
        *out_present = false;
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_power_source(pd_power_source_t *out_source) {
    /* VERIFY: battery/DC sense net not confirmed on the schematic; Phase 3. */
    static bool logged;
    if (!logged) {
        ESP_LOGW(TAG, "HAL_REAL: power source sensing not wired yet (Phase 3)");
        logged = true;
    }
    if (out_source != NULL) {
        *out_source = PD_POWER_SOURCE_UNKNOWN;
    }
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_power_battery_mv(uint32_t *out_mv) {
    (void)out_mv;
    return PD_HAL_ERR_NOT_IMPLEMENTED;
}

pd_hal_status_t pd_hal_buzzer_init(void) {
    /* VERIFY: no buzzer on the stock 4.3B; audible alert routes through the
     * optocoupler DO lines with the mechanism (Phase 3). */
    ESP_LOGI(TAG, "PD_BUZZER: no onboard buzzer; DO-line alert lands in Phase 3");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_buzzer_beep(uint8_t times, uint32_t on_ms, uint32_t off_ms) {
    (void)times;
    (void)on_ms;
    (void)off_ms;
    return PD_HAL_ERR_UNSUPPORTED;
}
