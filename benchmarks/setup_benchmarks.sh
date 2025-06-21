#!/bin/bash

# Benchmark setup script for KoviD Obfuscation Passes
# This script sets up the LLVM test suite and prepares benchmarks

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LLVM_VERSION="19"
COMPILER_TYPE="clang" # Default to clang (can be "gcc" or "clang")
COMPILER_PATH="" # Custom compiler path

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --compiler=*)
            COMPILER_TYPE="${1#*=}"
            shift
            ;;
        --path=*)
            COMPILER_PATH="${1#*=}"
            shift
            ;;
        --help)
            echo "Usage: $0 [--compiler=clang|gcc] [--path=/path/to/compiler]"
            echo "  --compiler=TYPE    Specify compiler type (clang or gcc)"
            echo "  --path=PATH        Specify custom compiler path"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=== KoviD Obfuscation Passes Benchmark Setup ==="
echo "Project root: $PROJECT_ROOT"

# Set compiler paths based on specified type and path
if [ -n "$COMPILER_PATH" ]; then
    # Use explicitly provided compiler path
    if [ ! -x "$COMPILER_PATH" ]; then
        echo "Error: Specified compiler at $COMPILER_PATH is not executable."
        exit 1
    fi
    
    if [ "$COMPILER_TYPE" = "clang" ]; then
        export CC="$COMPILER_PATH"
        # Derive C++ compiler path from C compiler path
        export CXX="${COMPILER_PATH}++"
        if [ ! -x "$CXX" ]; then
            # Try with "-cpp" suffix if "++" doesn't exist
            CXX="${COMPILER_PATH/clang/clang++}"
            if [ ! -x "$CXX" ]; then
                echo "Warning: Could not find C++ compiler at $CXX. Using C compiler for both."
                export CXX="$COMPILER_PATH"
            fi
        fi
    elif [ "$COMPILER_TYPE" = "gcc" ]; then
        export CC="$COMPILER_PATH"
        # Derive C++ compiler path from C compiler path
        export CXX="${COMPILER_PATH/gcc/g++}"
        if [ ! -x "$CXX" ]; then
            echo "Warning: Could not find C++ compiler at $CXX. Using C compiler for both."
            export CXX="$COMPILER_PATH"
        fi
    else
        echo "Error: Unknown compiler type: $COMPILER_TYPE. Use 'clang' or 'gcc'."
        exit 1
    fi
else
    # No custom path provided, check for compiler in PATH
    if [ "$COMPILER_TYPE" = "clang" ]; then
        # Check for version-specific clang first, then fall back to generic clang
        if command -v clang-${LLVM_VERSION} &> /dev/null; then
            export CC=clang-${LLVM_VERSION}
            export CXX=clang++-${LLVM_VERSION}
        elif command -v clang &> /dev/null; then
            export CC=clang
            export CXX=clang++
        else
            echo "Error: clang not found. Please install LLVM ${LLVM_VERSION} or specify path with --path."
            exit 1
        fi
    elif [ "$COMPILER_TYPE" = "gcc" ]; then
        if command -v gcc &> /dev/null; then
            export CC=gcc
            export CXX=g++
        else
            echo "Error: gcc not found. Please install GCC or specify path with --path."
            exit 1
        fi
    else
        echo "Error: Unknown compiler type: $COMPILER_TYPE. Use 'clang' or 'gcc'."
        exit 1
    fi
fi

echo "Using compiler: $CC ($COMPILER_TYPE)"
echo "Using C++ compiler: $CXX"

# Create benchmark directory
BENCHMARK_DIR="$SCRIPT_DIR/llvm-test-suite"
mkdir -p "$BENCHMARK_DIR"

# Clone LLVM test suite if not already present
if [ ! -d "$BENCHMARK_DIR/.git" ]; then
    echo "=== Cloning LLVM test suite ==="
    git clone https://github.com/llvm/llvm-test-suite.git "$BENCHMARK_DIR"
else
    echo "=== Updating LLVM test suite ==="
    cd "$BENCHMARK_DIR"
    git pull
fi

# Create build directories
BUILD_BASE="$SCRIPT_DIR/builds"
mkdir -p "$BUILD_BASE"

# Determine correct file extension for shared libraries
if [[ "$OSTYPE" == "darwin"* ]]; then
    LIB_EXT="dylib"
else
    LIB_EXT="so"
fi

