#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "esp_log.h"

#include "hal_display.h"
#if CONFIG_PD_SIM_CONSOLE
#include "sim_console.h"
#endif

/*
 * Phase 0.5 main application: boot banner, HAL contract smoke call, the
 * deterministic PD_READY marker expected by the QEMU smoke test, and (SIM
 * backend only) the `sim ...` console. Scheduler/FSM wiring lands in Phase 2.
 */
static const char *TAG = "pd_main";

void app_main(void) {
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "PillDispenser main v%s (idf %s)", app->version, esp_get_idf_version());

    /* HAL contract smoke: proves the selected backend is linked and callable. */
    const pd_hal_status_t st = pd_hal_display_init();
    ESP_LOGI(TAG, "display init rc=%d (%s)", (int)st,
             st == PD_HAL_OK ? "ok" : "expected stub on phase 0.5");

#if CONFIG_PD_TEST_HOOKS
    ESP_LOGI(TAG, "PD_READY");
#endif

#if CONFIG_PD_SIM_CONSOLE
    const esp_err_t repl_err = sim_console_start();
    if (repl_err != ESP_OK) {
        ESP_LOGW(TAG, "sim console start failed: %s", esp_err_to_name(repl_err));
    }
#endif
}
