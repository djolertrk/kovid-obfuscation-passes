#!/usr/bin/env python3

import json
import sys
import re
from collections import defaultdict

def parse_lit_output(filename):
    """Parse lit JSON output file and extract metrics."""
    results = defaultdict(dict)
    
    try:
        with open(filename, 'r') as f:
            data = json.load(f)
            
        # Parse lit's JSON format
        if 'tests' in data:
            for test in data['tests']:
                if test['code'] == 'PASS' and 'metrics' in test:
                    # Extract benchmark name from test path
                    test_name = test['name']
                    bench_name = test_name.split('/')[-2] if '/' in test_name else test_name
                    
                    # Store metrics
                    results[bench_name] = test['metrics']
    
    except Exception as e:
        print(f"Error parsing {filename}: {e}", file=sys.stderr)
    
    return results

def combine_results(filenames):
    """Combine results from multiple runs."""
    all_results = defaultdict(lambda: defaultdict(list))
    
    for filename in filenames:
        run_results = parse_lit_output(filename)
        
        for bench, metrics in run_results.items():
            for metric, value in metrics.items():
                if isinstance(value, (int, float)):
                    all_results[bench][metric].append(value)
    
    # Calculate averages
    combined = {}
    for bench, metrics in all_results.items():
        combined[bench] = {}
        for metric, values in metrics.items():
            if values:
                combined[bench][metric] = {
                    'mean': sum(values) / len(values),
                    'min': min(values),
                    'max': max(values),
                    'values': values
                }
    
    return combined

def main():
    if len(sys.argv) < 2:
        print("Usage: combine_results.py <result_file1> <result_file2> ...", file=sys.stderr)
        sys.exit(1)
    
    results = combine_results(sys.argv[1:])
    print(json.dumps(results, indent=2))

if __name__ == "__main__":
    main()