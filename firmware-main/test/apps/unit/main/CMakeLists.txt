# Unity test app: compiles the component test files directly (same sources the
# host CTest build uses) and drives them through the IDF unity menu, which
# pytest-embedded's run_all_single_board_cases() parses and executes.
#
# The HAL contract suite (components/pd_hal/test) runs against whichever
# backend the app's sdkconfig selects; REQUIRES lists BOTH backends — exactly
# one of them registers sources (see components/hal_* CMakeLists), so the
# linkage stays backend-agnostic (spec A4: contract tests on SIM and REAL).
# The storage suite is QEMU-only (NVS needs flash; not part of host CTest).
set(CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../components")

idf_component_register(
    SRCS "unit_main.c"
         "${CORE_DIR}/core_protocol/test/test_core_protocol.c"
         "${CORE_DIR}/core_scheduler/test/test_core_scheduler.c"
         "${CORE_DIR}/core_fsm/test/test_core_fsm.c"
         "${CORE_DIR}/core_inventory/test/test_core_inventory.c"
         "${CORE_DIR}/pd_hal/test/test_hal_contract.c"
         "${CORE_DIR}/storage/test/test_storage.c"
    INCLUDE_DIRS "."
                 "${CORE_DIR}/core_protocol/include"
                 "${CORE_DIR}/core_scheduler/include"
                 "${CORE_DIR}/core_fsm/include"
                 "${CORE_DIR}/core_inventory/include"
                 "${CORE_DIR}/pd_hal/include"
                 "${CORE_DIR}/storage/include"
    REQUIRES unity pd_hal hal_sim hal_real storage nvs_flash
             core_protocol core_scheduler core_fsm core_inventory
    # The TEST_CASE registrations are self-contained constructor sections with
    # no symbol references from unit_main; without WHOLE_ARCHIVE the linker
    # never pulls the test objects out of libmain.a and the unity menu boots
    # empty (the suite passes vacuously - root cause of the Phase 0.5 bug).
    WHOLE_ARCHIVE)
