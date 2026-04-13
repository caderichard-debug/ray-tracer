#!/bin/bash
set -e

echo "=========================================="
echo "  COMPREHENSIVE RAY TRACER BENCHMARK"
echo "=========================================="
echo ""

mkdir -p benchmark_results
rm -f benchmark_results/performance.log

echo "# Ray Tracer Performance Benchmark" > benchmark_results/performance.log
echo "========================================" >> benchmark_results/performance.log
echo ""

# Build and run Phase 1
echo "Building Phase 1..."
if $(MAKE) phase1 > /dev/null 2>&1; then
    echo "✓ Phase 1 built"
    echo ""
    echo "## Phase 1: Scalar Foundation" >> benchmark_results/performance.log
    echo "Running Phase 1 benchmark..."
    $(BUILD_DIR)//raytracer_phase1 > benchmark_results/phase1_output.ppm 2>&1
    echo "✓ Phase 1 complete: benchmark_results/phase1_output.ppm"
    echo ""
else
    echo "⚠️  Phase 1 build failed"
    echo "Phase 1: Not available" >> benchmark_results/performance.log
    echo ""
fi

# Build and run Phase 2
echo "Building Phase 2..."
if $(MAKE) phase2 > /dev/null 2>&1; then
    echo "✓ Phase 2 built"
    echo ""
    echo "## Phase 2: Basic Rendering" >> benchmark_results/performance.log
    echo "Running Phase 2 benchmark..."
    $(BUILD_DIR)//raytracer_phase2 > benchmark_results/phase2_output.ppm 2>&1
    echo "✓ Phase 2 complete: benchmark_results/phase2_output.ppm"
    echo ""
else
    echo "⚠️  Phase 2 build failed"
    echo ""
fi

echo "=========================================="
echo "  BENCHMARK COMPLETE"
echo "=========================================="
echo ""
echo "Results:"
echo "  📊 Performance log:  benchmark_results/performance.log"
echo "  🖼️  Phase 1 output:   benchmark_results/phase1_output.ppm"
echo "  🖼️  Phase 2 output:   benchmark_results/phase2_output.ppm"
echo ""

# Show performance summary
echo "Performance Summary:"
echo ""
if [ -f benchmark_results/phase1_output.ppm ]; then
    echo "Phase 1:"
    grep -A 10 "PERFORMANCE REPORT" benchmark_results/phase1_output.ppm | tail -n 7 | sed 's/^/  /'
    echo ""
fi

if [ -f benchmark_results/phase2_output.ppm ]; then
    echo "Phase 2:"
    grep -A 10 "PERFORMANCE REPORT" benchmark_results/phase2_output.ppm | tail -n 7 | sed 's/^/  /'
    echo ""
fi
