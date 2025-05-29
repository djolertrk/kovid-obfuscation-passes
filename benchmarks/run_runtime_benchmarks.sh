#!/bin/bash

# Runtime benchmark runner for KoviD Obfuscation Passes (Version 2)
# This script runs the actual benchmarks and measures their execution time

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_BASE="$SCRIPT_DIR/builds"
RESULTS_DIR="$SCRIPT_DIR/runtime_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create results directory
mkdir -p "$RESULTS_DIR"

# Activate virtual environment if it exists
if [ -f "$SCRIPT_DIR/venv/bin/activate" ]; then
    source "$SCRIPT_DIR/venv/bin/activate"
fi

# Number of runs for each benchmark
NUM_RUNS=20

echo "=== KoviD Obfuscation Passes Runtime Benchmark Runner ==="
echo "Results will be saved to: $RESULTS_DIR"
echo "Number of runs per benchmark: $NUM_RUNS"

# List of benchmarks to run
declare -a BENCHMARKS=(
    "7zip:7zip-benchmark"
    "sqlite3:sqlite3"
    "SPASS:SPASS"
    "consumer-typeset:consumer-typeset"
    "kimwitu++:kc"
    "lencod:lencod"
    "mafft:pairlocalalign"
    "tramp3d-v4:tramp3d-v4"
    "ClamAV:clamscan"
    "Bullet:bullet"
)

# Function to run a single benchmark
run_single_benchmark() {
    local pass_name=$1
    local bench_dir=$2
    local bench_exe=$3
    local run_num=$4
    
    local build_dir="$BUILD_BASE/build-$pass_name/CTMark/$bench_dir"
    local exe_path="$build_dir/$bench_exe"
    local test_file="$build_dir/$bench_exe.test"
    local timeit_path="$BUILD_BASE/build-$pass_name/tools/timeit"
    
    if [ ! -f "$exe_path" ]; then
        return
    fi
    
    # Extract RUN command from test file
    local run_cmd=""
    if [ -f "$test_file" ]; then
        run_cmd=$(grep "^RUN:" "$test_file" | head -1 | sed 's/^RUN: *//')
        run_cmd=$(echo "$run_cmd" | sed "s|%S|$build_dir|g")
        run_cmd=$(echo "$run_cmd" | sed "s|%t|/tmp/bench_output_$$|g")
    else
        run_cmd="$exe_path"
    fi
    
    # Run with timeit
    if [ -f "$timeit_path" ]; then
        $timeit_path --summary /tmp/timeit_$$_${run_num}.txt sh -c "$run_cmd > /dev/null 2>&1"
        grep "user" /tmp/timeit_$$_${run_num}.txt | awk '{print $2}'
        rm -f /tmp/timeit_$$_${run_num}.txt
    else
        # Fallback to time command
        { time -p sh -c "$run_cmd > /dev/null 2>&1"; } 2>&1 | grep "^user" | awk '{print $2}'
    fi
}

# Run benchmarks for each pass
declare -a PASSES=(
    "baseline"
    "rename"
    "dummy"
    "instruction"
    "cft"
)

for pass in "${PASSES[@]}"; do
    echo "=== Running benchmarks for $pass ==="
    
    # Create temporary file for results
    tmp_file="/tmp/bench_results_${pass}_$$.txt"
    echo "{" > "$tmp_file"
    
    first=true
    for bench_config in "${BENCHMARKS[@]}"; do
        IFS=':' read -r bench_dir bench_exe <<< "$bench_config"
        bench_name=$(basename $bench_dir)
        
        echo "  Running $bench_name..."
        
        # Collect times
        times=""
        for run in $(seq 1 $NUM_RUNS); do
            user_time=$(run_single_benchmark "$pass" "$bench_dir" "$bench_exe" "$run")
            if [ ! -z "$user_time" ]; then
                echo "    Run $run: ${user_time}s"
                if [ -z "$times" ]; then
                    times="$user_time"
                else
                    times="$times,$user_time"
                fi
            fi
        done
        
        # Add to JSON if we got results
        if [ ! -z "$times" ]; then
            if [ "$first" = false ]; then
                echo "," >> "$tmp_file"
            fi
            first=false
            
            # Calculate mean
            mean=$(echo "$times" | awk -F, '{sum=0; for(i=1;i<=NF;i++) sum+=$i; printf "%.4f", sum/NF}')
            
            # Write JSON entry
            echo -n "  \"$bench_name\": {" >> "$tmp_file"
            echo -n "\"runtime\": {" >> "$tmp_file"
            echo -n "\"values\": [$(echo $times | sed 's/,/, /g')]," >> "$tmp_file"
            echo -n "\"mean\": $mean" >> "$tmp_file"
            echo -n "}}" >> "$tmp_file"
        fi
    done
    
    echo "" >> "$tmp_file"
    echo "}" >> "$tmp_file"
    
    # Save results
    result_file="$RESULTS_DIR/${pass}_runtime_${TIMESTAMP}.json"
    mv "$tmp_file" "$result_file"
    echo "  Results saved to: $result_file"
done

echo ""
echo "=== All runtime benchmarks complete ==="
echo "Results are in: $RESULTS_DIR"
echo ""
echo "To compare runtime performance, run:"
echo "  python3 compare_runtime_results.py $RESULTS_DIR/*_runtime_${TIMESTAMP}.json --latex"