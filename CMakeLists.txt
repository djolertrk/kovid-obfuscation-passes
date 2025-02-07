cmake_minimum_required(VERSION 3.13.4)

project("kovid-obfustaion-passes")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_subdirectory(RenameCode)
