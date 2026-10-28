# hal_real links only when the PD_HAL_REAL backend is selected; otherwise it
# registers as an empty interface-only component so exactly one backend is
# linked. The HAL contracts (hal_*.h) live in components/pd_hal.
#
# IMPORTANT: component requirements are collected in an EARLY CMake scripting
# pass (tools/cmake/scripts/component_get_requirements.cmake) that runs BEFORE
# Kconfig generates sdkconfig, so CONFIG_* variables do not exist there.
# Registration itself must be unconditional; CONFIG_* may only gate the
# source list (root cause of the Phase 0.5 CI build failure).
set(PD_REAL_SRCS "")
if(CONFIG_PD_HAL_REAL)
    list(APPEND PD_REAL_SRCS "src/hal_real.c")
endif()

idf_component_register(SRCS "${PD_REAL_SRCS}"
                       INCLUDE_DIRS "include"
                       REQUIRES pd_hal log core_protocol ch422g pcf85063 gt911
                                esp_lcd esp_driver_i2c esp_driver_uart esp_driver_gpio)
