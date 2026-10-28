# Scenario app: the full firmware shape (main + selected HAL backend) built
# with PD_HAL_SIM so scenario tests exercise real code paths under QEMU.
# `console` = esp_console in IDF v5.5.x naming.
idf_component_register(SRCS "scenario_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES hal hal_real hal_sim console esp_app_format log)
