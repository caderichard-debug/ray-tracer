#!/bin/bash
# Benchmark script for CPU ray tracer

echo "=== CPU Ray Tracer Benchmark ==="
echo "Scene: Cornell Box"
echo "Resolution: 800x450"
echo "Samples: 16"
echo "Max Depth: 5"
echo ""

# Clean build to ensure fresh compile
echo "Building..."
make clean > /dev/null 2>&1
make 2>&1 | grep -E "(warning|error)" | head -5

BINARY="build/raytracer_phase2"
if [ ! -f ./$BINARY ]; then
    echo "ERROR: Build failed - $BINARY not found"
    exit 1
fi

echo ""
echo "Running benchmark (3 iterations)..."
echo ""

# Run 3 times and take average
total_time=0
iterations=3

for i in 1 2 3; do
    echo "Iteration $i..."
    start=$(gdate +%s.%N)
    ./$BINARY > /dev/null 2>&1
    end=$(gdate +%s.%N)

    elapsed=$(echo "$end - $start" | bc)
    total_time=$(echo "$total_time + $elapsed" | bc)

    echo "  Time: ${elapsed}s"
done

# Calculate average
avg_time=$(echo "scale=3; $total_time / $iterations" | bc)

echo ""
echo "=== Results ==="
echo "Average time: ${avg_time}s"
echo ""

# Calculate MRays/sec (approximate)
# 800 * 450 * 16 samples = 5,760,000 rays
rays=5760000
mray_sec=$(echo "scale=2; $rays / ($avg_time * 1000000)" | bc)
echo "Throughput: ${mray_sec} MRays/sec"
