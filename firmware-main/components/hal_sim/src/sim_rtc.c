/*
 * sim_rtc — QEMU/host clock. Maps the RTC HAL onto gettimeofday/settimeofday
 * with a settable offset so DST edge tests can jump time deterministically
 * (`sim time set <ISO8601>`, `sim tz <POSIX>` console commands).
 */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"

#include "hal_rtc.h"

static const char *TAG = "sim_rtc";

/* Offset (seconds) applied on top of the host clock when the guest cannot set
 * the wall clock directly; used by tests that move time forward. */
static int64_t s_offset_s;

static time_t sim_now(void) {
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return (time_t)(ts.tv_sec + s_offset_s);
}

pd_hal_status_t pd_hal_rtc_get(time_t *out_epoch) {
    if (out_epoch == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    *out_epoch = sim_now();
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_rtc_set(time_t epoch) {
    struct timespec ts = {.tv_sec = epoch, .tv_nsec = 0};
    if (clock_settime(CLOCK_REALTIME, &ts) == 0) {
        s_offset_s = 0;
        ESP_LOGI(TAG, "SIM_RTC: set epoch=%lld", (long long)epoch);
        return PD_HAL_OK;
    }
    /* Fallback: apply as offset when the sandbox forbids setting the clock. */
    s_offset_s = (int64_t)epoch - sim_now();
    ESP_LOGI(TAG, "SIM_RTC: set via offset epoch=%lld (delta=%lld s)", (long long)epoch,
             (long long)s_offset_s);
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_rtc_set_posix_tz(const char *posix_tz) {
    if (posix_tz == NULL || posix_tz[0] == '\0') {
        return PD_HAL_ERR_INVALID_ARG;
    }
    if (setenv("TZ", posix_tz, 1) != 0) {
        return PD_HAL_ERR_IO;
    }
    tzset();
    ESP_LOGI(TAG, "SIM_RTC: tz set to '%s'", posix_tz);
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_rtc_valid(bool *out_valid) {
    if (out_valid == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    *out_valid = true;
    return PD_HAL_OK;
}

/* Parse "YYYY-MM-DDTHH:MM:SS" into an epoch using the active TZ. */
bool sim_rtc_parse_iso(const char *iso, time_t *out_epoch) {
    if (iso == NULL || out_epoch == NULL) {
        return false;
    }
    struct tm t = {0};
    int consumed = 0;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d%n", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min,
               &t.tm_sec, &consumed) < 6) {
        return false;
    }
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1;
    *out_epoch = mktime(&t);
    return *out_epoch != (time_t)-1;
}
