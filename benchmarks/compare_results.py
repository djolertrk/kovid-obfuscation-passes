#!/usr/bin/env python3

"""
Compare benchmark results from different obfuscation passes
Generates performance impact analysis and visualizations
"""

import json
import sys
import os
from collections import defaultdict
import statistics
import argparse

def load_results(filename):
    """Load benchmark results from JSON file"""
    with open(filename, 'r') as f:
        data = json.load(f)
        
    # Handle our combined JSON format
    if isinstance(data, dict) and all(isinstance(v, dict) for v in data.values()):
        # This is our combined format
        results = {}
        for bench_name, metrics in data.items():
            bench_results = {}
            
            # Extract compile time
            if 'compile_time' in metrics and isinstance(metrics['compile_time'], dict):
                bench_results['compile_time'] = metrics['compile_time']['mean']
            elif 'compile_time' in metrics:
                bench_results['compile_time'] = metrics['compile_time']
                
            # Extract binary size
            if 'size' in metrics and isinstance(metrics['size'], dict):
                bench_results['size'] = metrics['size']['mean']
            elif 'size' in metrics:
                bench_results['size'] = metrics['size']
                
            # Extract text section size (most relevant for code size)
            if 'size.__text' in metrics and isinstance(metrics['size.__text'], dict):
                bench_results['size.__text'] = metrics['size.__text']['mean']
            elif 'size.__text' in metrics:
                bench_results['size.__text'] = metrics['size.__text']
                
            results[bench_name] = bench_results
        return results
    
    # Handle lit's JSON format
    if 'tests' in data:
        results = {}
        for test in data['tests']:
            if test['code'] == 'PASS' and 'metrics' in test:
                bench_name = test['name'].split('/')[-2]  # Extract benchmark name
                bench_results = {}
                
                if 'compile_time' in test['metrics']:
                    bench_results['compile_time'] = test['metrics']['compile_time']
                if 'size' in test['metrics']:
                    bench_results['size'] = test['metrics']['size']
                if 'size.__text' in test['metrics']:
                    bench_results['size.__text'] = test['metrics']['size.__text']
                    
                results[bench_name] = bench_results
        return results
    
    return {}

def calculate_overhead(baseline_value, obfuscated_value):
    """Calculate percentage overhead"""
    if baseline_value == 0:
        return 0
    return ((obfuscated_value - baseline_value) / baseline_value) * 100

def analyze_results(results_files):
    """Analyze benchmark results and calculate overheads"""
    # Load all results
    all_results = {}
    for filename in results_files:
        pass_name = os.path.basename(filename).split('_')[0]
        results = load_results(filename)
        if results:
            all_results[pass_name] = results
    
    # Find baseline
    if 'baseline' not in all_results:
        print("Error: No baseline results found")
        sys.exit(1)
    
    baseline = all_results['baseline']
    
    # Calculate overheads for each pass
    analysis = {}
    for pass_name, results in all_results.items():
        if pass_name == 'baseline':
            continue
        
        compile_overheads = []
        size_overheads = []
        text_size_overheads = []
        
        for test_name, metrics in results.items():
            if test_name in baseline:
                baseline_metrics = baseline[test_name]
                
                # Compile time overhead
                if 'compile_time' in metrics and 'compile_time' in baseline_metrics:
                    overhead = calculate_overhead(baseline_metrics['compile_time'], metrics['compile_time'])
                    compile_overheads.append(overhead)
                
                # Binary size overhead
                if 'size' in metrics and 'size' in baseline_metrics:
                    overhead = calculate_overhead(baseline_metrics['size'], metrics['size'])
                    size_overheads.append(overhead)
                    
                # Text section size overhead
                if 'size.__text' in metrics and 'size.__text' in baseline_metrics:
                    overhead = calculate_overhead(baseline_metrics['size.__text'], metrics['size.__text'])
                    text_size_overheads.append(overhead)
        
        if compile_overheads or size_overheads:
            analysis[pass_name] = {}
            
            if compile_overheads:
                analysis[pass_name]['compile_time'] = {
                    'mean_overhead': statistics.mean(compile_overheads),
                    'median_overhead': statistics.median(compile_overheads),
                    'min_overhead': min(compile_overheads),
                    'max_overhead': max(compile_overheads),
                    'stdev_overhead': statistics.stdev(compile_overheads) if len(compile_overheads) > 1 else 0,
                    'num_benchmarks': len(compile_overheads)
                }
            
            if size_overheads:
                analysis[pass_name]['binary_size'] = {
                    'mean_overhead': statistics.mean(size_overheads),
                    'median_overhead': statistics.median(size_overheads),
                    'min_overhead': min(size_overheads),
                    'max_overhead': max(size_overheads),
                    'stdev_overhead': statistics.stdev(size_overheads) if len(size_overheads) > 1 else 0,
                    'num_benchmarks': len(size_overheads)
                }
                
            if text_size_overheads:
                analysis[pass_name]['text_size'] = {
                    'mean_overhead': statistics.mean(text_size_overheads),
                    'median_overhead': statistics.median(text_size_overheads),
                    'min_overhead': min(text_size_overheads),
                    'max_overhead': max(text_size_overheads),
                    'stdev_overhead': statistics.stdev(text_size_overheads) if len(text_size_overheads) > 1 else 0,
                    'num_benchmarks': len(text_size_overheads)
                }
    
    return analysis

