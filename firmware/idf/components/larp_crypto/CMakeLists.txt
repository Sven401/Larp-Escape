get_filename_component(LARP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(SRCS "${LARP_ROOT}/lib/PsaPeerProofCrypto/PsaPeerProofCrypto.cpp"
    INCLUDE_DIRS "${LARP_ROOT}/lib/PsaPeerProofCrypto"
    REQUIRES larp_core mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
