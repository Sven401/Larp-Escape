get_filename_component(LARP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(SRCS
    "${LARP_ROOT}/lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp"
    "${LARP_ROOT}/lib/FreeRtosProfileChannel/FreeRtosProfileChannel.cpp"
    INCLUDE_DIRS "${LARP_ROOT}/lib/FreeRtosGameControlChannel"
                 "${LARP_ROOT}/lib/FreeRtosProfileChannel"
    REQUIRES larp_core freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
