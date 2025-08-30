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

#### Using LLVM/Clang Plugins
```bash
# For macOS, add LLVM to your PATH
export PATH=/opt/homebrew/opt/llvm@19/bin/:$PATH

# Setup with default clang compiler
./setup_benchmarks.sh --compiler=clang

# Or specify a custom compiler path (useful on Linux)
./setup_benchmarks.sh --compiler=clang --path=/usr/bin/clang-19

# Build benchmarks (this takes time)
cd builds/build-baseline && make -j8
cd ../build-rename && make -j8
# ... repeat for other passes

# Run compilation benchmarks (will automatically use venv if present)
cd ../..
./run_benchmarks.sh --compiler=clang
```

#### Using GCC Plugins
```bash
# Setup with GCC compiler
./setup_benchmarks.sh --compiler=gcc

# Or specify a custom compiler path
./setup_benchmarks.sh --compiler=gcc --path=/usr/bin/gcc-12

# Build benchmarks (this takes time)
cd builds/build-baseline && make -j8
cd ../build-rename && make -j8
# ... repeat for other passes

# Run compilation benchmarks
cd ../..
./run_benchmarks.sh --compiler=gcc

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

## Troubleshooting

### Plugin File Not Found
If you see errors like:
```
Could not load library '.../libKoviDRenameCodeLLVMPlugin.dylib': ... No such file or directory
```
Make sure:
1. You have built the plugins for the compiler you're using
2. The plugin extension matches your system (.so for Linux, .dylib for macOS)
3. The correct path is being used (check the PASSES array in setup_benchmarks.sh)
4. You've specified the correct compiler type (--compiler=clang or --compiler=gcc)

### Compiler Not Found
If you get "compiler not found" errors:
1. Make sure the compiler is installed
2. Use the --path parameter to specify the exact path to the compiler
3. For clang, check that both clang and clang++ are available

### Linux vs. macOS Differences
- On Linux, plugins use the .so extension
- On macOS, plugins use the .dylib extension
- The script automatically detects the correct extension for your system

### Cross-Compiler Issues
When using a compiler from a non-standard location:
```bash
./setup_benchmarks.sh --compiler=clang --path=/custom/path/to/clang
```

### CMake Configuration Issues

The script automatically addresses common CMake issues by:
- Detecting and setting the proper void pointer size
- Explicitly specifying the system architecture
- Completely replacing the architecture detection modules
- Creating custom C files for compile-time architecture detection
- Adding fallback options for problematic plugins (like -fno-inline, -O0)
- Automatically trying multiple configurations for known troublesome plugins

If you encounter errors like:
```
CMake Error: CMAKE_SIZEOF_VOID_P is not defined
```

or

```
Could not detect target system architecture!
```

Try the following steps:

1. Manually clear the build directory for the failed pass:
```bash
rm -rf benchmarks/builds/build-problematic_pass
```

2. Re-run the setup script with the same parameters:
```bash
./setup_benchmarks.sh --compiler=gcc --path=/usr/bin/gcc-12
```

3. If problems persist, you can manually force the architecture:
```bash
# For x86_64 systems:
TEST_SUITE_ARCH=x86_64 ./setup_benchmarks.sh --compiler=gcc --path=/usr/bin/gcc-12
```

### GCC Plugin Issues

When using GCC plugins, you might encounter compilation errors with certain passes. The script applies specific workarounds based on the plugin:

1. For RenameCode plugin (`libKoviDRenameCodeGCCPlugin.so`):
   - Uses `-fno-inline` to disable function inlining (required for this plugin)
   - May add additional flags like `-fno-strict-aliasing` if needed

2. For other problematic plugins:
   - Tries multiple flag combinations without forcing `-fno-inline`
   - May use `-fno-strict-aliasing` or `-O0` if the default configuration fails

If a plugin still fails to work, you can manually modify the script to add additional flags:

```bash
# Edit the plugin_options array in the script:
local plugin_options=(
    "-fplugin=$pass_plugin -fno-inline"
    "-fplugin=$pass_plugin -fno-inline -fno-strict-aliasing"
    "-fplugin=$pass_plugin -O0"
    # Add your custom options here:
    "-fplugin=$pass_plugin -fno-inline -O0 -fno-ipa-cp"
)
```
