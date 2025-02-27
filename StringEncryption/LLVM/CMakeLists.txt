execute_process(
    COMMAND sh -c "od -vAn -N8 -tx8 /dev/urandom | tr -d ' \n'"
    OUTPUT_VARIABLE SE_LLVM_CRYPTO_KEY
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

message(STATUS "Using build-time crypto key for StringEncryption LLVM Plugin: ${SE_LLVM_CRYPTO_KEY}")

add_llvm_pass_plugin(libKoviDStringEncryptionLLVMPlugin
  StringEncryption.cpp
    DEPENDS
    LLVMAnalysis
    LLVMTransformUtils
    intrinsics_gen
  )

target_compile_definitions(libKoviDStringEncryptionLLVMPlugin PRIVATE SE_LLVM_CRYPTO_KEY="${SE_LLVM_CRYPTO_KEY}")

set_target_properties(libKoviDStringEncryptionLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDStringEncryptionLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (KOP_BUILD_LLDB_PLUGINS)
  add_subdirectory(LLDB)
endif()
