/*
 * `sim ...` console command (CONFIG_PD_SIM_CONSOLE). Subcommands used by
 * interactive QEMU sessions and scripted scenario tests:
 *   sim time set <ISO8601>       — jump the clock (sim_rtc -> pd_hal_rtc_set)
 *   sim tz <POSIX TZ string>     — switch timezone (DST edge tests)
 *   sim touch <x> <y> [down|up]  — inject a touch event (sim_touch)
 *   sim coproc auto-ack on|off   — toggle the in-process coproc model
 *   sim coproc fault <type>      — inject crc|timeout|jam|empty
 *   sim fb dump                  — dump the framebuffer (ui golden tests)
 *   sim ioexp                    — dump the CH422G register model
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

#include "hal_ioexp.h"
#include "hal_rtc.h"

#include "sim_console.h"
#include "sim_private.h"

static const char *TAG = "sim_cmd";

static int cmd_time_set(const char *iso) {
    time_t epoch = 0;
    /* sim_rtc_parse_iso lives in sim_rtc.c (shared SIM clock model). */
    if (!sim_rtc_parse_iso(iso, &epoch)) {
        ESP_LOGW(TAG, "invalid ISO8601 timestamp '%s'", iso);
        return 1;
    }
    const pd_hal_status_t st = pd_hal_rtc_set(epoch);
    ESP_LOGI(TAG, "SIM: time set %s -> epoch=%lld rc=%d", iso, (long long)epoch, (int)st);
    return st == PD_HAL_OK ? 0 : 1;
}

static int cmd_sim(int argc, char **argv) {
    if (argc < 2) {
        ESP_LOGW(TAG, "usage: sim <time|tz|touch|coproc|fb|ioexp> [args]");
        return 1;
    }

    if (strcmp(argv[1], "time") == 0) {
        if (argc == 4 && strcmp(argv[2], "set") == 0) {
            return cmd_time_set(argv[3]);
        }
        ESP_LOGW(TAG, "usage: sim time set <ISO8601>");
        return 1;
    }

    if (strcmp(argv[1], "tz") == 0 && argc == 3) {
        const pd_hal_status_t st = pd_hal_rtc_set_posix_tz(argv[2]);
        ESP_LOGI(TAG, "SIM: tz %s rc=%d", argv[2], (int)st);
        return st == PD_HAL_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "touch") == 0) {
        if (argc >= 4) {
            const char *state = (argc >= 5) ? argv[4] : "down";
            const bool down = (strcmp(state, "up") != 0);
            sim_touch_inject(atoi(argv[2]), atoi(argv[3]), down);
            return 0;
        }
        ESP_LOGW(TAG, "usage: sim touch <x> <y> [down|up]");
        return 1;
    }

    if (strcmp(argv[1], "coproc") == 0) {
        if (argc == 4 && strcmp(argv[2], "auto-ack") == 0) {
            sim_coproc_set_auto_ack(strcmp(argv[3], "on") == 0);
            return 0;
        }
        if (argc == 4 && strcmp(argv[2], "fault") == 0) {
            if (strcmp(argv[3], "crc") == 0) {
                sim_coproc_set_fault(SIM_COPROC_FAULT_CRC);
            } else if (strcmp(argv[3], "timeout") == 0 || strcmp(argv[3], "no-ack") == 0) {
                sim_coproc_set_fault(SIM_COPROC_FAULT_TIMEOUT);
            } else if (strcmp(argv[3], "jam") == 0) {
                sim_coproc_set_fault(SIM_COPROC_FAULT_JAM);
            } else if (strcmp(argv[3], "empty") == 0) {
                sim_coproc_set_fault(SIM_COPROC_FAULT_EMPTY);
            } else {
                ESP_LOGW(TAG, "unknown fault '%s' (crc|timeout|no-ack|jam|empty)", argv[3]);
                return 1;
            }
            return 0;
        }
        ESP_LOGW(TAG,
                 "usage: sim coproc auto-ack on|off | sim coproc fault <crc|timeout|jam|empty>");
        return 1;
    }

    if (strcmp(argv[1], "fb") == 0 && argc == 3 && strcmp(argv[2], "dump") == 0) {
        /* Phase 1 spike b wires this to esp_lcd_qemu_rgb + PNG dump. */
        ESP_LOGI(TAG, "SIM: fb dump requested (esp_lcd_qemu_rgb lands with spike b)");
        return 0;
    }

    if (strcmp(argv[1], "ioexp") == 0 && argc == 2) {
        uint8_t regs[8];
        const pd_hal_status_t st = pd_hal_ioexp_dump(regs);
        ESP_LOGI(TAG, "SIM: ioexp dump rc=%d regs=%02x%02x%02x%02x%02x%02x%02x%02x", (int)st,
                 regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7]);
        return st == PD_HAL_OK ? 0 : 1;
    }

    ESP_LOGW(TAG, "unknown sim subcommand");
    return 1;
}

esp_err_t sim_console_start(void) {
    const esp_console_cmd_t sim_cmd = {
        .command = "sim",
        .help = "Simulation hooks: sim time set <ISO8601> | sim tz <TZ> | "
                "sim touch <x> <y> [down|up] | sim coproc auto-ack on|off | "
                "sim coproc fault <type> | sim fb dump | sim ioexp",
        .hint = NULL,
        .func = &cmd_sim,
    };
    esp_err_t err = esp_console_cmd_register(&sim_cmd);
    if (err != ESP_OK) {
        return err;
    }

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "pd>";
    /* IDF v5.5 console struct: channel + pin numbers only; line settings come
     * from the UART driver defaults. Runs under QEMU, so UART0 default pins
     * are fine (GPIO43/44 are RS485 only on real hardware, which never links
     * this file — the SIM console is SIM-backend only). */
    esp_console_dev_uart_config_t uart_config = {
        .channel = 0,
        .baud_rate = 115200,
        .tx_gpio_num = -1,
        .rx_gpio_num = -1,
    };
    esp_console_repl_t *repl = NULL;
    err = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_console_start_repl(repl);
    if (err == ESP_OK) {
        /* Deterministic marker: console input is now being drained. QEMU
         * scenario tests gate their first dut.write() on this line, otherwise
         * commands arriving before the REPL task runs are lost in the UART
         * FIFO. */
        ESP_LOGI(TAG, "SIM: console ready");
    }
    return err;
}
