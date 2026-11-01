get_filename_component(LARP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(SRCS
    "${LARP_ROOT}/lib/Esp32RulesBootClock/Esp32RulesBootClock.cpp"
    "${LARP_ROOT}/lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp"
    INCLUDE_DIRS "${LARP_ROOT}/lib/Esp32RulesBootClock"
    PRIV_INCLUDE_DIRS "${LARP_ROOT}/lib/Esp32DeviceIdentityStore"
    REQUIRES larp_core
    PRIV_REQUIRES bootloader_support esp_hw_support esp_timer)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
