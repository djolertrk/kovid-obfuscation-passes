if(UNIX)
  add_llvm_pass_plugin(libKoviDControlFlowTaintLLVMPlugin
    ControlFlowTaint.cpp
      DEPENDS
      LLVMAnalysis
      LLVMTransformUtils
      intrinsics_gen
    )
else()
  # Windows/MSVC
  add_llvm_library(libKoviDControlFlowTaintLLVMPlugin MODULE
    ControlFlowTaint.cpp PLUGIN_TOOL opt)

  add_dependencies(libKoviDControlFlowTaintLLVMPlugin LLVMAnalysis LLVMTransformUtils intrinsics_gen)
endif()

set_target_properties(libKoviDControlFlowTaintLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDControlFlowTaintLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
