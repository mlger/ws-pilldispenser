#ifndef HAL_RTC_H
#define HAL_RTC_H

#include <stddef.h>
#include <time.h>
#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Real-time clock (PCF85063 on I2C). hal_real talks to the chip; hal_sim maps
 * onto gettimeofday/settimeofday with a settable offset (sim time set). The
 * POSIX TZ string drives scheduler math (core_scheduler) — see docs/fmea.md
 * for the drift/reconciliation risk entries.
 */

pd_hal_status_t pd_hal_rtc_get(time_t *out_epoch);
pd_hal_status_t pd_hal_rtc_set(time_t epoch);
/* Persist/apply the POSIX TZ string (e.g. "CET-1CEST,M3.5.0,M10.5.0/3"). */
pd_hal_status_t pd_hal_rtc_set_posix_tz(const char *posix_tz);
/* True when the RTC reports a valid time (power-loss indicator cleared). */
pd_hal_status_t pd_hal_rtc_valid(bool *out_valid);

#ifdef __cplusplus
}
#endif

#endif /* HAL_RTC_H */
