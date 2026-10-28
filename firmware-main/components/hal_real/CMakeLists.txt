# hal_real links only when the PD_HAL_REAL backend is selected; otherwise it
# registers as an empty header-only component so exactly one backend is linked.
# NOTE: no public INCLUDE_DIRS — the HAL contracts live in components/hal.
if(CONFIG_PD_HAL_REAL)
    idf_component_register(SRCS "src/hal_real_stub.c"
                           REQUIRES pd_hal log)
else()
    idf_component_register()
endif()
