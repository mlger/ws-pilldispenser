/*
 * Scenario test app (Phase 2): the full offline dose pipeline — storage
 * (NVS journal), FSM service (frames + timers + journal replay), scheduler
 * service (tick task + dedup) — with the SIM backend under QEMU. Prints the
 * deterministic PD_READY marker after all services are up and exposes the
 * `sim ...` (hal_sim) and `pd ...` (operator) consoles for scripted scenarios.
 */
#include "esp_app_desc.h"
#include "esp_log.h"

#include "core_fsm.h"
#include "fsm_svc.h"
#include "hal_display.h"
#include "hal_rtc.h"
#include "pd_console.h"
#include "scheduler_svc.h"
#include "storage.h"
#if CONFIG_PD_SIM_CONSOLE
#include "sim_console.h"
#endif

static const char *TAG = "scenario";

static void on_dose_due(uint8_t slot_id, uint64_t dose_key, void *user) {
    (void)user;
    (void)pd_fsm_svc_dose_due(slot_id, dose_key);
}

void app_main(void) {
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "PillDispenser scenario app v%s", app->version);

    time_t now = 0;
    if (pd_hal_rtc_get(&now) == PD_HAL_OK) {
        ESP_LOGI(TAG, "SIM_RTC: boot epoch=%lld", (long long)now);
    }

    (void)pd_hal_display_init();

    if (pd_storage_init() != 0) {
        ESP_LOGE(TAG, "storage init failed; journal/dedup disabled");
    }

    /* FSM first (replays a pending journal entry from a power loss), then the
     * scheduler feeding it. */
    (void)pd_fsm_svc_inventory_init(8);
    if (pd_fsm_svc_start() != 0) {
        ESP_LOGE(TAG, "fsm service start failed");
    }
    (void)pd_sched_svc_set_tz("UTC0");
    if (pd_sched_svc_start(on_dose_due, NULL) != 0) {
        ESP_LOGE(TAG, "scheduler service start failed");
    }

#if CONFIG_PD_TEST_HOOKS
    ESP_LOGI(TAG, "PD_READY");
#endif

#if CONFIG_PD_SIM_CONSOLE
    /* Register every command BEFORE the REPL starts reading: commands
     * arriving in the UART ring are consumed by the REPL task as soon as it
     * runs, and a command looked up while registration is still in flight
     * would be reported as unrecognized. */
    if (pd_console_register() != ESP_OK) {
        ESP_LOGW(TAG, "pd console register failed");
    }
    const esp_err_t repl_err = sim_console_start();
    if (repl_err != ESP_OK) {
        ESP_LOGW(TAG, "sim console start failed: %s", esp_err_to_name(repl_err));
    }
#endif
}
