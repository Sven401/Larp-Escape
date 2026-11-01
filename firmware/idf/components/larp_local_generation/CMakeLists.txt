get_filename_component(LARP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(SRCS
    "${LARP_ROOT}/lib/Esp32LocalGeneration/Esp32LocalGeneration.cpp"
    "${LARP_ROOT}/lib/Esp32LocalGeneration/Esp32LocalGenerationStore.cpp"
    INCLUDE_DIRS "${LARP_ROOT}/lib/Esp32LocalGeneration"
    REQUIRES larp_core larp_boot_clock
    PRIV_REQUIRES nvs_flash esp_partition freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
