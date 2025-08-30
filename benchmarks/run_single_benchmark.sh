#!/bin/bash

# Script to run individual benchmarks and measure runtime performance
# Usage: ./run_single_benchmark.sh <benchmark_name>
# Example: ./run_single_benchmark.sh 7zip

if [ $# -eq 0 ]; then
    echo "Usage: $0 <benchmark_name>"
    echo "Available benchmarks: 7zip, Bullet, ClamAV, SPASS, consumer-typeset, kimwitu++, lencod, mafft, sqlite3, tramp3d-v4"
    exit 1
fi

BENCHMARK=$1
BUILD_DIR="/Users/djtodorovic/projects/kovid/kovid-obfustaion-passes/benchmarks/builds/build-baseline"

# Map benchmark names to executables
case $BENCHMARK in
    "7zip")
        EXEC="$BUILD_DIR/CTMark/7zip/7zip-benchmark"
        ARGS="b"
        ;;
    "Bullet")
        EXEC="$BUILD_DIR/CTMark/Bullet/bullet"
        ARGS="--benchmark 3"
        ;;
    "ClamAV")
        EXEC="$BUILD_DIR/CTMark/ClamAV/clamscan"
        cd "$BUILD_DIR/CTMark/ClamAV"
        ARGS="--database=dbdir --detect-pua --exclude-pua=Win.Packer --exclude-pua=Win.Trojan.Packed --exclude-pua=HTML.Jscript.Exploit --max-scansize=64M --max-filesize=16M --max-recursion=10 --max-files=500 -r inputs"
        ;;
    "sqlite3")
        EXEC="$BUILD_DIR/CTMark/sqlite3/sqlite3"
        cd "$BUILD_DIR/CTMark/sqlite3"
        ARGS="-init sqlite3rc :memory: < commands"
        ;;
    "SPASS")
        EXEC="$BUILD_DIR/CTMark/SPASS/SPASS"
        cd "$BUILD_DIR/CTMark/SPASS"
        ARGS="problem.dfg"
        ;;
    "consumer-typeset")
        EXEC="$BUILD_DIR/CTMark/consumer-typeset/consumer-typeset"
        cd "$BUILD_DIR/CTMark/consumer-typeset"
        ARGS="-x -I data/include -D data/fontdefs -F data/font -H data/hyph large.lout"
        ;;
    "kimwitu++")
        EXEC="$BUILD_DIR/CTMark/kimwitu++/kc"
        cd "$BUILD_DIR/CTMark/kimwitu++"
        ARGS="inputs/*.k"
        ;;
    "lencod")
        EXEC="$BUILD_DIR/CTMark/lencod/lencod"
        cd "$BUILD_DIR/CTMark/lencod"
        ARGS="-d data/encoder.cfg -p InputFile=data/foreman_part_qcif_444.yuv -p LeakyBucketRateFile=data/leakybucketrate.cfg -p QmatrixFile=data/q_matrix.cfg"
        ;;
    "mafft")
        EXEC="$BUILD_DIR/CTMark/mafft/pairlocalalign"
        cd "$BUILD_DIR/CTMark/mafft"
        ARGS="-i pyruvate_decarboxylase.fasta"
        ;;
    "tramp3d-v4")
        EXEC="$BUILD_DIR/CTMark/tramp3d-v4/tramp3d-v4"
        ARGS="--cartvis 1.0 0.0 --rhomin 1e-8 -n 50"
        ;;
    *)
        echo "Unknown benchmark: $BENCHMARK"
        exit 1
        ;;
esac

if [ ! -f "$EXEC" ]; then
    echo "Error: Executable not found: $EXEC"
    echo "Please run setup_benchmarks.sh first"
    exit 1
fi

echo "Running benchmark: $BENCHMARK"
echo "Command: $EXEC $ARGS"
echo "---"

# Run with time command to get runtime
if [ "$BENCHMARK" = "sqlite3" ]; then
    # Special handling for sqlite3 which needs input redirection
    time sh -c "$EXEC -init sqlite3rc :memory: < commands"
else
    time $EXEC $ARGS
fi