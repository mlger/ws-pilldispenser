# Unity test app: compiles the component test files directly (same sources the
# host CTest build uses) and drives them through the IDF unity menu, which
# pytest-embedded's run_all_single_board_cases() parses and executes.
#
# The HAL contract suite (components/pd_hal/test) runs against whichever
# backend the app's sdkconfig selects; REQUIRES lists BOTH backends — exactly
# one of them registers sources (see components/hal_* CMakeLists), so the
# linkage stays backend-agnostic (spec A4: contract tests on SIM and REAL).
set(CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../components")

idf_component_register(
    SRCS "unit_main.c"
         "${CORE_DIR}/core_protocol/test/test_core_protocol.c"
         "${CORE_DIR}/core_scheduler/test/test_core_scheduler.c"
         "${CORE_DIR}/core_fsm/test/test_core_fsm.c"
         "${CORE_DIR}/core_inventory/test/test_core_inventory.c"
         "${CORE_DIR}/pd_hal/test/test_hal_contract.c"
    INCLUDE_DIRS "."
                 "${CORE_DIR}/core_protocol/include"
                 "${CORE_DIR}/core_scheduler/include"
                 "${CORE_DIR}/core_fsm/include"
                 "${CORE_DIR}/core_inventory/include"
                 "${CORE_DIR}/pd_hal/include"
    REQUIRES unity pd_hal hal_sim hal_real)
