#!/usr/bin/env python3
"""
Code Obfuscation Index Calculator
=================================

This tool calculates a single "Code Obfuscation Index" (COI) that quantifies
the level of obfuscation in a binary compared to its unobfuscated version.

The index combines multiple metrics:
1. Control Flow Complexity (CFG analysis)
2. Symbol Table Obfuscation (symbol analysis)
3. String Obfuscation (string analysis)
4. Instruction Complexity (instruction pattern analysis)
5. Binary Size Impact (size analysis)

Usage:
    python3 obfuscation_index.py --original binary_original --obfuscated binary_obfuscated
    python3 obfuscation_index.py --benchmark binary1 binary2 binary3  # Compare multiple binaries

Output:
    Code Obfuscation Index (COI): 0.0 - 100.0
    - 0.0: No obfuscation detected
    - 100.0: Maximum obfuscation detected
"""

import argparse
import subprocess
import json
import re
import math
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple, Optional

class ObfuscationAnalyzer:
    def __init__(self):
        self.metrics = {}

    def analyze_binary(self, binary_path: str) -> Dict:
        """Analyze a binary and return obfuscation metrics."""
        if not os.path.exists(binary_path):
            raise FileNotFoundError(f"Binary not found: {binary_path}")

        print(f"Analyzing binary: {binary_path}")

        metrics = {
            'binary_path': binary_path,
            'file_size': os.path.getsize(binary_path),
            'control_flow_complexity': self._analyze_control_flow(binary_path),
            'symbol_obfuscation': self._analyze_symbols(binary_path),
            'string_obfuscation': self._analyze_strings(binary_path),
            'instruction_complexity': self._analyze_instructions(binary_path),
            'metadata_presence': self._analyze_metadata(binary_path),
            'decompilation_resistance': self._analyze_decompilation_resistance(binary_path)
        }

        return metrics

    def _analyze_control_flow(self, binary_path: str) -> Dict:
        """Analyze control flow complexity using objdump."""
        try:
            # Get disassembly
            result = subprocess.run(['objdump', '-d', binary_path],
                                  capture_output=True, text=True, timeout=30)
            if result.returncode != 0:
                return {'complexity_score': 0, 'error': 'objdump failed'}

            disasm = result.stdout

            # Count different instruction types
            jumps = len(re.findall(r'\bj[a-z]+\s', disasm))
            calls = len(re.findall(r'\bcall\s', disasm))
            branches = len(re.findall(r'\bb[a-z]+\s', disasm))

            # Count basic blocks (approximation)
            basic_blocks = len(re.findall(r'^[0-9a-f]+\s+<[^>]+>:', disasm, re.MULTILINE))

            # Calculate complexity metrics
            total_instructions = len(re.findall(r'^\s*[0-9a-f]+:', disasm, re.MULTILINE))

            if total_instructions == 0:
                return {'complexity_score': 0}

            # Look for obfuscation patterns rather than just complexity
            # Check for excessive conditional jumps (sign of obfuscation)
            conditional_jumps = len(re.findall(r'\bj[cnz][a-z]*\s', disasm))

            # Check for switch-like patterns (dispatcher blocks)
            switch_patterns = len(re.findall(r'jmp.*\*', disasm))

            # Check for excessive basic blocks relative to function size
            functions = len(re.findall(r'^[0-9a-f]+\s+<[^>]+>:', disasm, re.MULTILINE))

            if functions == 0:
                return {'complexity_score': 0}

            # Normal code should have reasonable ratios
            bb_per_function = basic_blocks / functions if functions > 0 else 0
            cond_jump_ratio = conditional_jumps / total_instructions if total_instructions > 0 else 0

            # Score based on unusual patterns that suggest obfuscation
            complexity_score = 0

            # Excessive basic blocks per function (normal: 5-15, obfuscated: 20+)
            if bb_per_function > 20:
                complexity_score += 40
            elif bb_per_function > 15:
                complexity_score += 20
            elif bb_per_function > 10:
                complexity_score += 10

            # High conditional jump ratio (normal: <0.1, obfuscated: >0.2)
            if cond_jump_ratio > 0.3:
                complexity_score += 30
            elif cond_jump_ratio > 0.2:
                complexity_score += 20
            elif cond_jump_ratio > 0.15:
                complexity_score += 10

            # Switch/dispatcher patterns
            if switch_patterns > 0:
                complexity_score += 20

            # Cap at 100
            complexity_score = min(100, complexity_score)

            return {
                'complexity_score': complexity_score,
                'jumps': jumps,
                'calls': calls,
                'branches': branches,
                'basic_blocks': basic_blocks,
                'total_instructions': total_instructions,
                'functions': functions,
                'bb_per_function': bb_per_function,
                'cond_jump_ratio': cond_jump_ratio,
                'switch_patterns': switch_patterns
            }

        except Exception as e:
            return {'complexity_score': 0, 'error': str(e)}

    def _analyze_symbols(self, binary_path: str) -> Dict:
        """Analyze symbol table obfuscation using nm."""
        try:
            # Get symbol table
            result = subprocess.run(['nm', '-D', binary_path],
                                  capture_output=True, text=True, timeout=30)
            if result.returncode != 0:
                # Try without -D flag
                result = subprocess.run(['nm', binary_path],
                                      capture_output=True, text=True, timeout=30)
                if result.returncode != 0:
                    return {'obfuscation_score': 0, 'error': 'nm failed'}

            symbols = result.stdout.strip().split('\n')
            if not symbols or symbols == ['']:
                return {'obfuscation_score': 100, 'total_symbols': 0, 'reason': 'no_symbols'}

            # Analyze symbol names
            total_symbols = len(symbols)
            obfuscated_symbols = 0
            hex_symbols = 0
            short_symbols = 0
            readable_symbols = 0

            for symbol_line in symbols:
                parts = symbol_line.strip().split()
                if len(parts) < 2:
                    continue

                symbol_name = parts[-1]  # Symbol name is usually the last part

                # Skip common system symbols that are expected to be cryptic
                if symbol_name.startswith(('__', '@@', '.L', '_start', '_init', '_fini')):
                    continue

                # Check for obfuscation patterns
                if re.match(r'^[0-9a-fA-F]{8,}$', symbol_name):  # Long hex names (obfuscated)
                    hex_symbols += 1
                    obfuscated_symbols += 1
                elif re.match(r'^_[0-9a-fA-F]{16,}$', symbol_name):  # Prefixed long hex names
                    hex_symbols += 1
                    obfuscated_symbols += 1
                elif len(symbol_name) < 2:  # Very short names
                    short_symbols += 1
                    obfuscated_symbols += 1
                elif re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', symbol_name) and len(symbol_name) > 2:
                    # Normal readable symbol names
                    readable_symbols += 1
                else:
                    # Assume anything else is obfuscated
                    obfuscated_symbols += 1

            meaningful_symbols = total_symbols
            if meaningful_symbols == 0:
                obfuscation_score = 0
            else:
                # Score based on ratio of obfuscated vs readable symbols
                obfuscation_ratio = obfuscated_symbols / meaningful_symbols
                obfuscation_score = min(100, obfuscation_ratio * 100)

            return {
                'obfuscation_score': obfuscation_score,
                'total_symbols': total_symbols,
                'obfuscated_symbols': obfuscated_symbols,
                'hex_symbols': hex_symbols,
                'short_symbols': short_symbols,
                'obfuscation_ratio': obfuscation_ratio
            }

        except Exception as e:
            return {'obfuscation_score': 0, 'error': str(e)}

    def _analyze_strings(self, binary_path: str) -> Dict:
        """Analyze string obfuscation using strings command."""
        try:
            # Get strings from binary
            result = subprocess.run(['strings', binary_path],
                                  capture_output=True, text=True, timeout=30)
            if result.returncode != 0:
                return {'obfuscation_score': 0, 'error': 'strings failed'}

            strings_output = result.stdout.strip().split('\n')
            if not strings_output or strings_output == ['']:
                return {'obfuscation_score': 100, 'total_strings': 0, 'reason': 'no_strings'}

            total_strings = len(strings_output)
            readable_strings = 0
            hex_strings = 0
            short_strings = 0

            for string in strings_output:
                string = string.strip()
                if len(string) < 4:  # Too short to be meaningful
                    short_strings += 1
                    continue

                # Check if string looks like hex data
                if re.match(r'^[0-9a-fA-F]+$', string) and len(string) % 2 == 0:
                    hex_strings += 1
                # Check if string looks readable
                elif re.match(r'^[a-zA-Z0-9\s\.,;:!?\-_/\\]+$', string):
                    readable_strings += 1

            # Calculate obfuscation score based on string patterns
            meaningful_strings = total_strings - short_strings
            if meaningful_strings == 0:
                obfuscation_score = 0  # No meaningful strings to analyze
            else:
                readable_ratio = readable_strings / meaningful_strings
                hex_ratio = hex_strings / meaningful_strings

                # Normal binaries have mostly readable strings
                # Obfuscated binaries have encrypted/hex strings
                if hex_ratio > 0.3:  # High hex content suggests encryption
                    obfuscation_score = min(100, hex_ratio * 80 + 20)
                elif readable_ratio < 0.3:  # Low readable content suggests obfuscation
                    obfuscation_score = min(100, (1 - readable_ratio) * 60)
                else:
                    obfuscation_score = hex_ratio * 30  # Some obfuscation detected

            return {
                'obfuscation_score': obfuscation_score,
                'total_strings': total_strings,
                'readable_strings': readable_strings,
                'hex_strings': hex_strings,
                'short_strings': short_strings,
                'readable_ratio': readable_strings / meaningful_strings if meaningful_strings > 0 else 0
            }

        except Exception as e:
            return {'obfuscation_score': 0, 'error': str(e)}

    def _analyze_instructions(self, binary_path: str) -> Dict:
        """Analyze instruction complexity patterns."""
        try:
            # Get disassembly
            result = subprocess.run(['objdump', '-d', binary_path],
                                  capture_output=True, text=True, timeout=30)
            if result.returncode != 0:
                return {'complexity_score': 0, 'error': 'objdump failed'}

            disasm = result.stdout

            # Count instruction patterns that suggest obfuscation
            xor_instructions = len(re.findall(r'\bxor\s', disasm))
            mov_instructions = len(re.findall(r'\bmov\s', disasm))
            arithmetic_ops = len(re.findall(r'\b(add|sub|mul|div|shl|shr)\s', disasm))
            total_instructions = len(re.findall(r'^\s*[0-9a-f]+:', disasm, re.MULTILINE))

            if total_instructions == 0:
                return {'complexity_score': 0}

            # Calculate complexity based on instruction mix
            xor_ratio = xor_instructions / total_instructions
            arithmetic_ratio = arithmetic_ops / total_instructions

            # Look for obfuscation patterns in instruction usage
            complexity_score = 0

            # Excessive XOR usage (normal: <0.02, obfuscated: >0.05)
            if xor_ratio > 0.1:
                complexity_score += 40
            elif xor_ratio > 0.05:
                complexity_score += 25
            elif xor_ratio > 0.02:
                complexity_score += 10

            # High arithmetic density can indicate dummy operations
            if arithmetic_ratio > 0.3:
                complexity_score += 30
            elif arithmetic_ratio > 0.2:
                complexity_score += 15
            elif arithmetic_ratio > 0.1:
                complexity_score += 5

            complexity_score = min(100, complexity_score)

            return {
                'complexity_score': complexity_score,
                'xor_instructions': xor_instructions,
                'mov_instructions': mov_instructions,
                'arithmetic_ops': arithmetic_ops,
                'total_instructions': total_instructions,
                'xor_ratio': xor_ratio,
                'arithmetic_ratio': arithmetic_ratio
            }

        except Exception as e:
            return {'complexity_score': 0, 'error': str(e)}

    def _analyze_metadata(self, binary_path: str) -> Dict:
        """Analyze presence of debug metadata."""
        try:
            # Check for debug sections
            result = subprocess.run(['readelf', '-S', binary_path],
                                  capture_output=True, text=True, timeout=30)
            if result.returncode != 0:
                return {'obfuscation_score': 0, 'error': 'readelf failed'}

            sections_output = result.stdout

            # Look for debug sections
            debug_sections = re.findall(r'\.debug_\w+', sections_output)
            dwarf_sections = re.findall(r'\.eh_frame', sections_output)

            # Calculate metadata removal score
            total_debug_indicators = len(debug_sections) + len(dwarf_sections)

            # Inverted logic: presence of debug info = low obfuscation score
            if total_debug_indicators == 0:
                obfuscation_score = 80  # Good, no debug info = some obfuscation
            elif total_debug_indicators <= 2:
                obfuscation_score = 40  # Some debug info stripped
            else:
                obfuscation_score = 0   # Lots of debug info = no metadata obfuscation

            return {
                'obfuscation_score': obfuscation_score,
                'debug_sections': debug_sections,
                'dwarf_sections': dwarf_sections,
                'total_debug_indicators': total_debug_indicators
            }

        except Exception as e:
            return {'obfuscation_score': 0, 'error': str(e)}

    def _analyze_decompilation_resistance(self, binary_path: str) -> Dict:
        """Analyze how well the binary resists decompilation using PyGhidra."""
        try:
            import pyghidra as pyhidra
        except ImportError:
            try:
                # Try alternative import names
                import pyhidra
            except ImportError:
                print("ERROR: PyGhidra not found! Install PyGhidra for decompilation resistance analysis.")
                print("Install with: pip install pyghidra")
                print("Or: pip install pyghidra-2.1.0")
                print("Or: pip install pyhidra")
                print("\nNote: Ensure PyGhidra is installed in the same Python environment as this script.")
                sys.exit(1)

        # Initialize PyGhidra with proper Ghidra installation detection
        if not self._initialize_pyghidra(pyhidra):
            return self._heuristic_decompilation_resistance(binary_path)

        try:
            # Initialize PyGhidra and load the binary
            with pyhidra.open_program(binary_path, analyze=True) as flat_api:
                program = flat_api.getCurrentProgram()

                # Initialize decompiler
                from ghidra.app.decompiler import DecompInterface, DecompileOptions
                decompiler = DecompInterface()
                options = DecompileOptions()
                decompiler.setOptions(options)
                decompiler.openProgram(program)

                # Get all functions
                function_manager = program.getFunctionManager()
                functions = function_manager.getFunctions(True)

                total_functions = 0
                successful_decomps = 0
                failed_decomps = 0
                analysis_errors = 0
                decompilation_quality_scores = []

                for function in functions:
                    total_functions += 1
                    try:
                        # Attempt decompilation with timeout
                        decompile_results = decompiler.decompileFunction(function, 30, None)

                        if decompile_results and decompile_results.decompileCompleted():
                            decompiled_function = decompile_results.getDecompiledFunction()
                            if decompiled_function:
                                # Get the C code
                                c_code = str(decompiled_function.getC())

                                # Evaluate decompilation quality
                                quality_score = self._evaluate_decompilation_quality(c_code, function)
                                decompilation_quality_scores.append(quality_score)

                                # Consider successful if quality is above threshold
                                if quality_score > 0.3:  # 30% quality threshold
                                    successful_decomps += 1
                                else:
                                    failed_decomps += 1
                            else:
                                failed_decomps += 1
                        else:
                            failed_decomps += 1

                    except Exception as e:
                        analysis_errors += 1
                        failed_decomps += 1

                # Calculate metrics
                if total_functions > 0:
                    success_rate = successful_decomps / float(total_functions)
                    avg_quality = sum(decompilation_quality_scores) / len(decompilation_quality_scores) if decompilation_quality_scores else 0

                    # Calculate resistance score
                    # High resistance = low success rate + low quality
                    base_resistance = (1.0 - success_rate) * 100
                    quality_penalty = (1.0 - avg_quality) * 100
                    resistance_score = min(100, (base_resistance * 0.7) + (quality_penalty * 0.3))
                else:
                    success_rate = 0.0
                    resistance_score = 0
                    avg_quality = 0

                decompiler.dispose()

                return {
                    'ghidra_available': True,
                    'resistance_score': round(resistance_score, 2),
                    'analysis_method': 'PyGhidra direct analysis',
                    'functions_analyzed': total_functions,
                    'decompilation_success_rate': round(success_rate, 3),
                    'analysis_errors': analysis_errors,
                    'successful_decomps': successful_decomps,
                    'failed_decomps': failed_decomps,
                    'average_quality': round(avg_quality, 3)
                }

        except Exception as e:
            # Fallback to heuristic analysis if PyGhidra fails
            print(f"PyGhidra analysis failed: {e}")
            return self._heuristic_decompilation_resistance(binary_path)

    def _evaluate_decompilation_quality(self, c_code: str, function) -> float:
        """Evaluate the quality of decompiled C code."""
        if not c_code or len(c_code) < 10:
            return 0.0

        quality_score = 0.0

        # Check for meaningful variable names (not just generic ones)
        generic_vars = ['var1', 'var2', 'local_', 'param_', 'uVar', 'iVar']
        generic_count = sum(1 for var in generic_vars if var in c_code)
        if generic_count == 0:
            quality_score += 0.2

        # Check for control flow structures
        control_structures = ['if (', 'for (', 'while (', 'switch (', 'else']
        structure_count = sum(1 for struct in control_structures if struct in c_code)
        if structure_count > 0:
            quality_score += min(0.3, structure_count * 0.1)

        # Check for function calls (indicates preserved semantics)
        if '(' in c_code and ')' in c_code:
            quality_score += 0.2

        # Check for readable code structure
        lines = c_code.split('\n')
        meaningful_lines = [line for line in lines if line.strip() and not line.strip().startswith('//')]
        if len(meaningful_lines) > 3:
            quality_score += 0.2

        # Penalty for obfuscated patterns
        obfuscated_patterns = ['goto', 'label_', 'case 0x', 'undefined']
        obfuscation_count = sum(1 for pattern in obfuscated_patterns if pattern in c_code)
        quality_score -= min(0.5, obfuscation_count * 0.1)

        return max(0.0, min(1.0, quality_score))

    def _initialize_pyghidra(self, pyhidra) -> bool:
        """Initialize PyGhidra with proper Ghidra installation detection."""
        import glob

        # Check if GHIDRA_INSTALL_DIR is already set
        if os.environ.get('GHIDRA_INSTALL_DIR'):
            try:
                pyhidra.start()
                return True
            except Exception as e:
                print(f"Failed to initialize PyGhidra with existing GHIDRA_INSTALL_DIR: {e}")

        # Try to find Ghidra installation
        ghidra_paths = [
            "/opt/ghidra",
            "/usr/local/ghidra",
            "/opt/ghidra_*",
            "/usr/local/ghidra_*",
            os.path.expanduser("~/ghidra"),
            os.path.expanduser("~/ghidra_*"),
            os.path.expanduser("~/.local/share/ghidra"),
            os.path.expanduser("~/.local/share/ghidra_*")
        ]

        for path_pattern in ghidra_paths:
            if '*' in path_pattern:
                matches = glob.glob(path_pattern)
                matches.sort(reverse=True)  # Get latest version first
                for match in matches:
                    if os.path.isdir(match) and self._is_valid_ghidra_dir(match):
                        return self._try_initialize_with_path(pyhidra, match)
            else:
                if os.path.isdir(path_pattern) and self._is_valid_ghidra_dir(path_pattern):
                    return self._try_initialize_with_path(pyhidra, path_pattern)

        # Try to let PyGhidra download Ghidra automatically
        try:
            print("Ghidra installation not found. Attempting to download via PyGhidra...")
            # Try different auto-download approaches
            auto_install_paths = [
                os.path.expanduser("~/.ghidra"),
                os.path.expanduser("~/.local/share/ghidra"),
                "/tmp/ghidra_auto"
            ]

            for auto_path in auto_install_paths:
                try:
                    os.makedirs(auto_path, exist_ok=True)
                    # Try to use PyGhidra's auto-install feature
                    from pyghidra.launcher import HeadlessPyGhidraLauncher
                    launcher = HeadlessPyGhidraLauncher(install_dir=auto_path)
                    pyhidra.start()
                    print(f"Successfully initialized PyGhidra with auto-downloaded Ghidra at: {auto_path}")
                    return True
                except Exception:
                    continue

            # Last resort: try without install_dir parameter
            pyhidra.start()
            return True
        except Exception as e:
            print(f"Failed to initialize PyGhidra: {e}")
            print("Please install Ghidra manually or set GHIDRA_INSTALL_DIR environment variable.")
            print("Download Ghidra from: https://ghidra-sre.org/")
            return False

    def _is_valid_ghidra_dir(self, path: str) -> bool:
        """Check if the path contains a valid Ghidra installation."""
        # Check for essential files that indicate a Ghidra installation
        essential_files = [
            "application.properties",
            "Ghidra",
            "support"
        ]

        for essential_file in essential_files:
            if not os.path.exists(os.path.join(path, essential_file)):
                return False
        return True

    def _try_initialize_with_path(self, pyhidra, ghidra_path: str) -> bool:
        """Try to initialize PyGhidra with a specific Ghidra path."""
        try:
            # Set environment variable
            os.environ['GHIDRA_INSTALL_DIR'] = ghidra_path
            print(f"Using Ghidra installation at: {ghidra_path}")

            # Initialize PyGhidra
            pyhidra.start()
            return True
        except Exception as e:
            print(f"Failed to initialize PyGhidra with {ghidra_path}: {e}")
            return False

    def _heuristic_decompilation_resistance(self, binary_path: str) -> Dict:
        """Fallback heuristic analysis when Ghidra is unavailable."""
        try:
            # Analyze function complexity as proxy for decompilation difficulty
            objdump_result = subprocess.run(['objdump', '-t', binary_path],
                                          capture_output=True, text=True, timeout=30)

            if objdump_result.returncode != 0:
                return {
                    'ghidra_available': False,
                    'resistance_score': 0,
                    'analysis_method': 'Heuristic analysis failed',
                    'functions_analyzed': 0,
                    'decompilation_success_rate': 0.0,
                    'analysis_errors': 1
                }

            # Count functions and analyze symbol obfuscation
            functions = 0
            obfuscated_symbols = 0

            for line in objdump_result.stdout.split('\n'):
                if 'F .text' in line:  # Function in text section
                    functions += 1
                    # Check if symbol name looks obfuscated
                    parts = line.split()
                    if len(parts) >= 6:
                        symbol_name = parts[-1]
                        if (re.match(r'^sub_[0-9A-Fa-f]+$', symbol_name) or
                            re.match(r'^loc_[0-9A-Fa-f]+$', symbol_name) or
                            len(symbol_name) > 20 and not symbol_name.isalnum()):
                            obfuscated_symbols += 1

            # Estimate resistance based on symbol obfuscation
            if functions > 0:
                obfuscation_ratio = obfuscated_symbols / functions
                resistance_score = min(80, obfuscation_ratio * 100)  # Max 80 for heuristics
            else:
                resistance_score = 0

            return {
                'ghidra_available': False,
                'resistance_score': resistance_score,
                'analysis_method': 'Heuristic symbol analysis',
                'functions_analyzed': functions,
                'decompilation_success_rate': 1.0 - obfuscation_ratio if functions > 0 else 0.0,
                'analysis_errors': 0,
                'obfuscated_symbols': obfuscated_symbols
            }

        except Exception as e:
            return {
                'ghidra_available': False,
                'resistance_score': 0,
                'analysis_method': 'Heuristic analysis failed',
                'functions_analyzed': 0,
                'decompilation_success_rate': 0.0,
                'analysis_errors': 1
            }

    def calculate_obfuscation_index(self, metrics: Dict) -> float:
        """Calculate the final Code Obfuscation Index (COI)."""
        # Weights for different metrics (must sum to 1.0)
        weights = {
            'control_flow': 0.30,      # Control flow obfuscation
            'decompilation': 0.25,     # Real-world resistance test
            'symbol': 0.20,            # Symbol obfuscation
            'instruction': 0.15,       # Instruction complexity
            'string': 0.07,            # String obfuscation
            'metadata': 0.03           # Metadata removal
        }

        # Extract scores with fallback to 0
        cf_score = metrics.get('control_flow_complexity', {}).get('complexity_score', 0)
        symbol_score = metrics.get('symbol_obfuscation', {}).get('obfuscation_score', 0)
        instruction_score = metrics.get('instruction_complexity', {}).get('complexity_score', 0)
        string_score = metrics.get('string_obfuscation', {}).get('obfuscation_score', 0)
        metadata_score = metrics.get('metadata_presence', {}).get('obfuscation_score', 0)
        decompilation_score = metrics.get('decompilation_resistance', {}).get('resistance_score', 0)

        # Calculate weighted average
        coi = (cf_score * weights['control_flow'] +
               decompilation_score * weights['decompilation'] +
               symbol_score * weights['symbol'] +
               instruction_score * weights['instruction'] +
               string_score * weights['string'] +
               metadata_score * weights['metadata'])

        return round(coi, 2)

    def compare_binaries(self, original_path: str, obfuscated_path: str) -> Dict:
        """Compare original and obfuscated binaries."""
        print("=== Code Obfuscation Index Analysis ===\n")

        # Check if comparing the same file
        if os.path.samefile(original_path, obfuscated_path):
            print("   WARNING: Comparing the same file to itself!")
            print("   This will show no improvement. Use different binaries for meaningful comparison.\n")

        original_metrics = self.analyze_binary(original_path)
        obfuscated_metrics = self.analyze_binary(obfuscated_path)

        original_coi = self.calculate_obfuscation_index(original_metrics)
        obfuscated_coi = self.calculate_obfuscation_index(obfuscated_metrics)

        improvement = obfuscated_coi - original_coi

        return {
            'original': {
                'path': original_path,
                'metrics': original_metrics,
                'coi': original_coi
            },
            'obfuscated': {
                'path': obfuscated_path,
                'metrics': obfuscated_metrics,
                'coi': obfuscated_coi
            },
            'improvement': improvement,
            'relative_improvement': (improvement / max(original_coi, 1)) * 100
        }

    def print_analysis_report(self, comparison: Dict):
        """Print a detailed analysis report."""
        orig = comparison['original']
        obf = comparison['obfuscated']

        print(f"Original Binary:    {orig['path']}")
        print(f"Obfuscated Binary:  {obf['path']}")
        print("=" * 60)

        print(f"Original COI:       {orig['coi']:.2f}")
        print(f"Obfuscated COI:     {obf['coi']:.2f}")
        print(f"Improvement:        +{comparison['improvement']:.2f} points")
        print(f"Relative Gain:      {comparison['relative_improvement']:.1f}%")
        print()

        # Detailed metrics comparison
        print("Detailed Metrics Comparison:")
        print("-" * 40)

        metrics_names = [
            ('Control Flow Complexity', 'control_flow_complexity', 'complexity_score'),
            ('Decompilation Resistance', 'decompilation_resistance', 'resistance_score'),
            ('Symbol Obfuscation', 'symbol_obfuscation', 'obfuscation_score'),
            ('Instruction Complexity', 'instruction_complexity', 'complexity_score'),
            ('String Obfuscation', 'string_obfuscation', 'obfuscation_score'),
            ('Metadata Removal', 'metadata_presence', 'obfuscation_score')
        ]

        for name, metric_key, score_key in metrics_names:
            orig_score = orig['metrics'].get(metric_key, {}).get(score_key, 0)
            obf_score = obf['metrics'].get(metric_key, {}).get(score_key, 0)
            delta = obf_score - orig_score

            print(f"{name:.<25} {orig_score:6.1f} -> {obf_score:6.1f} ({delta:+6.1f})")

        print()

        # Size comparison
        orig_size = orig['metrics'].get('file_size', 0)
        obf_size = obf['metrics'].get('file_size', 0)
        size_increase = ((obf_size - orig_size) / orig_size * 100) if orig_size > 0 else 0

        print(f"File Size:          {orig_size:,} -> {obf_size:,} bytes ({size_increase:+.1f}%)")
        print()

        # Interpretation
        if obf['coi'] >= 70:
            level = "EXCELLENT"
        elif obf['coi'] >= 50:
            level = "GOOD"
        elif obf['coi'] >= 30:
            level = "MODERATE"
        elif obf['coi'] >= 10:
            level = "MINIMAL"
        else:
            level = "INSUFFICIENT"

        print(f"Obfuscation Level:  {level}")


