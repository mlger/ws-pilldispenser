/*
 * `sim ...` console command (CONFIG_PD_SIM_CONSOLE). Registers subcommands
 * used by interactive QEMU sessions and scripted scenario tests:
 *   sim time set <ISO8601>       — jump the clock (sim_rtc)
 *   sim tz <POSIX TZ string>     — switch timezone (DST edge tests)
 *   sim touch <x> <y> [down|up]  — inject a touch event
 *   sim coproc auto-ack on|off   — toggle the in-process coproc model
 *   sim coproc fault <type>      — inject crc|timeout|jam|empty|no-ack
 *   sim fb dump                  — dump the framebuffer (ui golden tests)
 *   sim ioexp                    — dump the CH422G register model
 */
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

#include "sim_console.h"

static const char *TAG = "sim_cmd";

static int cmd_sim(int argc, char **argv) {
    if (argc < 2) {
        ESP_LOGW(TAG, "usage: sim <time|tz|touch|coproc|fb|ioexp> [args]");
        return 1;
    }

    if (strcmp(argv[1], "time") == 0) {
        if (argc == 4 && strcmp(argv[2], "set") == 0) {
            ESP_LOGI(TAG, "SIM: time set %s", argv[3]);
            /* Phase 2 wires this to sim_rtc + the scheduler; the marker is the
             * Phase 0.5 contract. */
            return 0;
        }
        ESP_LOGW(TAG, "usage: sim time set <ISO8601>");
        return 1;
    }

    if (strcmp(argv[1], "tz") == 0 && argc == 3) {
        ESP_LOGI(TAG, "SIM: tz %s", argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "touch") == 0) {
        if (argc >= 4) {
            const char *state = (argc >= 5) ? argv[4] : "down";
            ESP_LOGI(TAG, "SIM: touch x=%s y=%s %s", argv[2], argv[3], state);
            return 0;
        }
        ESP_LOGW(TAG, "usage: sim touch <x> <y> [down|up]");
        return 1;
    }

    if (strcmp(argv[1], "coproc") == 0) {
        if (argc == 4 && strcmp(argv[2], "auto-ack") == 0) {
            ESP_LOGI(TAG, "SIM: coproc auto-ack=%s", argv[3]);
            return 0;
        }
        if (argc == 4 && strcmp(argv[2], "fault") == 0) {
            ESP_LOGI(TAG, "SIM: coproc fault=%s", argv[3]);
            return 0;
        }
        ESP_LOGW(
            TAG,
            "usage: sim coproc auto-ack on|off | sim coproc fault <crc|timeout|jam|empty|no-ack>");
        return 1;
    }

    if (strcmp(argv[1], "fb") == 0 && argc == 3 && strcmp(argv[2], "dump") == 0) {
        ESP_LOGI(TAG, "SIM: fb dump requested");
        return 0;
    }

    if (strcmp(argv[1], "ioexp") == 0 && argc == 2) {
        ESP_LOGI(TAG, "SIM: ioexp dump requested");
        return 0;
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
    esp_console_dev_uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_console_repl_t *repl = NULL;
    err = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (err != ESP_OK) {
        return err;
    }
    return esp_console_start_repl(repl);
}
