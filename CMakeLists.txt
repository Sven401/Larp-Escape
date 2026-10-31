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
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME native-journal COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/test_configured_control/check_native_journal.py"
        "$<TARGET_FILE:larp-profile>")
    include(FetchContent)
    FetchContent_Declare(unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG cbcd08fa7de711053a3deec6339ee89cad5d2697
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(unity)
    add_executable(rules-planner test/test_rules_planner/test_main.cpp)
    target_link_libraries(rules-planner PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-planner PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-planner COMMAND rules-planner)
    if(WIN32)
        add_executable(rules-planner-review test/fixtures/rules-planner-review/cli.cpp)
        target_link_libraries(rules-planner-review PRIVATE larp_core bcrypt)
        target_compile_options(rules-planner-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_executable(rules-planner-budget test/fixtures/rules-planner-review/budget.cpp)
        target_link_libraries(rules-planner-budget PRIVATE larp_core bcrypt)
        target_compile_options(rules-planner-budget PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME rules-planner-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/rules-planner-review/run.py"
            --binary "$<TARGET_FILE:rules-planner-review>"
            --budget-binary "$<TARGET_FILE:rules-planner-budget>")
    endif()
    add_executable(package-modules test/test_package_modules/test_main.cpp)
    target_link_libraries(package-modules PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(package-modules PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME package-modules COMMAND package-modules)
    add_executable(progress-journal test/test_progress_journal/test_main.cpp)
    target_link_libraries(progress-journal PRIVATE larp_core unity)
    add_test(NAME progress-journal COMMAND progress-journal)
    add_executable(progress-partition test/test_progress_partition/main.cpp
        lib/Esp32ProgressPartition/Esp32ProgressPartition.cpp)
    target_include_directories(progress-partition PRIVATE
        test/test_progress_partition/stubs lib/Esp32ProgressPartition)
    target_link_libraries(progress-partition PRIVATE larp_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(progress-journal PRIVATE -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(progress-partition PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME progress-partition COMMAND progress-partition)
    add_executable(event-transport test/test_event_transport/test_main.cpp)
    target_link_libraries(event-transport PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(event-transport PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME event-transport COMMAND event-transport)
    add_executable(package-rules test/test_package_rules/test_main.cpp)
    target_link_libraries(package-rules PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(package-rules PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME package-rules COMMAND package-rules)
    add_executable(package-declarations test/test_package_declarations/test_main.cpp)
    target_link_libraries(package-declarations PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(package-declarations PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME package-declarations COMMAND package-declarations)
    add_executable(package-json test/test_package_json/test_main.cpp)
    target_link_libraries(package-json PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(package-json PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME package-json COMMAND package-json)
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
    add_subdirectory("${LARP_PSA_IDF_PATH}/components/mbedtls/mbedtls"
                     "${CMAKE_BINARY_DIR}/mbedtls")
    # Espressif's sources retain two legacy include paths supplied by its
    # ESP-only port. Host tests forward them to the identical private headers.
    target_include_directories(builtin PRIVATE "${CMAKE_SOURCE_DIR}/test/psa_host_compat")
    target_include_directories(tfpsacrypto PRIVATE "${CMAKE_SOURCE_DIR}/test/psa_host_compat")
    add_executable(rules-planner-review-psa test/fixtures/rules-planner-review/cli.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(rules-planner-review-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(rules-planner-review-psa PRIVATE RULES_PLANNER_REVIEW_PSA=1)
    target_link_libraries(rules-planner-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(rules-planner-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_executable(rules-planner-budget-psa test/fixtures/rules-planner-review/budget.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(rules-planner-budget-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(rules-planner-budget-psa PRIVATE RULES_PLANNER_REVIEW_PSA=1)
    target_link_libraries(rules-planner-budget-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(rules-planner-budget-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME rules-planner-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-planner-review/run.py"
        --binary "$<TARGET_FILE:rules-planner-review-psa>"
        --budget-binary "$<TARGET_FILE:rules-planner-budget-psa>")
    add_executable(package-modules-psa test/test_package_modules/test_main.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(package-modules-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(package-modules-psa PRIVATE LARP_REAL_PACKAGE_PSA=1)
    target_link_libraries(package-modules-psa PRIVATE larp_core unity tfpsacrypto)
    target_compile_options(package-modules-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME package-modules-psa COMMAND package-modules-psa)
    add_executable(package-modules-review-psa test/fixtures/package-modules-review/cli.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(package-modules-review-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(package-modules-review-psa PRIVATE PACKAGE_MODULE_REVIEW_PSA=1)
    target_link_libraries(package-modules-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(package-modules-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME package-modules-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/package-modules-review/run.py"
        --binary "$<TARGET_FILE:package-modules-review-psa>")
    add_executable(native-empty-ecc test/test_native_tls/test_empty_ecc.cpp)
    target_link_libraries(native-empty-ecc PRIVATE tfpsacrypto)
    add_test(NAME native-empty-ecc COMMAND native-empty-ecc)
    add_library(larp_peer_tls STATIC lib/PinnedPeerTls/PinnedPeerTls.cpp)
    target_include_directories(larp_peer_tls PUBLIC lib/PinnedPeerTls)
    target_link_libraries(larp_peer_tls PUBLIC mbedtls mbedx509 tfpsacrypto)
    target_compile_options(larp_peer_tls PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_executable(native-ble-source test/test_native_ble_source/main.cpp
        lib/NimblePeerSource/NimblePeerSource.cpp)
    target_include_directories(native-ble-source PRIVATE test/test_native_ble_source/stubs
        lib/NimblePeerSource)
    target_compile_definitions(native-ble-source PRIVATE CONFIG_BT_NIMBLE_ENABLED=1
        CONFIG_BT_NIMBLE_SM_SC_ONLY=1 CONFIG_BT_NIMBLE_SM_LEGACY=0
        CONFIG_BT_NIMBLE_SM_SC_DEBUG_KEYS=0 CONFIG_BT_NIMBLE_NVS_PERSIST=0)
    target_compile_options(native-ble-source PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(native-ble-source PRIVATE larp_peer_tls)
    foreach(case registration-failure timer-failure host-return weak-security short-key
        numeric-comparison copy-and-notify default-mtu reconnect second-link encryption-error
        queue-full wrong-task invalid-write notify-busy reset worker-revoke
        advertising-failure security-start-failure open-failure unencrypted unbonded
        numeric-wrong numeric-reject numeric-timeout
        admission-timeout admission-before admission-wrap admission-no-subscribe
        admission-repeated-subscribe admission-late-comparison admission-reconnect
        admission-callback-boundary admission-late-passkey admission-descriptor-boundary
        admission-open-boundary admission-open-return-boundary admission-open-revoke
        heartbeat-current-race heartbeat-ready-race lost-poll runtime-timer-failure notify-sdk-busy notify-sdk-fatal)
        add_test(NAME "native-ble-${case}" COMMAND native-ble-source "${case}")
    endforeach()
    add_executable(native-tls-probe test/test_native_tls/server.cpp)
    target_link_libraries(native-tls-probe PRIVATE larp_peer_tls)
    add_test(NAME native-tls-peer COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/test_native_tls/run.py" "$<TARGET_FILE:native-tls-probe>")
    set_tests_properties(native-tls-peer PROPERTIES TIMEOUT 120)
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
