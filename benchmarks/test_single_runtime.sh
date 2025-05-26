#!/bin/bash

# Test runtime measurement for a single benchmark

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_BASE="$SCRIPT_DIR/builds"

# Test with consumer-typeset (it's fast)
PASS="baseline"
BENCH_DIR="consumer-typeset"
BENCH_EXE="consumer-typeset"

echo "=== Testing runtime measurement ==="

build_dir="$BUILD_BASE/build-$PASS/CTMark/$BENCH_DIR"
exe_path="$build_dir/$BENCH_EXE"
test_file="$build_dir/$BENCH_EXE.test"
timeit_path="$BUILD_BASE/build-$PASS/tools/timeit"

if [ ! -f "$exe_path" ]; then
    echo "Error: Executable not found: $exe_path"
    echo "Please run: cd $BUILD_BASE/build-$PASS && make -j8"
    exit 1
fi

echo "Found executable: $exe_path"

# Check for test input
if [ -f "$test_file" ]; then
    echo "Found test file: $test_file"
    run_cmd=$(grep "^RUN:" "$test_file" | head -1)
    echo "RUN command: $run_cmd"
fi

# Check for timeit
if [ -f "$timeit_path" ]; then
    echo "Found timeit: $timeit_path"
else
    echo "timeit not found, will use time command"
fi

# Extract and prepare run command
run_cmd=$(grep "^RUN:" "$test_file" | head -1 | sed 's/^RUN: *//')
run_cmd=$(echo "$run_cmd" | sed "s|%S|$build_dir|g")
run_cmd=$(echo "$run_cmd" | sed "s|%t|/tmp/test_output|g")

echo ""
echo "Prepared command: $run_cmd"
echo ""
echo "Running benchmark..."

# Run with timeit
if [ -f "$timeit_path" ]; then
    $timeit_path --summary /tmp/test_time.txt sh -c "$run_cmd > /dev/null 2>&1"
    echo "Timing results:"
    cat /tmp/test_time.txt
    user_time=$(grep "user" /tmp/test_time.txt | awk '{print $2}')
    echo "Extracted user time: $user_time seconds"
    rm -f /tmp/test_time.txt
fi

echo ""
echo "Test complete!"