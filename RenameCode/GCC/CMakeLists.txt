# Generate a crypto key for the GCC plugin using /dev/urandom.
execute_process(
  COMMAND sh -c "od -vAn -N8 -tx8 /dev/urandom | tr -d ' \n'"
  OUTPUT_VARIABLE GCC_CRYPTO_KEY
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Using build-time crypto key for GCC Plugin: ${GCC_CRYPTO_KEY}")

# We'll explicitly use "make" for the external build.
set(EXT_MAKE make)

# Create a custom target that builds the plugin via the external Makefile.
add_custom_target(build_plugin ALL
  COMMAND ${EXT_MAKE} CRYPTO_KEY=${GCC_CRYPTO_KEY}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Building GCC plugin using external Makefile"
)

# Define the destination directory for the built shared library.
set(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_DIRECTORY})

# After the build_plugin target completes, copy the resulting library.
add_custom_command(TARGET build_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/libKoviDRenameCodeGCCPlugin.so ${LIBRARY_OUTPUT_DIRECTORY}
  COMMENT "Copying libKoviDRenameCodeGCCPlugin.so to ${LIBRARY_OUTPUT_DIRECTORY}"
)

# After the build_plugin target completes, copy the resulting library
# and then run "make clean" in the external directory.
add_custom_command(TARGET build_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/libKoviDRenameCodeGCCPlugin.so ${LIBRARY_OUTPUT_DIRECTORY}
  COMMAND ${EXT_MAKE} clean
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Copying libKoviDRenameCodeGCCPlugin.so to ${LIBRARY_OUTPUT_DIRECTORY} and cleaning external build"
)

install(
  FILES "${LIBRARY_OUTPUT_DIRECTORY}/libKoviDRenameCodeGCCPlugin.so"
  DESTINATION lib
)
