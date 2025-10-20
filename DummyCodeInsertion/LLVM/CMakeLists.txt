if(UNIX)
  add_llvm_pass_plugin(libKoviDDummyCodeInsertionLLVMPlugin
    DummyCodeInsertion.cpp
      DEPENDS
      LLVMAnalysis
      LLVMTransformUtils
      intrinsics_gen
    )
else()
  # Windows/MSVC
  add_llvm_library(libKoviDDummyCodeInsertionLLVMPlugin MODULE
    DummyCodeInsertion.cpp PLUGIN_TOOL opt)

  add_dependencies(libKoviDDummyCodeInsertionLLVMPlugin LLVMAnalysis LLVMTransformUtils intrinsics_gen)
endif()

set_target_properties(libKoviDDummyCodeInsertionLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDDummyCodeInsertionLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
