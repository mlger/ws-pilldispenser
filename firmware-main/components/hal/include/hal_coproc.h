#ifndef HAL_COPROC_H
#define HAL_COPROC_H

#include "hal_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RS485 link to the dispense-mechanism co-processor (GPIO43 RXD / GPIO44 TXD,
 * auto direction, 115200 8N1). Frames are PD-Link v1 (docs/protocol.md); the
 * codec itself lives in core_protocol — this HAL only moves bytes.
 * hal_sim embeds an in-process co-processor model with fault injection.
 */

pd_hal_status_t pd_hal_coproc_init(void);
pd_hal_status_t pd_hal_coproc_send(const uint8_t *buf, size_t len);
/* Receive one frame into buf. Returns PD_HAL_ERR_TIMEOUT when nothing arrives
 * within timeout_ms. */
pd_hal_status_t pd_hal_coproc_recv(uint8_t *buf, size_t cap, size_t timeout_ms, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_COPROC_H */
