  execute_process(
    COMMAND sh -c "od -vAn -N8 -tx8 /dev/urandom | tr -d ' \n'"
    OUTPUT_VARIABLE LLVM_CRYPTO_KEY
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
message(STATUS "Using build-time crypto key for LLVM Plugin: ${LLVM_CRYPTO_KEY}")

add_llvm_pass_plugin(libKoviDRenameCodeLLVMPlugin
  RenameCodePlugin.cpp
    DEPENDS
    LLVMAnalysis
    LLVMTransformUtils
    intrinsics_gen
  )

target_compile_definitions(libKoviDRenameCodeLLVMPlugin PRIVATE CRYPTO_KEY="${LLVM_CRYPTO_KEY}")

set_target_properties(libKoviDRenameCodeLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDRenameCodeLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
