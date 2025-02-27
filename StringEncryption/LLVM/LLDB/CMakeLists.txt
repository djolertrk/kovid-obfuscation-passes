set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Check for LLDB
find_library(LLDB_LIBRARY REQUIRED
    NAMES lldb LLDB
    HINTS /usr/lib/llvm-19/lib/
    PATHS /usr/lib/llvm-19/lib/
)

include_directories(/usr/lib/llvm-19/include/)

add_library(KoviDStringEncryptionDeobfuscateLLDB SHARED StringEncryptionDeobfuscate.cpp)

target_compile_definitions(KoviDStringEncryptionDeobfuscateLLDB PRIVATE SE_LLVM_CRYPTO_KEY="${SE_LLVM_CRYPTO_KEY}")

message(STATUS "Using build-time crypto key for StringEncryption LLDB Plugin: ${SE_LLVM_CRYPTO_KEY}")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set_target_properties(KoviDStringEncryptionDeobfuscateLLDB PROPERTIES
    INSTALL_RPATH "@loader_path"
    MACOSX_RPATH ON
    LINK_FLAGS "-Wl,-export_dynamic"
)
endif()

set_target_properties(KoviDStringEncryptionDeobfuscateLLDB PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS KoviDStringEncryptionDeobfuscateLLDB
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
