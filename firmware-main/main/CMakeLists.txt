# Exactly one HAL backend is linked; the others register as empty components.
idf_component_register(SRCS "pd_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES hal hal_real hal_sim esp_console esp_app_format log)
