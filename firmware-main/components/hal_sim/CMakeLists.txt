# hal_sim links only when the PD_HAL_SIM backend is selected; otherwise it
# registers as an empty header-only component so exactly one backend is linked.
set(SIM_SRCS "src/sim_rtc.c" "src/sim_stubs.c")
if(CONFIG_PD_SIM_CONSOLE)
    list(APPEND SIM_SRCS "src/sim_console.c")
endif()

if(CONFIG_PD_HAL_SIM)
    idf_component_register(SRCS "${SIM_SRCS}"
                           INCLUDE_DIRS "include"
                           # `console` = esp_console in IDF v5.5.x naming.
                           REQUIRES hal log console driver)
else()
    idf_component_register()
endif()