# List of obfuscation passes to test based on compiler type
if [ "$COMPILER_TYPE" = "clang" ]; then
    declare -a PASSES=(
        "baseline:none"
        "rename:$PROJECT_ROOT/build/lib/libKoviDRenameCodeLLVMPlugin.$LIB_EXT"
        "dummy:$PROJECT_ROOT/build/lib/libKoviDDummyCodeInsertionLLVMPlugin.$LIB_EXT"
        "instruction:$PROJECT_ROOT/build/lib/libKoviDInstructionObfuscationPassLLVMPlugin.$LIB_EXT"
        "cft:$PROJECT_ROOT/build/lib/libKoviDControlFlowTaintLLVMPlugin.$LIB_EXT"
        "metadata:$PROJECT_ROOT/build/lib/libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin.$LIB_EXT"
        "string:$PROJECT_ROOT/build/lib/libKoviDStringEncryptionLLVMPlugin.$LIB_EXT"
    )
elif [ "$COMPILER_TYPE" = "gcc" ]; then
    declare -a PASSES=(
        "baseline:none"
        "rename:$PROJECT_ROOT/build/lib/libKoviDRenameCodeGCCPlugin.$LIB_EXT"
        "dummy:$PROJECT_ROOT/build/lib/libKoviDDummyCodeInsertionGCCPlugin.$LIB_EXT"
        "instruction:$PROJECT_ROOT/build/lib/libKoviDInstructionObfuscationGCCPlugin.$LIB_EXT"
        "cft:$PROJECT_ROOT/build/lib/libKoviDControlFlowTaintGCCPlugin.$LIB_EXT"
        "metadata:$PROJECT_ROOT/build/lib/libKoviDRemoveMetadataAndUnusedCodeGCCPlugin.$LIB_EXT"
        "string:$PROJECT_ROOT/build/lib/libKoviDStringEncryptionGCCPlugin.$LIB_EXT"
    )
fi

