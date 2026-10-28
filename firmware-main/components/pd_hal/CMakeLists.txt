# HAL interface component: headers only. The implementations live in
# hal_real / hal_sim, gated by CONFIG_PD_HAL_REAL / CONFIG_PD_HAL_SIM.
idf_component_register(SRCS "src/pd_hal.c" INCLUDE_DIRS "include")
