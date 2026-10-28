cmake_minimum_required(VERSION 3.24)
project(larp_escape_host LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/LarpCoreSources.cmake)
add_library(larp_core STATIC ${LARP_CORE_SOURCES})
target_include_directories(larp_core PUBLIC include)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(larp_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
file(GLOB profile_tool_sources CONFIGURE_DEPENDS src/ProfileTool/*.cpp)
add_executable(larp-profile ${profile_tool_sources})
target_link_libraries(larp-profile PRIVATE larp_core)
include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG cbcd08fa7de711053a3deec6339ee89cad5d2697
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(unity)
    add_executable(configured-control test/test_configured_control/test_main.cpp)
    target_include_directories(configured-control PRIVATE
        test/test_configured_control/host_stubs
        lib/FreeRtosGameControlChannel lib/FreeRtosCommandFrameQueue
        lib/FreeRtosProfileFrameQueue)
    target_link_libraries(configured-control PRIVATE larp_core unity)
    add_test(NAME configured-control COMMAND configured-control)
    set_tests_properties(configured-control PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    # Existing independent wire/effect/state-machine cases use a normal main().
    add_executable(led-wire test/test_led_ble_wire/main.cpp
        firmware/led/src/LedWire.cpp firmware/led/src/NfcBleWire.cpp)
    target_include_directories(led-wire PRIVATE firmware/led/include)
    if(WIN32)
        target_link_libraries(led-wire PRIVATE bcrypt)
    else()
        find_package(OpenSSL REQUIRED)
        target_link_libraries(led-wire PRIVATE OpenSSL::Crypto)
    endif()
    add_test(NAME led-wire COMMAND led-wire "${CMAKE_SOURCE_DIR}/docs/led/fixtures/wire-v2.json")
endif()

# Optional real PSA backend: supplied by the exact qualified ESP-IDF source tree.
set(LARP_PSA_IDF_PATH "" CACHE PATH "ESP-IDF v6.1 source tree for real host PSA tests")
if(BUILD_TESTING AND LARP_PSA_IDF_PATH)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/scripts/check_idf_sources.py" "${LARP_PSA_IDF_PATH}"
        COMMAND_ERROR_IS_FATAL ANY)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(GEN_FILES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${LARP_PSA_IDF_PATH}/components/mbedtls/mbedtls/tf-psa-crypto"
                     "${CMAKE_BINARY_DIR}/tf-psa-crypto")
    # Espressif's sources retain two legacy include paths supplied by its
    # ESP-only port. Host tests forward them to the identical private headers.
    target_include_directories(builtin PRIVATE "${CMAKE_SOURCE_DIR}/test/psa_host_compat")
    target_include_directories(tfpsacrypto PRIVATE "${CMAKE_SOURCE_DIR}/test/psa_host_compat")
    add_executable(configured-control-psa test/test_configured_control/test_main.cpp
        lib/PsaPeerProofCrypto/PsaPeerProofCrypto.cpp)
    target_include_directories(configured-control-psa PRIVATE
        test/test_configured_control/host_stubs lib/PsaPeerProofCrypto
        lib/FreeRtosGameControlChannel lib/FreeRtosCommandFrameQueue
        lib/FreeRtosProfileFrameQueue)
    target_compile_definitions(configured-control-psa PRIVATE
        LARP_PSA_PEER_CRYPTO=1 LARP_REAL_PEER_CRYPTO=1)
    target_link_libraries(configured-control-psa PRIVATE larp_core unity tfpsacrypto)
    add_test(NAME configured-control-psa COMMAND configured-control-psa)
    set_tests_properties(configured-control-psa PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
