idf_component_register(
    SRCS "../../../../lib/Esp32ProgressPartition/Esp32ProgressPartition.cpp"
    INCLUDE_DIRS "../../../../lib/Esp32ProgressPartition"
    REQUIRES larp_core esp_partition)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
