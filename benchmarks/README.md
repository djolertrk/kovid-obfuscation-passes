# KoviD Obfuscation Passes Benchmarks

This directory contains benchmarking tools to measure the performance impact of various obfuscation passes.

## Types of Benchmarks

### 1. Compilation Metrics (`run_benchmarks.sh`)
Measures how obfuscation passes affect compilation time and binary size.

### 2. Runtime Performance (`run_runtime_benchmarks.sh`)
Measures how obfuscation passes affect the execution speed of compiled programs (e.g., how much slower 7zip, SQLite, etc. run after obfuscation).

## Prerequisites

### Python Dependencies
The benchmark tools require Python packages for test execution and result analysis. We recommend using a virtual environment:

```bash
# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate

# Install required packages
pip install psutil lit
```

The required packages are:
- `psutil` - For system monitoring and test timeouts
- `lit` - LLVM's Integrated Tester for running benchmarks

## Quick Start

### Clone LLVM Test Suite
First, clone the LLVM test suite into this directory:
```bash
git clone https://github.com/llvm/llvm-test-suite.git
```

### Full Benchmark Suite
For comprehensive benchmarking using LLVM's CTMark suite:
```bash
export PATH=/opt/homebrew/opt/llvm@19/bin/:$PATH
# Setup
./setup_benchmarks.sh

# Build benchmarks (this takes time)
cd builds/build-baseline && make -j8
cd ../build-rename && make -j8
# ... repeat for other passes

# Run compilation benchmarks (will automatically use venv if present)
cd ../..
./run_benchmarks.sh

# Analyze compilation results (specify JSON files, not directory)
python3 compare_results.py results/baseline_*.json results/rename_*.json results/cft_*.json --latex

# Or to compare all results from a specific timestamp:
python3 compare_results.py results/*_20250529_103929.json --latex

## Runtime Performance Benchmarking

To measure how obfuscation affects program execution speed:

```bash
# Or use the original version
./run_runtime_benchmarks.sh

# Check if benchmarks are running correctly (important!)
./check_benchmark_status.sh

# Analyze runtime results
python3 compare_runtime_results.py runtime_results/*_runtime_*.json --latex

# Export results to CSV
python3 compare_runtime_results.py runtime_results/*_runtime_*.json --csv runtime_overhead.csv
```

**Note**: Some obfuscation passes may cause benchmarks to crash. Use `check_benchmark_status.sh` to verify that obfuscated programs run correctly before trusting runtime measurements.

## Files

### Setup and Execution
- `setup_benchmarks.sh` - Sets up LLVM test suite builds with obfuscation passes
- `run_benchmarks.sh` - Measures compilation time and binary size
- `run_runtime_benchmarks.sh` - Measures runtime performance of obfuscated binaries

### Analysis Tools
- `compare_results.py` - Analyzes compilation metrics (compile time, binary size)
- `compare_runtime_results.py` - Analyzes runtime performance overhead
- `combine_results.py` - Aggregates multiple benchmark runs

## Results

The benchmarks measure two types of performance impact:

### Compilation Metrics
- Compilation time overhead (how much longer compilation takes)
- Binary size increase (total binary and text section size)

### Runtime Performance
- Execution time overhead (how much slower obfuscated programs run)
- Per-benchmark performance impact

Results can be exported as:
- LaTeX tables for academic papers
- CSV files for further analysis
- Detailed per-benchmark breakdowns
