#!/bin/bash

# Benchmark setup script for KoviD Obfuscation Passes
# This script sets up the LLVM test suite and prepares benchmarks

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LLVM_VERSION="19"

echo "=== KoviD Obfuscation Passes Benchmark Setup ==="
echo "Project root: $PROJECT_ROOT"

# Check if clang is available
if ! command -v clang-${LLVM_VERSION} &> /dev/null && ! command -v clang &> /dev/null; then
    echo "Error: clang not found. Please install LLVM ${LLVM_VERSION}."
    exit 1
fi

# Set compiler paths
if command -v clang-${LLVM_VERSION} &> /dev/null; then
    #export CC=clang-${LLVM_VERSION}
    #export CXX=clang++-${LLVM_VERSION}
    export CC=clang
    export CXX=clang++
else
    export CC=clang
    export CXX=clang++
fi

echo "Using compiler: $CC"

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

# List of obfuscation passes to test
declare -a PASSES=(
    "baseline:none"
    "rename:$PROJECT_ROOT/build/lib/libKoviDRenameCodeLLVMPlugin.dylib"
    "dummy:$PROJECT_ROOT/build/lib/libKoviDDummyCodeInsertionLLVMPlugin.dylib"
    "instruction:$PROJECT_ROOT/build/lib/libKoviDInstructionObfuscationPassLLVMPlugin.dylib"
    "cft:$PROJECT_ROOT/build/lib/libKoviDControlFlowTaintLLVMPlugin.dylib"
)

# Create CMake configuration for each pass
for pass_config in "${PASSES[@]}"; do
    IFS=':' read -r pass_name pass_plugin <<< "$pass_config"
    BUILD_DIR="$BUILD_BASE/build-$pass_name"
    
    echo "=== Configuring build for $pass_name ==="
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    if [ "$pass_name" == "baseline" ]; then
        # Baseline build without any obfuscation
        cmake "$BENCHMARK_DIR" \
            -DCMAKE_C_COMPILER=$CC \
            -DCMAKE_CXX_COMPILER=$CXX \
            -DCMAKE_BUILD_TYPE=Release \
            -DTEST_SUITE_BENCHMARKING_ONLY=ON \
            -DTEST_SUITE_SUBDIRS="CTMark;MicroBenchmarks" \
            -DTEST_SUITE_RUN_BENCHMARKS=OFF
    else
        # Build with obfuscation pass
        cmake "$BENCHMARK_DIR" \
            -DCMAKE_C_COMPILER=$CC \
            -DCMAKE_CXX_COMPILER=$CXX \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_FLAGS="-fpass-plugin=$pass_plugin" \
            -DCMAKE_CXX_FLAGS="-fpass-plugin=$pass_plugin" \
            -DTEST_SUITE_BENCHMARKING_ONLY=ON \
            -DTEST_SUITE_SUBDIRS="CTMark;MicroBenchmarks" \
            -DTEST_SUITE_RUN_BENCHMARKS=OFF
    fi
done

echo "=== Setup complete ==="
echo ""
echo "Next steps:"
echo "1. Build benchmarks: cd builds/build-<pass> && make -j"
echo "2. Run benchmarks: ./run_benchmarks.sh"
echo "3. Compare results: ./compare_results.py"