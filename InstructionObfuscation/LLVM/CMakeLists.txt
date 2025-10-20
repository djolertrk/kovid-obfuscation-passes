if(UNIX)
  add_llvm_pass_plugin(libKoviDInstructionObfuscationPassLLVMPlugin
    InstructionObfuscation.cpp
      DEPENDS
      LLVMAnalysis
      LLVMTransformUtils
      intrinsics_gen
    )
else()
  # Windows/MSVC
  add_llvm_library(libKoviDInstructionObfuscationPassLLVMPlugin MODULE
    InstructionObfuscation.cpp PLUGIN_TOOL opt)

  add_dependencies(libKoviDInstructionObfuscationPassLLVMPlugin LLVMAnalysis LLVMTransformUtils intrinsics_gen)
endif()

set_target_properties(libKoviDInstructionObfuscationPassLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDInstructionObfuscationPassLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
