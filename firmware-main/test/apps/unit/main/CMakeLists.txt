# Unity test app: compiles the component test files directly (same sources the
# host CTest build uses) and drives them through the IDF unity menu, which
# pytest-embedded's run_all_single_board_cases() parses and executes.
set(CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../components")

idf_component_register(
    SRCS "unit_main.c"
         "${CORE_DIR}/core_protocol/test/test_core_protocol.c"
         "${CORE_DIR}/core_scheduler/test/test_core_scheduler.c"
         "${CORE_DIR}/core_fsm/test/test_core_fsm.c"
         "${CORE_DIR}/core_inventory/test/test_core_inventory.c"
    INCLUDE_DIRS "."
                 "${CORE_DIR}/core_protocol/include"
                 "${CORE_DIR}/core_scheduler/include"
                 "${CORE_DIR}/core_fsm/include"
                 "${CORE_DIR}/core_inventory/include"
    REQUIRES unity)
