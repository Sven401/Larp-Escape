include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/LarpCoreSources.cmake)
idf_component_register(SRCS ${LARP_CORE_SOURCES}
    INCLUDE_DIRS "${LARP_ROOT}/include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
