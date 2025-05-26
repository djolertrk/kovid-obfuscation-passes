#!/usr/bin/env python3

"""
Script to measure runtime performance of benchmarks
Usage: python3 measure_runtime.py [benchmark_name]
"""

import os
import sys
import subprocess
import json
import statistics
from datetime import datetime

# Benchmark configurations
BENCHMARKS = {
    '7zip': {
        'test_file': 'CTMark/7zip/7zip-benchmark.test',
        'name': '7zip-benchmark'
    },
    'Bullet': {
        'test_file': 'CTMark/Bullet/bullet.test',
        'name': 'bullet'
    },
    'ClamAV': {
        'test_file': 'CTMark/ClamAV/clamscan.test',
        'name': 'clamscan'
    },
    'sqlite3': {
        'test_file': 'CTMark/sqlite3/sqlite3.test',
        'name': 'sqlite3'
    },
    'SPASS': {
        'test_file': 'CTMark/SPASS/SPASS.test',
        'name': 'SPASS'
    },
    'consumer-typeset': {
        'test_file': 'CTMark/consumer-typeset/consumer-typeset.test',
        'name': 'consumer-typeset'
    },
    'kimwitu++': {
        'test_file': 'CTMark/kimwitu++/kc.test',
        'name': 'kc'
    },
    'lencod': {
        'test_file': 'CTMark/lencod/lencod.test',
        'name': 'lencod'
    },
    'mafft': {
        'test_file': 'CTMark/mafft/pairlocalalign.test',
        'name': 'pairlocalalign'
    },
    'tramp3d-v4': {
        'test_file': 'CTMark/tramp3d-v4/tramp3d-v4.test',
        'name': 'tramp3d-v4'
    }
}

BUILD_DIR = '/Users/djtodorovic/projects/kovid/kovid-obfustaion-passes/benchmarks/builds/build-baseline'
NUM_RUNS = 3

def run_benchmark_with_lit(benchmark_name, test_file):
    """Run a benchmark using lit and extract exec_time metric"""
    cmd = ['lit', '-v', test_file]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=BUILD_DIR)
        
        # Parse the output to extract exec_time
        output = result.stdout
        for line in output.split('\n'):
            if 'exec_time:' in line:
                # Extract the time value
                time_str = line.split('exec_time:')[1].strip()
                return float(time_str)
        
        # If exec_time not found, return None
        return None
        
    except Exception as e:
        print(f"Error running benchmark {benchmark_name}: {e}")
        return None

def measure_benchmark(benchmark_name):
    """Measure a single benchmark multiple times"""
    if benchmark_name not in BENCHMARKS:
        print(f"Unknown benchmark: {benchmark_name}")
        return None
    
    config = BENCHMARKS[benchmark_name]
    test_file = config['test_file']
    
    print(f"Measuring {benchmark_name}...")
    times = []
    
    for i in range(NUM_RUNS):
        print(f"  Run {i+1}/{NUM_RUNS}...", end='', flush=True)
        exec_time = run_benchmark_with_lit(benchmark_name, test_file)
        
        if exec_time is not None:
            times.append(exec_time)
            print(f" {exec_time:.3f}s")
        else:
            print(" Failed")
    
    if len(times) > 0:
        return {
            'benchmark': benchmark_name,
            'times': times,
            'mean': statistics.mean(times),
            'median': statistics.median(times),
            'stdev': statistics.stdev(times) if len(times) > 1 else 0.0,
            'min': min(times),
            'max': max(times)
        }
    else:
        return None

def main():
    if len(sys.argv) > 1:
        # Run specific benchmark
        benchmark = sys.argv[1]
        result = measure_benchmark(benchmark)
        if result:
            print(f"\nResults for {benchmark}:")
            print(f"  Mean:   {result['mean']:.3f}s")
            print(f"  Median: {result['median']:.3f}s")
            print(f"  StdDev: {result['stdev']:.3f}s")
            print(f"  Min:    {result['min']:.3f}s")
            print(f"  Max:    {result['max']:.3f}s")
    else:
        # Run all benchmarks
        results = []
        for benchmark in BENCHMARKS:
            result = measure_benchmark(benchmark)
            if result:
                results.append(result)
        
        # Save results
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = f'runtime_results_{timestamp}.json'
        with open(filename, 'w') as f:
            json.dump(results, f, indent=2)
        
        print(f"\nResults saved to: {filename}")
        
        # Print summary
        print("\nSummary:")
        print(f"{'Benchmark':<20} {'Mean (s)':<10} {'Median (s)':<10} {'StdDev (s)':<10}")
        print("-" * 50)
        for result in results:
            print(f"{result['benchmark']:<20} {result['mean']:<10.3f} {result['median']:<10.3f} {result['stdev']:<10.3f}")

if __name__ == '__main__':
    main()