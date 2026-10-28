#ifndef SIM_CONSOLE_H
#define SIM_CONSOLE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Register the `sim` console command and start the UART REPL.
 * Compiled only when CONFIG_PD_SIM_CONSOLE is set (hal_sim backend). */
esp_err_t sim_console_start(void);

#ifdef __cplusplus
}
#endif

#endif /* SIM_CONSOLE_H */
