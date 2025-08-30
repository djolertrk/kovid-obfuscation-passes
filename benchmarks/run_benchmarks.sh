#!/bin/bash

# Benchmark runner for KoviD Obfuscation Passes
# This script runs benchmarks and collects performance data

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_BASE="$SCRIPT_DIR/builds"
RESULTS_DIR="$SCRIPT_DIR/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Activate virtual environment if it exists
if [ -f "$SCRIPT_DIR/venv/bin/activate" ]; then
    source "$SCRIPT_DIR/venv/bin/activate"
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

# Number of runs for each benchmark
NUM_RUNS=3

echo "=== KoviD Obfuscation Passes Benchmark Runner ==="
echo "Results will be saved to: $RESULTS_DIR"
echo "Number of runs per benchmark: $NUM_RUNS"

# Function to run a single benchmark
run_benchmark() {
    local pass_name=$1
    local build_dir="$BUILD_BASE/build-$pass_name"
    local result_file="$RESULTS_DIR/${pass_name}_${TIMESTAMP}.json"
    
    if [ ! -d "$build_dir" ]; then
        echo "Error: Build directory not found: $build_dir"
        echo "Please run setup_benchmarks.sh first"
        return 1
    fi
    
    echo "=== Running benchmarks for $pass_name ==="
    cd "$build_dir"
    
    # Use lit to run the benchmarks
    if command -v lit &> /dev/null; then
        LIT_CMD="lit"
    elif command -v llvm-lit &> /dev/null; then
        LIT_CMD="llvm-lit"
    else
        echo "Error: lit not found. Please install lit (pip install lit)"
        return 1
    fi
    
    # Run benchmarks multiple times and collect timing data
    for run in $(seq 1 $NUM_RUNS); do
        echo "  Run $run of $NUM_RUNS..."
        $LIT_CMD -v -o "${result_file}.run${run}" CTMark/ --timeout=600
    done
    
    # Combine results
    echo "  Combining results..."
    python3 "$SCRIPT_DIR/combine_results.py" \
        "${result_file}.run"* \
        > "$result_file"
    
    # Clean up individual run files
    rm -f "${result_file}.run"*
    
    echo "  Results saved to: $result_file"
}

# Add support for compiler type parameter
COMPILER_TYPE="clang" # Default to clang (can be "gcc" or "clang")

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --compiler=*)
            COMPILER_TYPE="${1#*=}"
            shift
            ;;
        --help)
            echo "Usage: $0 [--compiler=clang|gcc]"
            echo "  --compiler=TYPE    Specify compiler type (clang or gcc)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "Using compiler type: $COMPILER_TYPE"

# List of passes to benchmark - ensure it matches what's in setup_benchmarks.sh
if [ "$COMPILER_TYPE" = "clang" ]; then
    declare -a PASSES=(
        "baseline"
        "rename"
        "dummy"
        "instruction"
        "cft"
        "metadata"
        "string"
    )
elif [ "$COMPILER_TYPE" = "gcc" ]; then
    declare -a PASSES=(
        "baseline"
        "rename"
        "dummy"
        "instruction"
        "cft"
        "metadata"
        "string"
    )
else
    echo "Error: Unknown compiler type: $COMPILER_TYPE. Use 'clang' or 'gcc'."
    exit 1
fi

# Run benchmarks for each pass
for pass in "${PASSES[@]}"; do
    run_benchmark "$pass"
done

echo ""
echo "=== All benchmarks complete ==="
echo "Results are in: $RESULTS_DIR"
echo ""
echo "To compare results, run:"
echo "  python3 compare_results.py $RESULTS_DIR/*_${TIMESTAMP}.json"