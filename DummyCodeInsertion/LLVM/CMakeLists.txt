add_llvm_pass_plugin(libKoviDDummyCodeInsertionLLVMPlugin
  DummyCodeInsertion.cpp
    DEPENDS
    LLVMAnalysis
    LLVMTransformUtils
    intrinsics_gen
  )

set_target_properties(libKoviDDummyCodeInsertionLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDDummyCodeInsertionLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
