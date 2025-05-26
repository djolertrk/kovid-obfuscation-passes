option(CICD_MACHINE "Set to ON when building in a CI/CD environment" OFF)
message(STATUS "CICD_MACHINE is ${CICD_MACHINE}")

if(CICD_MACHINE)
  find_package(LLVM 18.1.8 REQUIRED CONFIG)
else()
  find_package(LLVM REQUIRED CONFIG)
endif()

message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using LLVM_VERSION_MAJOR: ${LLVM_VERSION_MAJOR}")

include(AddLLVM)

# Find the paths to FileCheck and not
find_program(FILECHECK_EXE NAMES FileCheck FileCheck-${LLVM_VERSION_MAJOR})
find_program(NOT_EXE NAMES not not-${LLVM_VERSION_MAJOR})

if(NOT FILECHECK_EXE)
    message(FATAL_ERROR "FileCheck executable not found!")
endif()

if(NOT NOT_EXE)
    message(FATAL_ERROR "'not' executable not found!")
endif()

message(STATUS "Using FileCheck: ${FILECHECK_EXE}")
message(STATUS "Using not: ${NOT_EXE}")

# Pass the paths to lit via configure_file
set(FILECHECK_PATH "${FILECHECK_EXE}")
set(NOT_PATH "${NOT_EXE}")
set(DEPLOY "0")
set(CROSS_TESTS "0")
set(NATIVE_TESTS "1")

# LLVM version information
set(LLVM_VERSION_MAJOR "${LLVM_VERSION_MAJOR}")
set(LLVM_VERSION_MINOR "${LLVM_VERSION_MINOR}")
set(TARGET_TRIPLE "${LLVM_HOST_TRIPLE}")

# Use configure_file directly instead of configure_lit_site_cfg for better control
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  @ONLY
)

# Find llvm-lit
find_program(LLVM_LIT NAMES llvm-lit lit llvm-lit-${LLVM_VERSION_MAJOR})

if(NOT LLVM_LIT)
    message(FATAL_ERROR "llvm-lit not found. Please install it or specify its location.")
else()
    message(STATUS "Found llvm-lit: ${LLVM_LIT}")
endif()

# Check and set LLVM_EXTERNAL_LIT
if(DEFINED LLVM_EXTERNAL_LIT AND EXISTS ${LLVM_EXTERNAL_LIT})
    message(STATUS "Using LLVM_EXTERNAL_LIT: ${LLVM_EXTERNAL_LIT}")
else()
    message(WARNING "LLVM_EXTERNAL_LIT not set or invalid, using LLVM_LIT instead")
    set(LLVM_EXTERNAL_LIT ${LLVM_LIT} CACHE FILEPATH "Path to llvm-lit" FORCE)
endif()

# Define dependencies for tests - collect only targets that exist
set(KOVID_TEST_DEPENDS)

# Check if targets exist before adding them as dependencies
foreach(target
    libKoviDRenameCodeLLVMPlugin
    libKoviDDummyCodeInsertionLLVMPlugin
    libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin
    libKoviDStringEncryptionLLVMPlugin
    libKoviDInstructionObfuscationPassLLVMPlugin
    libKoviDBreakCFGLLVMPlugin
    libKoviDCFFlatteningLLVMPlugin)
  if(TARGET ${target})
    list(APPEND KOVID_TEST_DEPENDS ${target})
    message(STATUS "Added test dependency: ${target}")
  else()
    message(STATUS "Skipping test dependency (target not found): ${target}")
  endif()
endforeach()

# Native tests only with dependencies if found
add_lit_testsuite(check-passes "Running the KoviD obfuscation passes tests"
    ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${KOVID_TEST_DEPENDS}
    ARGS --threads=1
)

set_target_properties(check-passes PROPERTIES FOLDER "Tests")

# Add the main lit testsuites
add_lit_testsuites(KOVID ${CMAKE_CURRENT_SOURCE_DIR} DEPENDS ${KOVID_TEST_DEPENDS})

# Add info about the targets
message(STATUS "Added test targets: check-passes")
message(STATUS "You can run tests with: make check-passes")
