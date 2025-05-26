#!/usr/bin/env python3

"""
Compare runtime performance of obfuscated binaries
Analyzes execution time overhead from different obfuscation passes
"""

import json
import sys
import os
import statistics
import argparse

def load_results(filename):
    """Load runtime benchmark results from JSON file"""
    with open(filename, 'r') as f:
        return json.load(f)

def calculate_overhead(baseline_time, obfuscated_time):
    """Calculate percentage overhead"""
    if baseline_time == 0:
        return 0
    return ((obfuscated_time - baseline_time) / baseline_time) * 100

def analyze_results(results_files):
    """Analyze runtime benchmark results and calculate overheads"""
    # Load all results
    all_results = {}
    for filename in results_files:
        pass_name = os.path.basename(filename).split('_')[0]
        all_results[pass_name] = load_results(filename)
    
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
        
        overheads = []
        detailed_results = {}
        
        for bench_name, bench_data in results.items():
            if bench_name in baseline and 'runtime' in bench_data and 'runtime' in baseline[bench_name]:
                baseline_time = baseline[bench_name]['runtime']['mean']
                obfuscated_time = bench_data['runtime']['mean']
                
                overhead = calculate_overhead(baseline_time, obfuscated_time)
                overheads.append(overhead)
                
                detailed_results[bench_name] = {
                    'baseline_time': baseline_time,
                    'obfuscated_time': obfuscated_time,
                    'overhead': overhead
                }
        
        if overheads:
            analysis[pass_name] = {
                'mean_overhead': statistics.mean(overheads),
                'median_overhead': statistics.median(overheads),
                'min_overhead': min(overheads),
                'max_overhead': max(overheads),
                'stdev_overhead': statistics.stdev(overheads) if len(overheads) > 1 else 0,
                'num_benchmarks': len(overheads),
                'detailed_results': detailed_results
            }
    
    return analysis

def print_analysis(analysis):
    """Print analysis results in a formatted table"""
    print("\n=== KoviD Obfuscation Passes Runtime Performance Impact ===\n")
    
    # Summary table
    print("=== Runtime Overhead Summary ===")
    print(f"{'Pass Name':<20} {'Mean %':<10} {'Median %':<10} {'Min %':<10} {'Max %':<10} {'StdDev':<10} {'# Tests':<10}")
    print("-" * 80)
    
    for pass_name, stats in sorted(analysis.items()):
        print(f"{pass_name:<20} "
              f"{stats['mean_overhead']:<10.2f} "
              f"{stats['median_overhead']:<10.2f} "
              f"{stats['min_overhead']:<10.2f} "
              f"{stats['max_overhead']:<10.2f} "
              f"{stats['stdev_overhead']:<10.2f} "
              f"{stats['num_benchmarks']:<10d}")
    
    # Detailed results for each pass
    print("\n=== Detailed Results by Benchmark ===")
    for pass_name, stats in sorted(analysis.items()):
        print(f"\n{pass_name}:")
        print(f"{'Benchmark':<20} {'Baseline (s)':<15} {'Obfuscated (s)':<15} {'Overhead %':<10}")
        print("-" * 60)
        
        for bench_name, results in sorted(stats['detailed_results'].items()):
            print(f"{bench_name:<20} "
                  f"{results['baseline_time']:<15.4f} "
                  f"{results['obfuscated_time']:<15.4f} "
                  f"{results['overhead']:<10.2f}")
    
    print("\n=== Summary ===")
    if analysis:
        runtime_overheads = [(p, s['mean_overhead']) for p, s in analysis.items()]
        print(f"Lowest runtime overhead:  {min(runtime_overheads, key=lambda x: x[1])[0]} ({min(runtime_overheads, key=lambda x: x[1])[1]:.2f}%)")
        print(f"Highest runtime overhead: {max(runtime_overheads, key=lambda x: x[1])[0]} ({max(runtime_overheads, key=lambda x: x[1])[1]:.2f}%)")

def generate_latex_table(analysis):
    """Generate LaTeX table for the paper"""
    print("\n=== LaTeX Table ===\n")
    print("\\begin{table}[h]")
    print("\\centering")
    print("\\caption{Runtime Performance Overhead of Obfuscation Passes}")
    print("\\begin{tabular}{|l|r|r|r|r|}")
    print("\\hline")
    print("Pass Name & Mean \\% & Median \\% & Min \\% & Max \\% \\\\")
    print("\\hline")
    
    for pass_name, stats in sorted(analysis.items()):
        print(f"{pass_name.replace('_', '\\_')} & "
              f"{stats['mean_overhead']:.2f} & "
              f"{stats['median_overhead']:.2f} & "
              f"{stats['min_overhead']:.2f} & "
              f"{stats['max_overhead']:.2f} \\\\")
    
    print("\\hline")
    print("\\end{tabular}")
    print("\\label{tab:runtime-overhead}")
    print("\\end{table}")

def generate_csv(analysis, output_file):
    """Generate CSV file with results"""
    import csv
    
    with open(output_file, 'w', newline='') as csvfile:
        # Write summary
        writer = csv.writer(csvfile)
        writer.writerow(['Pass Name', 'Mean %', 'Median %', 'Min %', 'Max %', 'StdDev', '# Tests'])
        
        for pass_name, stats in sorted(analysis.items()):
            writer.writerow([
                pass_name,
                f"{stats['mean_overhead']:.2f}",
                f"{stats['median_overhead']:.2f}",
                f"{stats['min_overhead']:.2f}",
                f"{stats['max_overhead']:.2f}",
                f"{stats['stdev_overhead']:.2f}",
                stats['num_benchmarks']
            ])
        
        # Write detailed results
        writer.writerow([])
        writer.writerow(['Detailed Results'])
        writer.writerow(['Pass', 'Benchmark', 'Baseline (s)', 'Obfuscated (s)', 'Overhead %'])
        
        for pass_name, stats in sorted(analysis.items()):
            for bench_name, results in sorted(stats['detailed_results'].items()):
                writer.writerow([
                    pass_name,
                    bench_name,
                    f"{results['baseline_time']:.4f}",
                    f"{results['obfuscated_time']:.4f}",
                    f"{results['overhead']:.2f}"
                ])
    
    print(f"\nResults saved to: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Compare runtime performance of obfuscated binaries')
    parser.add_argument('results', nargs='+', help='Runtime result JSON files to compare')
    parser.add_argument('--latex', action='store_true', help='Generate LaTeX table')
    parser.add_argument('--csv', help='Export results to CSV file')
    
    args = parser.parse_args()
    
    # Analyze results
    analysis = analyze_results(args.results)
    
    # Print analysis
    print_analysis(analysis)
    
    # Generate LaTeX table if requested
    if args.latex:
        generate_latex_table(analysis)
    
    # Export to CSV if requested
    if args.csv:
        generate_csv(analysis, args.csv)

if __name__ == '__main__':
    main()