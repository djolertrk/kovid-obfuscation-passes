set(LLVM_LINK_COMPONENTS
  Support
  )

add_llvm_tool(kovid-deobfuscator
  kovid-deobfuscator.cpp
  )

set_target_properties(kovid-deobfuscator PROPERTIES EXCLUDE_FROM_ALL OFF)
set_target_properties(kovid-deobfuscator
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
