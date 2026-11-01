get_filename_component(LARP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
idf_component_register(SRCS "${LARP_ROOT}/lib/PsaPeerProofCrypto/PsaPeerProofCrypto.cpp"
    "${LARP_ROOT}/lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp"
    "${LARP_ROOT}/lib/PsaD0FactSigner/PsaD0FactSigner.cpp"
    "${LARP_ROOT}/lib/PsaPackageDigest/PsaPackageDigest.cpp"
    INCLUDE_DIRS "${LARP_ROOT}/lib/PsaPeerProofCrypto"
    "${LARP_ROOT}/lib/PsaD0FactCrypto" "${LARP_ROOT}/lib/PsaPackageDigest"
    "${LARP_ROOT}/lib/PsaD0FactSigner"
    REQUIRES larp_core mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
