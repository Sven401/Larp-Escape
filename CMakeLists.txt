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
# Independent original-byte/runtime integration audits. PSA variant is mandatory
# in the existing qualified native job; Windows uses its real BCrypt backend.
function(larp_progress_audits suffix backend)
    set(catalog "rules-progress-independent-review${suffix}")
    set(trace "rules-progress-traces-review${suffix}")
    set(owner "manual-owner-review${suffix}")
    set(own "signed-rules-evidence-own${suffix}")
    set(real "signed-rules-evidence-independent${suffix}")
    set(aux "rules-bundle-aux-review${suffix}")
    set(runtime_own "rules-progress-owner-own${suffix}")
    set(runtime "rules-progress-owner-review${suffix}")
    add_executable(${catalog} test/fixtures/rules-progress-independent-review/cli.cpp)
    add_executable(${trace} test/fixtures/rules-progress-independent-review/trace.cpp)
    add_executable(${owner} test/fixtures/manual-owner-review/cli.cpp)
    add_executable(${own} test/fixtures/signed-rules-evidence-own/main.cpp)
    add_executable(${real} test/fixtures/signed-rules-evidence-independent/trace.cpp)
    add_executable(${aux} test/fixtures/rules-bundle-aux-review/main.cpp)
    add_executable(${runtime_own} test/fixtures/rules-progress-owner-own/main.cpp)
    add_executable(${runtime} test/fixtures/rules-progress-owner-review/main.cpp)
    foreach(target IN ITEMS ${catalog} ${trace} ${owner} ${own} ${real} ${aux} ${runtime_own} ${runtime})
        target_link_libraries(${target} PRIVATE larp_core)
        target_compile_options(${target} PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror)
    endforeach()
    if("${backend}" STREQUAL "PSA")
        foreach(target IN ITEMS ${catalog} ${trace} ${owner} ${own} ${real} ${aux} ${runtime_own} ${runtime})
            target_sources(${target} PRIVATE lib/PsaPackageDigest/PsaPackageDigest.cpp)
            target_include_directories(${target} PRIVATE lib/PsaPackageDigest)
            target_link_libraries(${target} PRIVATE tfpsacrypto)
        endforeach()
        foreach(target IN ITEMS ${trace} ${own} ${real} ${runtime})
            target_sources(${target} PRIVATE lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp
                lib/PsaD0FactSigner/PsaD0FactSigner.cpp)
            target_include_directories(${target} PRIVATE lib/PsaD0FactCrypto lib/PsaD0FactSigner)
        endforeach()
        target_compile_definitions(${catalog} PRIVATE RULES_PROGRESS_REVIEW_PSA=1)
        target_compile_definitions(${trace} PRIVATE RULES_PROGRESS_REVIEW_PSA=1)
        target_compile_definitions(${owner} PRIVATE MANUAL_OWNER_REVIEW_PSA=1)
        target_compile_definitions(${own} PRIVATE D0_FACT_REVIEW_PSA=1)
        target_compile_definitions(${real} PRIVATE SIGNED_RULES_REVIEW_PSA=1)
        target_compile_definitions(${aux} PRIVATE PACKAGE_MODULE_REVIEW_PSA=1)
        target_compile_definitions(${runtime_own} PRIVATE PACKAGE_MODULE_REVIEW_PSA=1)
        target_compile_definitions(${runtime} PRIVATE RULES_PROGRESS_OWNER_REVIEW_PSA=1)
    else()
        foreach(target IN ITEMS ${catalog} ${trace} ${owner} ${own} ${real} ${aux} ${runtime_own} ${runtime})
            target_link_libraries(${target} PRIVATE bcrypt)
        endforeach()
    endif()
    add_test(NAME ${catalog} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-progress-independent-review/run.py" --binary "$<TARGET_FILE:${catalog}>")
    add_test(NAME ${trace} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-progress-independent-review/run_traces.py" --binary "$<TARGET_FILE:${trace}>")
    add_test(NAME "rules-progress-malformed-review${suffix}" COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-progress-independent-review/run_malformed.py"
        --catalog-binary "$<TARGET_FILE:${catalog}>" --trace-binary "$<TARGET_FILE:${trace}>")
    add_test(NAME ${owner} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/manual-owner-review/run.py" --binary "$<TARGET_FILE:${owner}>")
    add_test(NAME ${own} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/signed-rules-evidence-own/run.py" --binary "$<TARGET_FILE:${own}>")
    add_test(NAME ${real} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/signed-rules-evidence-independent/run.py" --binary "$<TARGET_FILE:${real}>")
    add_test(NAME ${aux} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-bundle-aux-review/run.py" --binary "$<TARGET_FILE:${aux}>")
    add_test(NAME ${runtime_own} COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-progress-owner-own/run.py" --binary "$<TARGET_FILE:${runtime_own}>")
    add_test(NAME ${runtime} COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-progress-owner-review/run.py"
        --binary "$<TARGET_FILE:${runtime}>" --artifact-binary "$<TARGET_FILE:${catalog}>")
    set_tests_properties(${catalog} ${trace} "rules-progress-malformed-review${suffix}" ${owner} ${own} ${real} ${aux} ${runtime_own} ${runtime}
        PROPERTIES TIMEOUT 300)
endfunction()
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME native-journal COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/test_configured_control/check_native_journal.py"
        "$<TARGET_FILE:larp-profile>")
    add_test(NAME manual-override-v2-model COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
        -s "${CMAKE_SOURCE_DIR}/scripts/tests" -p test_p0_manual_override_v2_model.py)
    include(FetchContent)
    FetchContent_Declare(unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG cbcd08fa7de711053a3deec6339ee89cad5d2697
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(unity)
    add_executable(d0-fact test/test_d0_fact/test_main.cpp)
    target_link_libraries(d0-fact PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(d0-fact PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME d0-fact COMMAND d0-fact)
    # Negative-only backend lifecycle injection; not a signature acceptance proof.
    add_executable(d0-crypto-lifecycle test/fixtures/d0-crypto-faults/main.cpp
        lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp)
    target_include_directories(d0-crypto-lifecycle PRIVATE test/fixtures/d0-crypto-faults/stubs
        include lib/PsaD0FactCrypto)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(d0-crypto-lifecycle PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME d0-crypto-lifecycle COMMAND d0-crypto-lifecycle)
    add_executable(d0-signer-lifecycle test/fixtures/d0-signer-faults/main.cpp
        lib/PsaD0FactSigner/PsaD0FactSigner.cpp)
    target_include_directories(d0-signer-lifecycle PRIVATE test/fixtures/d0-signer-faults/stubs
        include lib/PsaD0FactSigner)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(d0-signer-lifecycle PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME d0-signer-lifecycle COMMAND d0-signer-lifecycle)
    add_executable(package-artifacts test/test_package_artifacts/test_main.cpp)
    target_link_libraries(package-artifacts PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(package-artifacts PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME package-artifacts COMMAND package-artifacts)
    if(WIN32)
        larp_progress_audits("" "WINDOWS")
        add_executable(rules-bundle-review test/fixtures/rules-bundle-review/cli.cpp
            src/DeviceRuntime/ProgressJournal.cpp src/DeviceRuntime/RulesBundleJournal.cpp
            src/DeviceRuntime/RulesProgressControl.cpp)
        target_include_directories(rules-bundle-review PRIVATE include)
        target_link_libraries(rules-bundle-review PRIVATE larp_core bcrypt)
        target_compile_options(rules-bundle-review PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME rules-bundle-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/rules-bundle-review/run.py"
            --binary "$<TARGET_FILE:rules-bundle-review>")
        set_tests_properties(rules-bundle-review PROPERTIES TIMEOUT 300)
        add_executable(manual-override-v2-review test/fixtures/manual-override-v2-review/cli.cpp)
        target_link_libraries(manual-override-v2-review PRIVATE larp_core bcrypt)
        target_compile_options(manual-override-v2-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME manual-override-v2-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/manual-override-v2-review/run.py"
            --binary "$<TARGET_FILE:manual-override-v2-review>")
        add_executable(manual-override-v2-writer-review test/fixtures/manual-override-v2-review/WriterFaults.cpp)
        target_link_libraries(manual-override-v2-writer-review PRIVATE larp_core bcrypt)
        target_compile_options(manual-override-v2-writer-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME manual-override-v2-writer-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/manual-override-v2-review/run_writer.py"
            --binary "$<TARGET_FILE:manual-override-v2-writer-review>")
        add_executable(d0-writer-review test/fixtures/d0-writer-review/cli.cpp)
        target_link_libraries(d0-writer-review PRIVATE larp_core bcrypt)
        target_compile_options(d0-writer-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME d0-writer-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/d0-writer-review/run.py"
            --binary "$<TARGET_FILE:d0-writer-review>")
        add_executable(d0-fact-review test/fixtures/d0-fact-review/cli.cpp)
        target_link_libraries(d0-fact-review PRIVATE larp_core bcrypt)
        target_compile_options(d0-fact-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME d0-fact-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/d0-fact-review/run.py"
            --binary "$<TARGET_FILE:d0-fact-review>")
        add_executable(package-artifacts-review test/fixtures/package-artifacts-review/cli.cpp)
        target_link_libraries(package-artifacts-review PRIVATE larp_core bcrypt)
        target_compile_options(package-artifacts-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME package-artifacts-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/package-artifacts-review/run.py"
            --binary "$<TARGET_FILE:package-artifacts-review>")
        add_executable(package-metadata-review test/fixtures/package-artifacts-review/metadata.cpp)
        target_link_libraries(package-metadata-review PRIVATE larp_core bcrypt)
        target_compile_options(package-metadata-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
        add_test(NAME package-metadata-review COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/test/fixtures/package-artifacts-review/run_metadata.py"
            --binary "$<TARGET_FILE:package-metadata-review>")
    endif()
    add_executable(rules-commit-ordering-review test/fixtures/rules-commit-ordering-review/main.cpp)
    target_link_libraries(rules-commit-ordering-review PRIVATE larp_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-commit-ordering-review PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-commit-ordering-review COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-commit-ordering-review/run.py"
        --binary "$<TARGET_FILE:rules-commit-ordering-review>")
    set_tests_properties(rules-commit-ordering-review PROPERTIES TIMEOUT 300)
    add_executable(rules-progress-codec-review test/fixtures/rules-progress-codec-review/main.cpp)
    target_link_libraries(rules-progress-codec-review PRIVATE larp_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-progress-codec-review PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-progress-codec-review COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-progress-codec-review/run.py"
        --binary "$<TARGET_FILE:rules-progress-codec-review>")
    add_executable(rules-effect-own test/fixtures/rules-effect-own/main.cpp)
    target_link_libraries(rules-effect-own PRIVATE larp_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-effect-own PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-effect-own COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-effect-own/run.py" --binary "$<TARGET_FILE:rules-effect-own>")
    add_executable(rules-effect-independent-review test/fixtures/rules-effect-independent-review/main.cpp)
    target_link_libraries(rules-effect-independent-review PRIVATE larp_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-effect-independent-review PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-effect-independent-review COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-effect-independent-review/run.py"
        --binary "$<TARGET_FILE:rules-effect-independent-review>")
    add_executable(rules-progress-transaction test/test_rules_progress_transaction/test_main.cpp)
    target_link_libraries(rules-progress-transaction PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-progress-transaction PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-progress-transaction COMMAND rules-progress-transaction)
    add_executable(rules-progress-control test/test_rules_progress_control/test_main.cpp)
    target_link_libraries(rules-progress-control PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(rules-progress-control PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME rules-progress-control COMMAND rules-progress-control)
    add_executable(event-authority test/test_event_authority/test_main.cpp)
    target_link_libraries(event-authority PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(event-authority PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME event-authority COMMAND event-authority)
    add_executable(manual-override-owner test/test_manual_override_owner/test_main.cpp)
    target_link_libraries(manual-override-owner PRIVATE larp_core unity)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(manual-override-owner PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME manual-override-owner COMMAND manual-override-owner)
    set_tests_properties(rules-progress-codec-review rules-effect-own rules-effect-independent-review
        rules-progress-transaction rules-progress-control event-authority manual-override-owner
        PROPERTIES TIMEOUT 300)
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
    larp_progress_audits("-psa" "PSA")
    add_executable(rules-bundle-review-psa test/fixtures/rules-bundle-review/cli.cpp
        src/DeviceRuntime/ProgressJournal.cpp src/DeviceRuntime/RulesBundleJournal.cpp
        src/DeviceRuntime/RulesProgressControl.cpp lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(rules-bundle-review-psa PRIVATE include lib/PsaPackageDigest)
    target_compile_definitions(rules-bundle-review-psa PRIVATE RULES_BUNDLE_REVIEW_PSA=1)
    target_link_libraries(rules-bundle-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(rules-bundle-review-psa PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME rules-bundle-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/rules-bundle-review/run.py"
        --binary "$<TARGET_FILE:rules-bundle-review-psa>")
    set_tests_properties(rules-bundle-review-psa PROPERTIES TIMEOUT 300)
    add_executable(manual-override-v2-review-psa test/fixtures/manual-override-v2-review/cli.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(manual-override-v2-review-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(manual-override-v2-review-psa PRIVATE MANUAL_OVERRIDE_V2_REVIEW_PSA=1)
    target_link_libraries(manual-override-v2-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(manual-override-v2-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME manual-override-v2-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/manual-override-v2-review/run.py"
        --binary "$<TARGET_FILE:manual-override-v2-review-psa>")
    add_executable(manual-override-v2-writer-review-psa test/fixtures/manual-override-v2-review/WriterFaults.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp
        lib/PsaD0FactSigner/PsaD0FactSigner.cpp)
    target_include_directories(manual-override-v2-writer-review-psa PRIVATE lib/PsaPackageDigest
        lib/PsaD0FactCrypto lib/PsaD0FactSigner)
    target_compile_definitions(manual-override-v2-writer-review-psa PRIVATE MANUAL_OVERRIDE_V2_REVIEW_PSA=1)
    target_link_libraries(manual-override-v2-writer-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(manual-override-v2-writer-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME manual-override-v2-writer-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/manual-override-v2-review/run_writer.py"
        --binary "$<TARGET_FILE:manual-override-v2-writer-review-psa>")
    add_executable(d0-writer-review-psa test/fixtures/d0-writer-review/cli.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp
        lib/PsaD0FactSigner/PsaD0FactSigner.cpp)
    target_include_directories(d0-writer-review-psa PRIVATE lib/PsaPackageDigest
        lib/PsaD0FactCrypto lib/PsaD0FactSigner)
    target_compile_definitions(d0-writer-review-psa PRIVATE D0_FACT_REVIEW_PSA=1)
    target_link_libraries(d0-writer-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(d0-writer-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME d0-writer-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/d0-writer-review/run.py"
        --binary "$<TARGET_FILE:d0-writer-review-psa>")
    add_executable(d0-fact-review-psa test/fixtures/d0-fact-review/cli.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp)
    target_include_directories(d0-fact-review-psa PRIVATE lib/PsaPackageDigest lib/PsaD0FactCrypto)
    target_compile_definitions(d0-fact-review-psa PRIVATE D0_FACT_REVIEW_PSA=1)
    target_link_libraries(d0-fact-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(d0-fact-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME d0-fact-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/d0-fact-review/run.py"
        --binary "$<TARGET_FILE:d0-fact-review-psa>")
    add_executable(package-artifacts-review-psa test/fixtures/package-artifacts-review/cli.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(package-artifacts-review-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(package-artifacts-review-psa PRIVATE PACKAGE_ARTIFACT_REVIEW_PSA=1)
    target_link_libraries(package-artifacts-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(package-artifacts-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME package-artifacts-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/package-artifacts-review/run.py"
        --binary "$<TARGET_FILE:package-artifacts-review-psa>")
    add_executable(package-metadata-review-psa test/fixtures/package-artifacts-review/metadata.cpp
        lib/PsaPackageDigest/PsaPackageDigest.cpp)
    target_include_directories(package-metadata-review-psa PRIVATE lib/PsaPackageDigest)
    target_compile_definitions(package-metadata-review-psa PRIVATE PACKAGE_ARTIFACT_REVIEW_PSA=1)
    target_link_libraries(package-metadata-review-psa PRIVATE larp_core tfpsacrypto)
    target_compile_options(package-metadata-review-psa PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME package-metadata-review-psa COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/test/fixtures/package-artifacts-review/run_metadata.py"
        --binary "$<TARGET_FILE:package-metadata-review-psa>")
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
