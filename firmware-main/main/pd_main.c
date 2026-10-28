#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "esp_log.h"

#include "hal_buzzer.h"
#include "hal_coproc.h"
#include "hal_display.h"
#include "hal_ioexp.h"
#include "hal_rtc.h"
#include "hal_touch.h"
#if CONFIG_PD_SIM_CONSOLE
#include "sim_console.h"
#endif

/*
 * Phase 1 main application: board bring-up through the selected HAL backend,
 * the deterministic PD_READY marker expected by the QEMU smoke test, and
 * (SIM backend only) the `sim ...` console. Scheduler/FSM wiring lands in
 * Phase 2 — bring-up failures are logged, never panicked, so a half-wired
 * bench board still reaches the console (FMEA-02: no boot-time hard faults).
 */
static const char *TAG = "pd_main";

#define BRING_UP(name, call)                                                                       \
    do {                                                                                           \
        const pd_hal_status_t st_ = (call);                                                        \
        ESP_LOGI(TAG, "bring-up %-10s rc=%d (%s)", name, (int)st_,                                 \
                 st_ == PD_HAL_OK ? "ok" : "deferred");                                            \
    } while (0)

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
