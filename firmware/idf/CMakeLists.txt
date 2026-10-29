cmake_minimum_required(VERSION 3.24)
# Native qualification probe; it is not the BLE/product entry point.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/../../scripts/check_idf_sources.py" "$ENV{IDF_PATH}"
    COMMAND_ERROR_IS_FATAL ANY)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
set(PROJECT_VER "idf6.1-qualification")
project(larp_idf_qualification)
if(NOT "$ENV{IDF_VERSION}" STREQUAL "6.1.0")
    message(FATAL_ERROR "Qualify and pin a new ESP-IDF release before changing 6.1.0")
endif()
if(NOT IDF_TARGET STREQUAL "esp32")
    message(FATAL_ERROR "Only the classic ESP32 target is currently under qualification")
endif()
