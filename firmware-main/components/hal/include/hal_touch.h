#ifndef HAL_TOUCH_H
#define HAL_TOUCH_H

#include <stdbool.h>
#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Capacitive touch (GT911, 5-point, I2C GPIO8/9, IRQ GPIO4, RST via CH422G
 * EXIO1). hal_sim injects scripted events from the `sim touch` console command.
 */

typedef struct {
    int x;
    int y;
    bool pressed;
} pd_hal_touch_event_t;

pd_hal_status_t pd_hal_touch_init(void);
/* Poll one pending event; returns PD_HAL_ERR_TIMEOUT when the queue is empty. */
pd_hal_status_t pd_hal_touch_read(pd_hal_touch_event_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TOUCH_H */
