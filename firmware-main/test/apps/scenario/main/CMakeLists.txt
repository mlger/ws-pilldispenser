# Scenario app: the full firmware shape (services + selected HAL backend)
# built with PD_HAL_SIM so scenario tests exercise real code paths under QEMU.
# `console` = esp_console in IDF v5.5.x naming.
idf_component_register(SRCS "scenario_main.c" "pd_console.c"
                       INCLUDE_DIRS "."
                       REQUIRES pd_hal hal_real hal_sim storage scheduler fsm
                                core_fsm core_scheduler core_inventory core_protocol
                                console esp_app_format log)
