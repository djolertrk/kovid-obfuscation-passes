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

# Automatically disable GCC plugins on Windows and macOS
if(WIN32 OR APPLE)
  set(KOP_BUILD_GCC_PLUGINS_DEFAULT OFF)
else()
  set(KOP_BUILD_GCC_PLUGINS_DEFAULT ON)
endif()

option(KOP_BUILD_GCC_PLUGINS "Build GCC plugins" ${KOP_BUILD_GCC_PLUGINS_DEFAULT})
if(WIN32 AND KOP_BUILD_GCC_PLUGINS)
  message(WARNING "GCC plugins are not supported on Windows. Disabling KOP_BUILD_GCC_PLUGINS.")
  set(KOP_BUILD_GCC_PLUGINS OFF CACHE BOOL "Build GCC plugins" FORCE)
endif()
if(APPLE AND KOP_BUILD_GCC_PLUGINS)
  message(WARNING "GCC plugins are not recommended on macOS. Disabling KOP_BUILD_GCC_PLUGINS.")
  set(KOP_BUILD_GCC_PLUGINS OFF CACHE BOOL "Build GCC plugins" FORCE)
endif()

add_subdirectory(RenameCode)
add_subdirectory(DummyCodeInsertion)
add_subdirectory(RemoveMetadataAndUnusedCode)
add_subdirectory(InstructionObfuscation)
add_subdirectory(StringEncryption)
add_subdirectory(ControlFlowTaint)
add_subdirectory(tools)

option(ENABLE_TESTS "Enable Tests" OFF)
if(ENABLE_TESTS)
  add_subdirectory(test)
endif()

# Only install GCC plugin on Unix systems
if(UNIX AND KOP_BUILD_GCC_PLUGINS)
  install(FILES ${CMAKE_BINARY_DIR}/lib/libKoviDRenameCodeGCCPlugin.so DESTINATION /usr/local/lib/)
endif()
