/*
 * hal_sim internal hooks shared between the sim driver modules and the
 * `sim ...` console. Not part of the public HAL contract.
 */
#ifndef SIM_PRIVATE_H
#define SIM_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "hal_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- sim_rtc (sim_rtc.c) --- */

/* Parse "YYYY-MM-DDTHH:MM:SS" into an epoch using the active TZ. */
bool sim_rtc_parse_iso(const char *iso, time_t *out_epoch);

/* --- sim_touch (sim_touch.c) --- */

/* Inject one scripted touch event (console `sim touch x y [down|up]`). */
void sim_touch_inject(int x, int y, bool pressed);

/* --- sim_coproc (sim_coproc.c) --- */

typedef enum {
    SIM_COPROC_FAULT_NONE = 0,
    SIM_COPROC_FAULT_CRC = 1,     /* responses carry a corrupted CRC */
    SIM_COPROC_FAULT_TIMEOUT = 2, /* no response at all (alias: no-ack) */
    SIM_COPROC_FAULT_JAM = 3,     /* STATUS_RESP reports jam=1 */
    SIM_COPROC_FAULT_EMPTY = 4,   /* STATUS_RESP reports empty=1 */
} sim_coproc_fault_t;

void sim_coproc_set_auto_ack(bool on);
void sim_coproc_set_fault(sim_coproc_fault_t fault);
const char *sim_coproc_fault_name(sim_coproc_fault_t fault);

#ifdef __cplusplus
}
#endif

#endif /* SIM_PRIVATE_H */
