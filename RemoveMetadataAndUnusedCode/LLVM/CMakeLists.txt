if(UNIX)
  add_llvm_pass_plugin(libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin
    RemoveMetadataAndUnusedCode.cpp
      DEPENDS
      LLVMAnalysis
      LLVMTransformUtils
      intrinsics_gen
    )
else()
  # Windows/MSVC
  add_llvm_library(libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin MODULE
    RemoveMetadataAndUnusedCode.cpp PLUGIN_TOOL opt)

  add_dependencies(libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin LLVMAnalysis LLVMTransformUtils intrinsics_gen)
endif()

set_target_properties(libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
