if(UNIX)
  execute_process(
    COMMAND sh -c "od -vAn -N8 -tx8 /dev/urandom | tr -d ' \n'"
    OUTPUT_VARIABLE LLVM_CRYPTO_KEY
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  # Windows: Generate a random hex key using PowerShell
  execute_process(
    COMMAND powershell -Command "[System.BitConverter]::ToString([System.Guid]::NewGuid().ToByteArray()).Replace('-','')"
    OUTPUT_VARIABLE LLVM_CRYPTO_KEY
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()
message(STATUS "Using build-time crypto key for RenameCode LLVM Plugin: ${LLVM_CRYPTO_KEY}")

if(UNIX)
  add_llvm_pass_plugin(libKoviDRenameCodeLLVMPlugin
    RenameCodePlugin.cpp
      DEPENDS
      LLVMAnalysis
      LLVMTransformUtils
      intrinsics_gen
    )
else()
  # Windows/MSVC
  add_llvm_library(libKoviDRenameCodeLLVMPlugin MODULE
    RenameCodePlugin.cpp PLUGIN_TOOL opt)

  add_dependencies(libKoviDRenameCodeLLVMPlugin LLVMAnalysis LLVMTransformUtils intrinsics_gen)
endif()

target_compile_definitions(libKoviDRenameCodeLLVMPlugin PRIVATE CRYPTO_KEY="${LLVM_CRYPTO_KEY}")

set_target_properties(libKoviDRenameCodeLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDRenameCodeLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
