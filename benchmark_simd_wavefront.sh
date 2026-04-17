#!/usr/bin/env bash
# Headless Cornell-box benchmark: scalar OpenMP vs SIMD packets vs wavefront.
# Replaces the old stub benchmark_simd.sh pattern with a real timed run.

set -euo pipefail
cd "$(dirname "$0")"

mkdir -p benchmark_results
LOG="benchmark_results/simd_wavefront_scalar_$(date +%Y%m%d_%H%M%S).log"

echo "Building bench_cpu_render_modes..." | tee "$LOG"
make bench-cpu-modes 2>&1 | tee -a "$LOG"

echo "" | tee -a "$LOG"
echo "Running default suite (800px wide, 4 samples, 5 timed reps, 2 warmup)..." | tee -a "$LOG"
./build/bench_cpu_render_modes -w 800 -s 4 -r 5 --warmup 2 2>&1 | tee -a "$LOG"

echo "" | tee -a "$LOG"
echo "Wrote $LOG"
