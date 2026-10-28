#ifndef HAL_POWER_H
#define HAL_POWER_H

#include <stdbool.h>
#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Power monitoring (board runs from Type-C 5 V, DC 7-36 V, or a 3.7 V single-
 * cell lithium battery with a connector switch). ADC calibration is avoided at
 * boot so the QEMU leg stays deterministic (docs/test-plan.md).
 */

typedef enum {
    PD_POWER_SOURCE_UNKNOWN = 0,
    PD_POWER_SOURCE_DC = 1,
    PD_POWER_SOURCE_BATTERY = 2,
} pd_power_source_t;

pd_hal_status_t pd_hal_power_source(pd_power_source_t *out_source);
pd_hal_status_t pd_hal_power_battery_mv(uint32_t *out_mv);

#ifdef __cplusplus
}
#endif

#endif /* HAL_POWER_H */
