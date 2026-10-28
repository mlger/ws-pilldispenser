#ifndef HAL_IOEXP_H
#define HAL_IOEXP_H

#include <stdbool.h>
#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CH422G I2C IO expander. EXIO2 = DISP (backlight enable), EXIO4 = SD_CS,
 * EXIO0 = DI0, EXIO5 = DI1; optocoupler-isolated DI/DO lines. hal_sim keeps an
 * in-RAM register model and logs every write (contract-test observable).
 */

typedef enum {
    PD_IOEXP_EXIO0 = 0,
    PD_IOEXP_EXIO1 = 1, /* TP_RST */
    PD_IOEXP_EXIO2 = 2, /* DISP */
    PD_IOEXP_EXIO3 = 3,
    PD_IOEXP_EXIO4 = 4, /* SD_CS */
    PD_IOEXP_EXIO5 = 5, /* DI1 */
} pd_ioexp_pin_t;

pd_hal_status_t pd_hal_ioexp_init(void);
pd_hal_status_t pd_hal_ioexp_write(pd_ioexp_pin_t pin, bool level);
pd_hal_status_t pd_hal_ioexp_read(pd_ioexp_pin_t pin, bool *out_level);
/* Dump the raw register model (hal_sim) or the last cached register state
 * (hal_real) into out (8 bytes). Diagnostic + test hook. */
pd_hal_status_t pd_hal_ioexp_dump(uint8_t out_regs[8]);

#ifdef __cplusplus
}
#endif

#endif /* HAL_IOEXP_H */
