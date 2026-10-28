/*
 * Scenario test app (Phase 0.5): boots the full component shape with the SIM
 * backend, prints the deterministic PD_READY marker, and exposes the `sim ...`
 * console for scripted scenarios. Scheduler/FSM/coproc wiring lands in
 * Phases 2-3 (see docs/test-plan.md scenario suite list).
 */
#include "esp_app_desc.h"
#include "esp_log.h"

#include "hal_display.h"
#include "hal_rtc.h"
#if CONFIG_PD_SIM_CONSOLE
#include "sim_console.h"
#endif

static const char *TAG = "scenario";

void app_main(void) {
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "PillDispenser scenario app v%s", app->version);

    time_t now = 0;
    if (pd_hal_rtc_get(&now) == PD_HAL_OK) {
        ESP_LOGI(TAG, "SIM_RTC: boot epoch=%lld", (long long)now);
    }

    (void)pd_hal_display_init();

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
