cmake_minimum_required(VERSION 3.13.4)

project("kovid-obfustaion-passes")

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_subdirectory(RenameCode)
add_subdirectory(DummyCodeInsertion)
add_subdirectory(RemoveMetadataAndUnusedCode)
add_subdirectory(InstructionObfuscation)
add_subdirectory(StringEncryption)
add_subdirectory(BreakCFG)
add_subdirectory(CFFlattening)
add_subdirectory(tools)

install(FILES ${CMAKE_BINARY_DIR}/lib/libKoviDRenameCodeGCCPlugin.so DESTINATION /usr/local/lib/)
