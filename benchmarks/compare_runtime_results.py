#!/usr/bin/env python3

"""
Compare runtime performance of obfuscated binaries
Analyzes execution time overhead from different obfuscation passes
"""

import json
import sys
import os
import re
import statistics
import argparse
import logging

# Set up logging
logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)
logger = logging.getLogger('runtime_analyzer')

def fix_json_content(content):
    """Fix issues with JSON content related to decimal separators"""
    # Skip empty content
    if not content or content.strip() in ['{}', '']:
        return '{}'
    
    # Fix decimal separator issues (replace comma with period in numeric values)
    # Match both quoted and unquoted decimal values with commas
    content = re.sub(r':\s*(\d+),(\d+)', r': \1.\2', content)
    
    # Handle quoted number values
    content = re.sub(r':\s*"(\d+),(\d+)"', r': "\1.\2"', content)
    
    return content

def load_results(filename):
    """Load runtime benchmark results from JSON file with robust error handling"""
    try:
        # Check if file exists and is not empty
        if not os.path.exists(filename) or os.path.getsize(filename) == 0:
            logger.warning(f"File is empty or does not exist: {filename}")
            return {}

        with open(filename, 'r') as f:
            content = f.read()
            
            # Handle empty JSON files
            if content.strip() in ['{}', '']:
                logger.warning(f"File contains empty JSON: {filename}")
                return {}
            
            # Fix decimal separator issues
            fixed_content = fix_json_content(content)
            
            try:
                # Attempt to parse the fixed JSON
                result = json.loads(fixed_content)
                return result
            except json.JSONDecodeError as e:
                logger.error(f"Failed to parse fixed JSON from {filename}: {e}")
                logger.debug("Falling back to direct JSON loading...")
                
                # Try loading the original content as a fallback
                try:
                    with open(filename, 'r') as f_original:
                        return json.load(f_original)
                except json.JSONDecodeError as e2:
                    logger.error(f"Failed to parse original JSON: {e2}")
                    logger.error(f"First 200 chars of content: {content[:200]}")
                    return {}
                
    except Exception as e:
        logger.error(f"Unexpected error loading {filename}: {e}")
        return {}

def calculate_overhead(baseline_time, obfuscated_time):
    """Calculate percentage overhead"""
    try:
        # Convert to float if necessary
        baseline_time = float(baseline_time) if baseline_time else 0
        obfuscated_time = float(obfuscated_time) if obfuscated_time else 0
        
        if baseline_time == 0:
            return 0
        return ((obfuscated_time - baseline_time) / baseline_time) * 100
    except (TypeError, ValueError) as e:
        logger.error(f"Error calculating overhead: {e}")
        return 0

def analyze_results(results_files):
    """Analyze runtime benchmark results and calculate overheads"""
    # Load all results
    all_results = {}
    for filename in results_files:
        try:
            pass_name = os.path.basename(filename).split('_')[0]
            results = load_results(filename)
            
            if not results:
                logger.warning(f"No results found in {filename}")
                all_results[pass_name] = {}
            else:
                all_results[pass_name] = results
        except Exception as e:
            logger.error(f"Error processing {filename}: {e}")
            all_results[os.path.basename(filename)] = {}
    
    # Find baseline
    if 'baseline' not in all_results:
        logger.error("Error: No baseline results found")
        sys.exit(1)
    
    baseline = all_results['baseline']
    if not baseline:
        logger.error("Error: Baseline results are empty")
        sys.exit(1)
    
    # Calculate overheads for each pass
    analysis = {}
    for pass_name, results in all_results.items():
        if pass_name == 'baseline' or not results:
            continue
        
        overheads = []
        detailed_results = {}
        
        for bench_name, bench_data in results.items():
            try:
                if (bench_name in baseline and 
                    bench_data and 'runtime' in bench_data and 
                    baseline.get(bench_name, {}).get('runtime')):
                    
                    baseline_runtime = baseline[bench_name].get('runtime', {})
                    baseline_time = baseline_runtime.get('mean', 0)
                    
                    obfuscated_runtime = bench_data.get('runtime', {})
                    obfuscated_time = obfuscated_runtime.get('mean', 0)
                    
                    # Ensure we have numeric values
                    if baseline_time and obfuscated_time:
                        overhead = calculate_overhead(baseline_time, obfuscated_time)
                        overheads.append(overhead)
                        
                        detailed_results[bench_name] = {
                            'baseline_time': float(baseline_time),
                            'obfuscated_time': float(obfuscated_time),
                            'overhead': overhead
                        }
            except Exception as e:
                logger.error(f"Error analyzing {pass_name}/{bench_name}: {e}")
        
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
    
    if not analysis:
        print("No analysis results available. Check that your input files contain valid data.")
        return
    
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
        if runtime_overheads:
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
        # Use double backslash for proper LaTeX escaping
        escaped_name = pass_name.replace('_', '\\_')
        print(f"{escaped_name} & {stats['mean_overhead']:.2f} & {stats['median_overhead']:.2f} & {stats['min_overhead']:.2f} & {stats['max_overhead']:.2f} \\\\")
    
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
    parser.add_argument('--verbose', action='store_true', help='Enable verbose logging')
    
    args = parser.parse_args()
    
    # Set log level based on verbose flag
    if args.verbose:
        logger.setLevel(logging.DEBUG)
        logger.debug("Verbose logging enabled")
    
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