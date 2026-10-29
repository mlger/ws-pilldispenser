/*
 * PillDispenser main application — Phase 2: board bring-up through the
 * selected HAL backend plus the offline dose pipeline (storage journal, FSM
 * service, scheduler service). Bring-up and service failures are logged,
 * never panicked, so a half-wired bench board still reaches the console
 * (FMEA-02: no boot-time hard faults).
 */
#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "esp_log.h"

#include "core_fsm.h"
#include "fsm_svc.h"
#include "hal_buzzer.h"
#include "hal_coproc.h"
#include "hal_display.h"
#include "hal_ioexp.h"
#include "hal_rtc.h"
#include "hal_touch.h"
#include "scheduler_svc.h"
#include "storage.h"
#if CONFIG_PD_SIM_CONSOLE
#include "sim_console.h"
#endif

static const char *TAG = "pd_main";

#define BRING_UP(name, call)                                                                       \
    do {                                                                                           \
        const pd_hal_status_t st_ = (call);                                                        \
        ESP_LOGI(TAG, "bring-up %-10s rc=%d (%s)", name, (int)st_,                                 \
                 st_ == PD_HAL_OK ? "ok" : "deferred");                                            \
    } while (0)

static void on_dose_due(uint8_t slot_id, uint64_t dose_key, void *user) {
    (void)user;
    (void)pd_fsm_svc_dose_due(slot_id, dose_key);
}

void app_main(void) {
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "PillDispenser main v%s (idf %s)", app->version, esp_get_idf_version());

    /* Board bring-up order: IO expander first (it owns LCD_RST/backlight),
     * then display, touch, RTC, and the RS485 link. */
    BRING_UP("ioexp", pd_hal_ioexp_init());
    BRING_UP("display", pd_hal_display_init());
    BRING_UP("backlight", pd_hal_display_backlight(true));
    BRING_UP("touch", pd_hal_touch_init());
    BRING_UP("rtc", pd_hal_rtc_set_posix_tz("UTC0"));
    BRING_UP("coproc", pd_hal_coproc_init());
    BRING_UP("buzzer", pd_hal_buzzer_init());

    time_t now = 0;
    if (pd_hal_rtc_get(&now) == PD_HAL_OK) {
        bool rtc_valid = false;
        (void)pd_hal_rtc_valid(&rtc_valid);
        ESP_LOGI(TAG, "rtc time=%lld valid=%d", (long long)now, (int)rtc_valid);
    }

    /* Offline dose pipeline (Phase 2). Storage failure degrades to a
     * no-journal mode: the FSM service still runs but the FMEA-04 replay
     * guarantee is logged as unavailable. */
    if (pd_storage_init() != 0) {
        ESP_LOGE(TAG, "storage init failed; journal/dedup disabled");
    }
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
    const esp_err_t repl_err = sim_console_start();
    if (repl_err != ESP_OK) {
        ESP_LOGW(TAG, "sim console start failed: %s", esp_err_to_name(repl_err));
    }
#endif
}
