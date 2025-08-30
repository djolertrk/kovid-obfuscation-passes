#!/bin/bash

# Quick test of runtime benchmarking for a single benchmark

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_BASE="$SCRIPT_DIR/builds"

# Test with sqlite3 only (it's relatively fast)
BENCH_DIR="sqlite3"
BENCH_EXE="sqlite3"

echo "=== Testing runtime benchmark with SQLite3 ==="

# Function to run a single test
run_test() {
    local pass_name=$1
    local build_dir="$BUILD_BASE/build-$pass_name/CTMark/$BENCH_DIR"
    local exe_path="$build_dir/$BENCH_EXE"
    
    if [ ! -f "$exe_path" ]; then
        echo "Error: Executable not found: $exe_path"
        echo "Please ensure benchmarks are built first"
        return 1
    fi
    
    echo "Testing $pass_name..."
    
    # Use timeit tool
    local timeit_path="$BUILD_BASE/build-$pass_name/tools/timeit"
    if [ -f "$timeit_path" ]; then
        # Run once with timeit
        $timeit_path --summary /tmp/timeit_test.txt sh -c "$exe_path < $build_dir/sqlite3.test.in > /dev/null 2>&1"
        echo "Timing results:"
        cat /tmp/timeit_test.txt
        rm -f /tmp/timeit_test.txt
    else
        echo "timeit not found, using time command"
        time $exe_path < $build_dir/sqlite3.test.in > /dev/null 2>&1
    fi
}

# Test baseline and one obfuscated version
run_test "baseline"
echo ""
run_test "rename"

echo ""
echo "=== Test complete ==="
echo "If this worked, you can run the full benchmark suite with:"
echo "  ./run_runtime_benchmarks.sh"