def print_analysis(analysis):
    """Print analysis results in formatted tables"""
    print("\n=== KoviD Obfuscation Passes Performance Impact ===\n")
    
    # Compilation time overhead
    print("=== Compilation Time Overhead ===")
    print(f"{'Pass Name':<20} {'Mean %':<10} {'Median %':<10} {'Min %':<10} {'Max %':<10} {'StdDev':<10} {'# Tests':<10}")
    print("-" * 80)
    
    for pass_name, metrics in sorted(analysis.items()):
        if 'compile_time' in metrics:
            stats = metrics['compile_time']
            print(f"{pass_name:<20} "
                  f"{stats['mean_overhead']:<10.2f} "
                  f"{stats['median_overhead']:<10.2f} "
                  f"{stats['min_overhead']:<10.2f} "
                  f"{stats['max_overhead']:<10.2f} "
                  f"{stats['stdev_overhead']:<10.2f} "
                  f"{stats['num_benchmarks']:<10d}")
    
    # Binary size overhead
    print("\n=== Binary Size Overhead ===")
    print(f"{'Pass Name':<20} {'Mean %':<10} {'Median %':<10} {'Min %':<10} {'Max %':<10} {'StdDev':<10} {'# Tests':<10}")
    print("-" * 80)
    
    for pass_name, metrics in sorted(analysis.items()):
        if 'binary_size' in metrics:
            stats = metrics['binary_size']
            print(f"{pass_name:<20} "
                  f"{stats['mean_overhead']:<10.2f} "
                  f"{stats['median_overhead']:<10.2f} "
                  f"{stats['min_overhead']:<10.2f} "
                  f"{stats['max_overhead']:<10.2f} "
                  f"{stats['stdev_overhead']:<10.2f} "
                  f"{stats['num_benchmarks']:<10d}")
    
    # Text section size overhead
    print("\n=== Text Section Size Overhead ===")
    print(f"{'Pass Name':<20} {'Mean %':<10} {'Median %':<10} {'Min %':<10} {'Max %':<10} {'StdDev':<10} {'# Tests':<10}")
    print("-" * 80)
    
    for pass_name, metrics in sorted(analysis.items()):
        if 'text_size' in metrics:
            stats = metrics['text_size']
            print(f"{pass_name:<20} "
                  f"{stats['mean_overhead']:<10.2f} "
                  f"{stats['median_overhead']:<10.2f} "
                  f"{stats['min_overhead']:<10.2f} "
                  f"{stats['max_overhead']:<10.2f} "
                  f"{stats['stdev_overhead']:<10.2f} "
                  f"{stats['num_benchmarks']:<10d}")
    
    print("\n=== Summary ===")
    if analysis:
        # Find lowest/highest for each metric
        if any('compile_time' in m for m in analysis.values()):
            compile_times = [(p, m['compile_time']['mean_overhead']) for p, m in analysis.items() if 'compile_time' in m]
            print(f"Compile time - Lowest overhead:  {min(compile_times, key=lambda x: x[1])[0]}")
            print(f"Compile time - Highest overhead: {max(compile_times, key=lambda x: x[1])[0]}")
        
        if any('binary_size' in m for m in analysis.values()):
            binary_sizes = [(p, m['binary_size']['mean_overhead']) for p, m in analysis.items() if 'binary_size' in m]
            print(f"Binary size - Lowest overhead:   {min(binary_sizes, key=lambda x: x[1])[0]}")
            print(f"Binary size - Highest overhead:  {max(binary_sizes, key=lambda x: x[1])[0]}")

