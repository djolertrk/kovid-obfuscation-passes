  execute_process(
    COMMAND sh -c "od -vAn -N8 -tx8 /dev/urandom | tr -d ' \n'"
    OUTPUT_VARIABLE CRYPTO_KEY
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
message(STATUS "Using build-time crypto key: ${CRYPTO_KEY}")

add_llvm_pass_plugin(KoviDRenameCodePlugin
  RenameCodePlugin.cpp
    DEPENDS
    LLVMAnalysis
    LLVMTransformUtils
    intrinsics_gen
  )

target_compile_definitions(KoviDRenameCodePlugin PRIVATE CRYPTO_KEY="${CRYPTO_KEY}")

set_target_properties(KoviDRenameCodePlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(KoviDRenameCodePlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
