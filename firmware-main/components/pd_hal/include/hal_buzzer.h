#ifndef HAL_BUZZER_H
#define HAL_BUZZER_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Audible alert. hal_sim is a deterministic stub that logs the request. */
pd_hal_status_t pd_hal_buzzer_init(void);
pd_hal_status_t pd_hal_buzzer_beep(uint8_t times, uint32_t on_ms, uint32_t off_ms);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BUZZER_H */
