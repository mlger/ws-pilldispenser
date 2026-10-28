#ifndef HAL_STORAGE_EXT_H
#define HAL_STORAGE_EXT_H

#include <stdbool.h>
#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * External TF card (SPI: GPIO11 MOSI, GPIO12 SCK, GPIO13 MISO; chip select is
 * CH422G EXIO4). HIL-only in testing: QEMU cannot emulate the CH422G-driven
 * CS path, so hal_sim stubs this interface (see docs/test-plan.md D1 matrix).
 */

pd_hal_status_t pd_hal_storage_ext_mount(void);
pd_hal_status_t pd_hal_storage_ext_unmount(void);
pd_hal_status_t pd_hal_storage_ext_present(bool *out_present);

#ifdef __cplusplus
}
#endif

#endif /* HAL_STORAGE_EXT_H */