def generate_latex_tables(analysis):
    """Generate LaTeX tables for the paper"""
    print("\n=== LaTeX Tables ===\n")
    
    # Compilation time table
    print("% Compilation Time Overhead")
    print("\\begin{table}[h]")
    print("\\centering")
    print("\\caption{Compilation Time Overhead of Obfuscation Passes}")
    print("\\begin{tabular}{|l|r|r|r|r|}")
    print("\\hline")
    print("Pass Name & Mean \\% & Median \\% & Min \\% & Max \\% \\\\")
    print("\\hline")
    
    for pass_name, metrics in sorted(analysis.items()):
        if 'compile_time' in metrics:
            stats = metrics['compile_time']
            print(f"{pass_name.replace('_', '\\_')} & "
                  f"{stats['mean_overhead']:.2f} & "
                  f"{stats['median_overhead']:.2f} & "
                  f"{stats['min_overhead']:.2f} & "
                  f"{stats['max_overhead']:.2f} \\\\")
    
    print("\\hline")
    print("\\end{tabular}")
    print("\\label{tab:compilation-overhead}")
    print("\\end{table}")
    
    # Binary size table
    print("\n% Binary Size Overhead")
    print("\\begin{table}[h]")
    print("\\centering")
    print("\\caption{Binary Size Overhead of Obfuscation Passes}")
    print("\\begin{tabular}{|l|r|r|r|r|}")
    print("\\hline")
    print("Pass Name & Mean \\% & Median \\% & Min \\% & Max \\% \\\\")
    print("\\hline")
    
    for pass_name, metrics in sorted(analysis.items()):
        if 'binary_size' in metrics:
            stats = metrics['binary_size']
            print(f"{pass_name.replace('_', '\\_')} & "
                  f"{stats['mean_overhead']:.2f} & "
                  f"{stats['median_overhead']:.2f} & "
                  f"{stats['min_overhead']:.2f} & "
                  f"{stats['max_overhead']:.2f} \\\\")
    
    print("\\hline")
    print("\\end{tabular}")
    print("\\label{tab:binary-size-overhead}")
    print("\\end{table}")
    
    # Text section size table
    print("\n% Text Section Size Overhead")
    print("\\begin{table}[h]")
    print("\\centering")
    print("\\caption{Text Section Size Overhead of Obfuscation Passes}")
    print("\\begin{tabular}{|l|r|r|r|r|}")
    print("\\hline")
    print("Pass Name & Mean \\% & Median \\% & Min \\% & Max \\% \\\\")
    print("\\hline")
    
    for pass_name, metrics in sorted(analysis.items()):
        if 'text_size' in metrics:
            stats = metrics['text_size']
            print(f"{pass_name.replace('_', '\\_')} & "
                  f"{stats['mean_overhead']:.2f} & "
                  f"{stats['median_overhead']:.2f} & "
                  f"{stats['min_overhead']:.2f} & "
                  f"{stats['max_overhead']:.2f} \\\\")
    
    print("\\hline")
    print("\\end{tabular}")
    print("\\label{tab:text-size-overhead}")
    print("\\end{table}")

def main():
    parser = argparse.ArgumentParser(description='Compare obfuscation pass benchmark results')
    parser.add_argument('results', nargs='+', help='Result JSON files to compare')
    parser.add_argument('--latex', action='store_true', help='Generate LaTeX tables')
    
    args = parser.parse_args()
    
    # Filter to only JSON files
    json_files = [f for f in args.results if f.endswith('.json') and not f.endswith('.json.run1') and not f.endswith('.json.run2') and not f.endswith('.json.run3')]
    
    if not json_files:
        print("Error: No JSON result files found")
        sys.exit(1)
    
    # Analyze results
    analysis = analyze_results(json_files)
    
    # Print analysis
    print_analysis(analysis)
    
    # Generate LaTeX tables if requested
    if args.latex:
        generate_latex_tables(analysis)

if __name__ == '__main__':
    main()