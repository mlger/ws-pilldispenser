# Exactly one HAL backend is linked; the others register as empty components.
# NOTE: the console component is named `console` in IDF v5.5.x (renamed to
# `esp_console` in IDF v6 — tracked by the nightly canary leg).
idf_component_register(SRCS "pd_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES pd_hal hal_real hal_sim console esp_app_format log)
