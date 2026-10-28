# Host-side unit tests for the pure core_* components (spec D5).
# Same test sources as the ESP-IDF unit test app — only the unity TEST_CASE
# shim differs (include/unity.h wrapper -> include/unity_host.h). No ESP-IDF
# required.
#
#   cmake -S firmware-main/host_test -B build-host -DSANITIZE=ON
#   cmake --build build-host -j
#   ctest --test-dir build-host --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(pd_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(SANITIZE "Enable ASan/UBSan" OFF)

set(COMPONENTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../components")
set(SHIM_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(UNITY_DIR "${CMAKE_CURRENT_LIST_DIR}/unity")

# --- Vendored Unity (ThrowTheSwitch, v2.6.1) ---
# unity.c resolves its own headers relative to its source dir; no PUBLIC
# include path is published so the wrapper below always wins the search.
add_library(unity STATIC "${UNITY_DIR}/unity.c")

# --- Component under test + its test file ---
# Include order matters: the shim dir (wrapper unity.h) precedes the vendored
# Unity dir so #include_next chains correctly.
function(pd_host_test component)
    set(src "${COMPONENTS_DIR}/${component}/src/${component}.c")
    set(tst "${COMPONENTS_DIR}/${component}/test/test_${component}.c")
    if(NOT EXISTS "${src}" OR NOT EXISTS "${tst}")
        message(FATAL_ERROR "host test: missing sources for component '${component}'")
    endif()

    add_executable(test_${component}
        "${src}"
        "${tst}"
        "${CMAKE_CURRENT_LIST_DIR}/main/host_main.c"
    )
    target_include_directories(test_${component} PRIVATE
        "${SHIM_DIR}"
        "${COMPONENTS_DIR}/${component}/include"
        "${UNITY_DIR}"
    )
    target_link_libraries(test_${component} PRIVATE unity)
    target_compile_options(test_${component} PRIVATE -Wall -Wextra -Werror)

    if(SANITIZE)
        target_compile_options(test_${component} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        target_link_options(test_${component} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif()

    add_test(NAME ${component} COMMAND test_${component})
    set_tests_properties(${component} PROPERTIES
        LABELS "host;core"
        PASS_REGULAR_EXPRESSION "HOST_TESTS_DONE failed=0"
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endfunction()

pd_host_test(core_protocol)
pd_host_test(core_scheduler)
pd_host_test(core_fsm)
pd_host_test(core_inventory)

enable_testing()
