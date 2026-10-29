/*
 * Unit test app entry: prints the unity test menu, which pytest-embedded
 * (dut.run_all_single_board_cases) drives over the QEMU UART console.
 * NVS is initialized up front so the storage component suite can run.
 */
#include <stdio.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "unity.h"

static void init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("unit", "nvs needs erase (rc=%s), retrying", esp_err_to_name(err));
        (void)nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE("unit", "nvs init failed: %s (storage tests will fail)", esp_err_to_name(err));
    }
}

void app_main(void) {
    init_nvs();
    printf("\n=== PillDispenser unit tests (esp32s3) ===\n");
    unity_run_menu();
}
