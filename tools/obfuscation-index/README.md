# Code Obfuscation Index (COI) Calculator

A tool to quantitatively measure and compare code obfuscation effectiveness using a single numerical index (0-100).

## Overview

The Code Obfuscation Index (COI) provides a standardized way to measure obfuscation effectiveness by analyzing multiple aspects of a binary:

- **Control Flow Complexity (30%)**: CFG complexity, branch density, basic block patterns
- **Decompilation Resistance (25%)**: Real-world resistance using Ghidra decompilation analysis
- **Symbol Obfuscation (20%)**: Symbol table analysis, name pattern detection
- **Instruction Complexity (15%)**: Instruction pattern analysis, arithmetic operation density
- **String Obfuscation (7%)**: String table analysis, hex pattern detection
- **Metadata Removal (3%)**: Debug information presence, section analysis

The tool detects obfuscation patterns rather than general complexity, ensuring accurate differentiation between normal and obfuscated binaries. It includes Ghidra-based decompilation resistance testing for real-world evaluation and falls back to heuristic analysis when Ghidra is unavailable.

## Requirements

- Linux system with standard binutils tools
- Python 3.6+ with argparse and json modules
- objdump, nm, strings, readelf utilities
- ELF binary format support
- **PyGhidra** for decompilation resistance analysis (required)
- Java 17+ (required by PyGhidra/Ghidra)

## Installation

### 1. Install System Dependencies

```bash
# Install required system tools
sudo apt-get install binutils elfutils openjdk-17-jdk

# Verify Java installation
java -version
```

### 2. Install PyGhidra

```bash
# Install PyGhidra (includes Ghidra)
pip install pyghidra

# Or install specific version
pip install pyghidra-2.1.0

# Initialize PyGhidra (downloads Ghidra automatically)
python3 -c "import pyghidra; print('PyGhidra installed successfully')"
```

Basically you can follow https://github.com/NationalSecurityAgency/ghidra?tab=readme-ov-file#install.

## Usage

### Set up enviroment

```bash
export PATH=/path/to/jdk-22.0.2+9/bin:$PATH
export GHIDRA_INSTALL_DIR=/path/to/ghidra/
```

### Compare Original vs Obfuscated Binary

```bash
# Basic comparison
python3 obfuscation_index.py --original program_orig --obfuscated program_obf

# JSON output for automation
python3 obfuscation_index.py --original program_orig --obfuscated program_obf --json

# Verbose output with detailed analysis
python3 obfuscation_index.py --original program_orig --obfuscated program_obf --verbose
```

### Benchmark Multiple Binaries

```bash
# Compare multiple binaries
python3 obfuscation_index.py --benchmark binary1 binary2 binary3

# Verbose benchmark with detailed metrics
python3 obfuscation_index.py --benchmark *.bin --verbose

# JSON output for automated processing
python3 obfuscation_index.py --benchmark program_* --json
```

## Testing PyGhidra Integration

To verify that PyGhidra integration is working correctly:

### Quick Test

```bash
# Test PyGhidra installation
python3 -c "import pyhidra; print('PyGhidra available')"

# Test COI tool with PyGhidra
python3 obfuscation_index.py --benchmark /bin/ls

# If successful, you should see "Decompilation Resistance" with "PyGhidra direct analysis" method
```

### Comprehensive Test

```bash
# Run the included test suite
python3 test_ghidra.py

# This will:
# 1. Verify PyGhidra availability
# 2. Create test binaries with different complexity levels
# 3. Run COI analysis with PyGhidra integration
# 4. Test both comparison and benchmark modes
# 5. Clean up automatically
```

### Manual Testing

```bash
# Create a simple test binary
echo 'int main(){return 0;}' > test.c
gcc test.c -o test_normal
gcc -s test.c -o test_stripped

# Run COI analysis
python3 obfuscation_index.py --original test_normal --obfuscated test_stripped

# Look for "Decompilation Resistance" and "PyGhidra direct analysis" in the output
```

### Troubleshooting

```bash
# If PyGhidra fails to initialize with "GHIDRA_INSTALL_DIR is not set":

# Option 1: Set GHIDRA_INSTALL_DIR manually
export GHIDRA_INSTALL_DIR=/opt/ghidra  # or your Ghidra path
python3 obfuscation_index.py --benchmark /bin/ls

# Option 2: Let PyGhidra download Ghidra automatically
python3 -c "import pyghidra; pyghidra.start()"

# Option 3: Install Ghidra manually
# Download from https://ghidra-sre.org/ and extract to /opt/ghidra
wget https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_10.4_build/ghidra_10.4_PUBLIC_20230928.zip
sudo unzip ghidra_10.4_PUBLIC_20230928.zip -d /opt/
sudo mv /opt/ghidra_10.4_PUBLIC /opt/ghidra
export GHIDRA_INSTALL_DIR=/opt/ghidra

# Make the environment variable permanent
echo 'export GHIDRA_INSTALL_DIR=/opt/ghidra' >> ~/.bashrc

# Other troubleshooting steps:
# 1. Ensure Java 17+ is installed
java -version

# 2. Clear PyGhidra cache and reinitialize
rm -rf ~/.pyhidra
python3 -c "import pyghidra; pyghidra.start()"

# 3. Check for error messages
python3 -c "import pyghidra; pyghidra.open_program('/bin/ls')"
```

## Example Output

### Comparison Mode
```
=== Code Obfuscation Index Analysis ===

Original Binary:    test_program
Obfuscated Binary:  test_program_obf
============================================================
Original COI:       12.45
Obfuscated COI:     67.83
Improvement:        +55.38 points
Relative Gain:      444.9%

Detailed Metrics Comparison:
----------------------------------------
Control Flow Complexity..    8.2 ->  72.1 (+63.9)
Decompilation Resistance.   12.5 ->  89.3 (+76.8)
Symbol Obfuscation.......   15.0 ->  85.6 (+70.6)
Instruction Complexity...    5.1 ->  45.2 (+40.1)
String Obfuscation.......   20.0 ->  78.9 (+58.9)
Metadata Removal.........   10.0 ->  80.0 (+70.0)

File Size:          45,632 -> 52,108 bytes (+14.2%)

Obfuscation Level:  GOOD
```

### Benchmark Mode
```
=== Code Obfuscation Index Benchmark ===

Analyzing binary: program_original
Analyzing binary: program_kovid
Analyzing binary: program_ollvm
Binary Obfuscation Comparison:
--------------------------------------------------
program_original........................  13.17
program_kovid...........................  68.42
program_ollvm...........................  71.85
```

### JSON Output
```json
{
  "original": {
    "file": "test_program",
    "coi": 12.45,
    "metrics": {
      "control_flow": 8.2,
      "symbol_obf": 15.0,
      "instruction": 5.1,
      "string_obf": 20.0,
      "metadata": 10.0
    }
  },
  "obfuscated": {
    "file": "test_program_obf",
    "coi": 67.83,
    "improvement": 55.38,
    "relative_gain": 444.9
  }
}
```

## COI Score Interpretation

| COI Range | Level | Description |
|-----------|-------|-------------|
| 70-100 | **EXCELLENT** | Highly obfuscated, very difficult to reverse engineer |
| 50-69 | **GOOD** | Well obfuscated, significant protection applied |
| 30-49 | **MODERATE** | Some obfuscation present, basic protection |
| 10-29 | **MINIMAL** | Light obfuscation, limited protection |
| 0-9 | **INSUFFICIENT** | No significant obfuscation detected |
