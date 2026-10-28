cmake_minimum_required(VERSION 3.16)

# Pull in the monorepo components (core_*, hal, hal_real, hal_sim).
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(pd_unit_tests)