def main():
    parser = argparse.ArgumentParser(description='Calculate Code Obfuscation Index (COI)')
    parser.add_argument('--original', '-o', help='Path to original (unobfuscated) binary')
    parser.add_argument('--obfuscated', '-b', help='Path to obfuscated binary')
    parser.add_argument('--benchmark', '-m', nargs='+', help='Benchmark multiple binaries')
    parser.add_argument('--json', '-j', action='store_true', help='Output results in JSON format')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')

    args = parser.parse_args()

    analyzer = ObfuscationAnalyzer()

    try:
        if args.original and args.obfuscated:
            # Compare two binaries
            comparison = analyzer.compare_binaries(args.original, args.obfuscated)

            if args.json:
                print(json.dumps(comparison, indent=2))
            else:
                analyzer.print_analysis_report(comparison)

        elif args.benchmark:
            # Benchmark multiple binaries
            print("=== Code Obfuscation Index Benchmark ===\n")

            results = []
            for binary_path in args.benchmark:
                metrics = analyzer.analyze_binary(binary_path)
                coi = analyzer.calculate_obfuscation_index(metrics)
                results.append({
                    'binary': binary_path,
                    'coi': coi,
                    'metrics': metrics if args.verbose else None
                })

            if args.json:
                print(json.dumps(results, indent=2))
            else:
                print("Binary Obfuscation Comparison:")
                print("-" * 50)
                for result in sorted(results, key=lambda x: x['coi'], reverse=True):
                    print(f"{result['binary']:.<40} {result['coi']:6.2f}")

        else:
            parser.print_help()

    except KeyboardInterrupt:
        print("\nAnalysis interrupted.")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
