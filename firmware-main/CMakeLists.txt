cmake_minimum_required(VERSION 3.16)

# Nested driver components (components/drivers/{ch422g,gt911,pcf85063}) are
# below the auto-scanned depth, so add the directory explicitly.
list(APPEND EXTRA_COMPONENT_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/components/drivers")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# firmware-main/version.txt (written by release-please) is picked up
# automatically as PROJECT_VER and shown on the About screen + MQTT telemetry.
project(pill_dispenser_main)
