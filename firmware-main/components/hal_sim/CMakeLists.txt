# hal_sim links only when the PD_HAL_SIM backend is selected; otherwise it
# registers as an empty interface-only component so exactly one backend is
# linked. See hal_real/CMakeLists.txt for why registration is unconditional:
# component requirements are expanded in an early CMake scripting pass that
# runs before sdkconfig exists, so REQUIRES must not depend on CONFIG_*.
# Only the source/include lists may be gated on CONFIG_*.
set(PD_SIM_SRCS "")
set(PD_SIM_INCLUDES "")

if(CONFIG_PD_HAL_SIM)
    list(APPEND PD_SIM_SRCS "src/sim_console.c" "src/sim_coproc.c" "src/sim_ioexp.c"
                            "src/sim_rtc.c" "src/sim_stubs.c" "src/sim_touch.c")
    list(APPEND PD_SIM_INCLUDES "include")
endif()

# REQUIRES are unconditional: in a PD_HAL_REAL build this component is an
# interface-only placeholder and pulling console/driver/core_protocol is
# harmless (nothing links against an empty static archive).
idf_component_register(SRCS "${PD_SIM_SRCS}"
                       INCLUDE_DIRS "${PD_SIM_INCLUDES}"
                       # `console` = esp_console in IDF v5.5.x naming.
                       REQUIRES pd_hal log console driver core_protocol)
