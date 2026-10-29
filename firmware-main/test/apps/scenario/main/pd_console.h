#ifndef PD_CONSOLE_H
#define PD_CONSOLE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `pd ...` operator/test console (scenario app, Phase 2):
 *   pd status                 - FSM state, current dose, journal, slots
 *   pd slots list             - dose slot table
 *   pd slots set <id> <HH:MM> <once|daily|weekly> [MTWTFSS]
 *                             - upsert a slot and persist it
 *   pd dose trigger <slot>    - force DOSE_DUE with the current wall-minute key
 *   pd dose reboot <slot>     - power-loss emulation: journal write-ahead then
 *                               esp_restart() mid-dispense (FMEA-04 replay path)
 *   pd reset | pd retry       - operator FSM actions
 */
esp_err_t pd_console_register(void);

#ifdef __cplusplus
}
#endif

#endif /* PD_CONSOLE_H */
