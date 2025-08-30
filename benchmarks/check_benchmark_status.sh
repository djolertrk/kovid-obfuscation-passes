#!/bin/bash

# Check if benchmarks are running correctly

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_BASE="$SCRIPT_DIR/builds"

echo "=== Checking Benchmark Status ==="
echo ""

# Function to check a single benchmark
check_benchmark() {
    local pass_name=$1
    local bench_dir=$2
    local bench_exe=$3
    
    local build_dir="$BUILD_BASE/build-$pass_name/CTMark/$bench_dir"
    local exe_path="$build_dir/$bench_exe"
    local test_file="$build_dir/$bench_exe.test"
    
    echo "Checking $pass_name/$bench_dir:"
    
    if [ ! -f "$exe_path" ]; then
        echo "  ❌ Executable not found: $exe_path"
        return 1
    else
        echo "  ✓ Executable found"
    fi
    
    # Extract and run command
    if [ -f "$test_file" ]; then
        local run_cmd=$(grep "^RUN:" "$test_file" | head -1 | sed 's/^RUN: *//')
        run_cmd=$(echo "$run_cmd" | sed "s|%S|$build_dir|g")
        run_cmd=$(echo "$run_cmd" | sed "s|%t|/tmp/bench_test_$$|g")
        
        echo "  Running: $run_cmd"
        
        # Run and check exit status
        if sh -c "$run_cmd > /tmp/bench_output_$$ 2>&1"; then
            echo "  ✓ Benchmark completed successfully"
            # Check output size
            local output_size=$(wc -c < /tmp/bench_output_$$)
            echo "  Output size: $output_size bytes"
            if [ $output_size -lt 100 ]; then
                echo "  ⚠️  Warning: Output seems too small"
                echo "  First 50 chars of output:"
                head -c 50 /tmp/bench_output_$$ | sed 's/^/    /'
                echo ""
            fi
        else
            echo "  ❌ Benchmark failed with exit code: $?"
            echo "  Error output:"
            head -20 /tmp/bench_output_$$ | sed 's/^/    /'
        fi
        
        rm -f /tmp/bench_output_$$ /tmp/bench_test_$$
    else
        echo "  ❌ Test file not found"
    fi
    
    echo ""
}

# Check a few benchmarks for baseline and cft
echo "=== Baseline Benchmarks ==="
check_benchmark "baseline" "sqlite3" "sqlite3"
check_benchmark "baseline" "SPASS" "SPASS"

echo "=== CFT Obfuscated Benchmarks ==="
check_benchmark "cft" "sqlite3" "sqlite3"
check_benchmark "cft" "SPASS" "SPASS"

echo "=== Summary ==="
echo "If obfuscated benchmarks are failing or producing small output,"
echo "it means the obfuscation is breaking the programs."