# Function to attempt different plugin configurations for problematic plugins
try_plugin_configurations() {
    local pass_name=$1
    local pass_plugin=$2
    local build_dir=$3
    
    echo "Attempting alternative configurations for problematic plugin: $pass_name"
    
    # Customize options based on specific plugin
    local plugin_options=()
    
    case "$pass_name" in
        rename)
            # RenameCode specifically needs -fno-inline to work properly
            plugin_options=(
                "-fplugin=$pass_plugin -fno-inline"
                "-fplugin=$pass_plugin -fno-inline -fno-strict-aliasing"
                "-fplugin=$pass_plugin -fno-inline -O0"
            )
            ;;
        metadata|cft)
            # Other problematic plugins - try different options but without forcing -fno-inline
            plugin_options=(
                "-fplugin=$pass_plugin"
                "-fplugin=$pass_plugin -fno-strict-aliasing"
                "-fplugin=$pass_plugin -O0"
            )
            ;;
        *)
            # Not a problematic plugin, but provide a default option anyway
            plugin_options=(
                "-fplugin=$pass_plugin"
            )
            ;;
    esac
    
    if [ ${#plugin_options[@]} -gt 0 ]; then
        
        for option in "${plugin_options[@]}"; do
            echo "Trying configuration with options: $option"
            
            # Clean build directory between attempts
            rm -rf "$build_dir"
            mkdir -p "$build_dir"
            cd "$build_dir"
            
            if cmake "$BENCHMARK_DIR" \
                -DCMAKE_C_COMPILER=$CC \
                -DCMAKE_CXX_COMPILER=$CXX \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_C_FLAGS="$option" \
                -DCMAKE_CXX_FLAGS="$option" \
                -DCMAKE_SIZEOF_VOID_P="$VOID_PTR_SIZE" \
                -DTEST_SUITE_BENCHMARKING_ONLY=ON \
                -DTEST_SUITE_SUBDIRS="CTMark;MicroBenchmarks" \
                -DTEST_SUITE_RUN_BENCHMARKS=OFF \
                -DLLVM_TARGETS_TO_BUILD="$TARGET_ARCH" \
                -DLLVM_DEFAULT_TARGET_TRIPLE="$ARCH-unknown-linux-gnu" \
                -DTEST_SUITE_ARCH="$TEST_SUITE_ARCH" \
                -DARCH="x86" \
                -DCMAKE_MODULE_PATH="/tmp/cmake_defines_$$"; then
                
                echo "Success! Configuration worked with options: $option"
                return 0
            else
                echo "Failed with options: $option"
            fi
        done
        
        echo "All attempted configurations failed for $pass_name"
        return 1
    fi
    
    return 1
}

# Create CMake configuration for each pass
for pass_config in "${PASSES[@]}"; do
    IFS=':' read -r pass_name pass_plugin <<< "$pass_config"
    BUILD_DIR="$BUILD_BASE/build-$pass_name"
    
    # Check if the plugin file exists and is readable (for non-baseline passes)
    if [ "$pass_name" != "baseline" ]; then
        if [ ! -r "$pass_plugin" ]; then
            echo "Warning: Plugin file not found or not readable: $pass_plugin"
            echo "Did you build the plugins for $COMPILER_TYPE?"
            echo "Skipping $pass_name configuration"
            continue
        fi
        
        # Check plugin file type and verify it's a valid shared library
        echo "Verifying plugin: $pass_plugin"
        file "$pass_plugin"
        ldd "$pass_plugin" || echo "Warning: Unable to check plugin dependencies"
    fi
    
    echo "=== Configuring build for $pass_name ==="
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Create a test program to determine void pointer size (for both compilers)
    echo "#include <stdio.h>
int main() {
    printf(\"%zu\", sizeof(void*));
    return 0;
}" > /tmp/sizeof_void_ptr_$$.c
    
    # Compile and run the test program
    $CC -o /tmp/sizeof_void_ptr_$$ /tmp/sizeof_void_ptr_$$.c
    VOID_PTR_SIZE=$(/tmp/sizeof_void_ptr_$$)
    rm -f /tmp/sizeof_void_ptr_$$ /tmp/sizeof_void_ptr_$$.c
    
    echo "Detected sizeof(void*) = $VOID_PTR_SIZE"
    
    # Detect architecture
    ARCH=$(uname -m)
    if [ "$ARCH" = "x86_64" ]; then
        TARGET_ARCH="X86"
        TEST_SUITE_ARCH="x86_64"
    elif [[ "$ARCH" =~ ^(aarch64|arm64)$ ]]; then
        TARGET_ARCH="AArch64"
        TEST_SUITE_ARCH="AArch64"
    elif [[ "$ARCH" =~ ^arm ]]; then
        TARGET_ARCH="ARM"
        TEST_SUITE_ARCH="ARM"
    elif [ "$ARCH" = "ppc64le" ]; then
        TARGET_ARCH="PowerPC"
        TEST_SUITE_ARCH="PowerPC"
    else
        TARGET_ARCH="$ARCH"
        TEST_SUITE_ARCH="$ARCH"
    fi
    
    echo "Detected architecture: $ARCH (using $TARGET_ARCH for LLVM test-suite)"
    
    # Create a direct replacement for DetectArchitecture.cmake
    mkdir -p /tmp/cmake_defines_$$/cmake/modules
    
    # Replace the architecture detection module
    cat > /tmp/cmake_defines_$$/cmake/modules/DetectArchitecture.cmake << EOF
# Direct replacement for DetectArchitecture.cmake to force architecture detection
function(detect_architecture variable)
  set(DETECT_ARCH "x86")
  message(STATUS "Check target system architecture: \${DETECT_ARCH} [forced]")
  set(\${variable} \${DETECT_ARCH} PARENT_SCOPE)
endfunction(detect_architecture)

function(detect_x86_cpu_architecture variable)
  set(DETECT_ARCH "x86_64")
  message(STATUS "Check target system cpu architecture: \${DETECT_ARCH} [forced]")
  set(\${variable} \${DETECT_ARCH} PARENT_SCOPE)
endfunction(detect_x86_cpu_architecture)
EOF

    # Create a custom DetectArchitecture.c that will always work
    cat > /tmp/cmake_defines_$$/cmake/modules/DetectArchitecture.c << EOF
/* Simplified architecture detection file */
const char *str = "ARCHITECTURE IS x86";

int main(int argc, char **argv) {
    return 0;
}
EOF

    # Create a custom DetectX86CPUArchitecture.c
    cat > /tmp/cmake_defines_$$/cmake/modules/DetectX86CPUArchitecture.c << EOF
/* Simplified CPU architecture detection file */
#include <stdio.h>

int main(int argc, char **argv) {
    printf("x86_64");
    return 0;
}
EOF

    # Also create a simple ArchDefines.cmake for additional safety
    cat > /tmp/cmake_defines_$$/ArchDefines.cmake << EOF
# Architecture detection
set(TEST_SUITE_ARCH "${TEST_SUITE_ARCH}" CACHE STRING "Test suite architecture")
set(ARCH "x86" CACHE STRING "Target architecture" FORCE)
set(CMAKE_SIZEOF_VOID_P "${VOID_PTR_SIZE}" CACHE STRING "Size of void pointer" FORCE)
set(TARGET_OS "Linux" CACHE STRING "Target OS" FORCE)
EOF

    if [ "$pass_name" == "baseline" ]; then
        # Baseline build without any obfuscation
        if ! cmake "$BENCHMARK_DIR" \
            -DCMAKE_C_COMPILER=$CC \
            -DCMAKE_CXX_COMPILER=$CXX \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_SIZEOF_VOID_P="$VOID_PTR_SIZE" \
            -DTEST_SUITE_BENCHMARKING_ONLY=ON \
            -DTEST_SUITE_SUBDIRS="CTMark;MicroBenchmarks" \
            -DTEST_SUITE_RUN_BENCHMARKS=OFF \
            -DLLVM_TARGETS_TO_BUILD="$TARGET_ARCH" \
            -DLLVM_DEFAULT_TARGET_TRIPLE="$ARCH-unknown-linux-gnu" \
            -DTEST_SUITE_ARCH="$TEST_SUITE_ARCH" \
            -DARCH="x86" \
            -DCMAKE_MODULE_PATH="/tmp/cmake_defines_$$"; then
            
            echo "Error: CMake configuration for $pass_name failed"
            echo "To fix, try manually clearing the build directory:"
            echo "  rm -rf $BUILD_DIR"
            echo "Then run this script again."
        fi
    else
        # Build with obfuscation pass
        if [ "$COMPILER_TYPE" = "clang" ]; then
            # LLVM/Clang plugin flags
            if ! cmake "$BENCHMARK_DIR" \
                -DCMAKE_C_COMPILER=$CC \
                -DCMAKE_CXX_COMPILER=$CXX \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_C_FLAGS="-fpass-plugin=$pass_plugin" \
                -DCMAKE_CXX_FLAGS="-fpass-plugin=$pass_plugin" \
                -DCMAKE_SIZEOF_VOID_P="$VOID_PTR_SIZE" \
                -DTEST_SUITE_BENCHMARKING_ONLY=ON \
                -DTEST_SUITE_SUBDIRS="CTMark;MicroBenchmarks" \
                -DTEST_SUITE_RUN_BENCHMARKS=OFF \
                -DLLVM_TARGETS_TO_BUILD="$TARGET_ARCH" \
                -DLLVM_DEFAULT_TARGET_TRIPLE="$ARCH-unknown-linux-gnu" \
                -DTEST_SUITE_ARCH="$TEST_SUITE_ARCH" \
                -DARCH="x86" \
                -DCMAKE_MODULE_PATH="/tmp/cmake_defines_$$"; then
                
                echo "Error: CMake configuration for $pass_name failed"
                echo "To fix, try manually clearing the build directory:"
                echo "  rm -rf $BUILD_DIR"
                echo "Then run this script again."
            fi
        elif [ "$COMPILER_TYPE" = "gcc" ]; then
            # GCC plugin flags - only use -fno-inline for the rename plugin
            PLUGIN_FLAGS="-fplugin=$pass_plugin"
            if [ "$pass_name" = "rename" ]; then
                PLUGIN_FLAGS="$PLUGIN_FLAGS -fno-inline"
                echo "Using -fno-inline for $pass_name plugin"
            fi
            
            if ! cmake "$BENCHMARK_DIR" \
                -DCMAKE_C_COMPILER=$CC \
                -DCMAKE_CXX_COMPILER=$CXX \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_C_FLAGS="$PLUGIN_FLAGS" \
                -DCMAKE_CXX_FLAGS="$PLUGIN_FLAGS" \
                -DCMAKE_SIZEOF_VOID_P="$VOID_PTR_SIZE" \
                -DTEST_SUITE_BENCHMARKING_ONLY=ON \
                -DTEST_SUITE_SUBDIRS="CTMark;MicroBenchmarks" \
                -DTEST_SUITE_RUN_BENCHMARKS=OFF \
                -DLLVM_TARGETS_TO_BUILD="$TARGET_ARCH" \
                -DLLVM_DEFAULT_TARGET_TRIPLE="$ARCH-unknown-linux-gnu" \
                -DTEST_SUITE_ARCH="$TEST_SUITE_ARCH" \
                -DARCH="x86" \
                -DCMAKE_MODULE_PATH="/tmp/cmake_defines_$$"; then
                
                echo "Initial configuration for $pass_name failed, attempting alternative configurations..."
                if ! try_plugin_configurations "$pass_name" "$pass_plugin" "$BUILD_DIR"; then
                    echo "Error: All CMake configurations for $pass_name failed"
                    echo "To fix, try manually clearing the build directory:"
                    echo "  rm -rf $BUILD_DIR"
                    echo "Then run this script again."
                fi
            fi
        fi
    fi
done

# Clean up temporary files
rm -rf /tmp/cmake_defines_$$

echo "=== Setup complete ==="
echo ""
echo "Next steps:"
echo "1. Build benchmarks: cd builds/build-<pass> && make -j"
echo "2. Run benchmarks: ./run_benchmarks.sh --compiler=$COMPILER_TYPE"
echo "3. Compare results: ./compare_results.py"
echo ""
echo "To benchmark with a different compiler:"
echo "  ./setup_benchmarks.sh --compiler=[clang|gcc] --path=/path/to/compiler"
echo "Example: ./setup_benchmarks.sh --compiler=clang --path=/usr/bin/clang-19"
echo "Example: ./setup_benchmarks.sh --compiler=gcc --path=/usr/bin/gcc"