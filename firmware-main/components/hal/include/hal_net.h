#ifndef HAL_NET_H
#define HAL_NET_H

#include <stdbool.h>
#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Network connectivity. hal_real: Wi-Fi STA (provisioning lands in Phase 4);
 * hal_sim: never links esp_wifi/BT (not emulated in QEMU) — the QEMU leg uses
 * OpenCores Ethernet (CONFIG_ETH_USE_OPENETH) with the broker on 10.0.2.2.
 */

typedef enum {
    PD_NET_DISCONNECTED = 0,
    PD_NET_CONNECTING = 1,
    PD_NET_CONNECTED = 2,
    PD_NET_ERROR = 3,
} pd_net_status_t;

/* Kick off a connection using stored credentials. Non-blocking. */
pd_hal_status_t pd_hal_net_connect(void);
pd_hal_status_t pd_hal_net_status(pd_net_status_t *out_status);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NET_H */
