# hal_real links only when the PD_HAL_REAL backend is selected; otherwise it
# registers as an empty interface-only component so exactly one backend is
# linked. The HAL contracts (hal_*.h) live in components/pd_hal.
#
# IMPORTANT (root cause of the Phase 0.5 CI build failure): component
# requirements are collected in an EARLY CMake scripting pass
# (tools/cmake/scripts/component_get_requirements.cmake) that runs BEFORE
# Kconfig generates sdkconfig, so CONFIG_* variables do not exist there.
# Wrapping idf_component_register() in if(CONFIG_...) therefore registers the
# component with EMPTY requirements during that pass and the real-requirements
# pass never repairs them — sources compile, but pd_hal include dirs never
# propagate. Registration itself must be unconditional; CONFIG_* may only gate
# the source list.
set(PD_REAL_SRCS "")
if(CONFIG_PD_HAL_REAL)
    list(APPEND PD_REAL_SRCS "src/hal_real_stub.c")
endif()

idf_component_register(SRCS "${PD_REAL_SRCS}"
                       REQUIRES pd_hal log)
