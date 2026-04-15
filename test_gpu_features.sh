#!/bin/bash
# GPU Feature Combination Test Script
# Tests all toggleable GPU features to ensure nothing breaks

echo "=============================================="
echo "GPU Feature Combination Testing"
echo "=============================================="

# Function to test a specific configuration
test_config() {
    local pbr=$1
    local lights=$2
    local tone=$3
    local gamma=$4
    local scene=$5
    local description="$6"

    echo "Testing: $description"
    echo "  PBR: $pbr, Lights: $lights, Tone: $tone, Gamma: $gamma, Scene: $scene"

    # Build with configuration
    make clean > /dev/null 2>&1
    make interactive-gpu \
        ENABLE_GPU_PBR=$pbr \
        ENABLE_GPU_MULTIPLE_LIGHTS=$lights \
        ENABLE_GPU_TONE_MAPPING=$tone \
        ENABLE_GPU_GAMMA_CORRECTION=$gamma \
        GPU_SCENE=$scene \
        > /tmp/gpu_build.log 2>&1

    if [ $? -eq 0 ]; then
        echo "  ✓ Build successful"
        return 0
    else
        echo "  ✗ Build failed"
        tail -5 /tmp/gpu_build.log
        return 1
    fi
}

# Test counter
total_tests=0
passed_tests=0

echo ""
echo "=== Testing Feature Independence ==="

# Test 1: Baseline (all features disabled)
total_tests=$((total_tests + 1))
if test_config 0 0 0 0 cornell_box "Baseline (Phong only, single light, no tone mapping, no gamma)"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 2: PBR only
total_tests=$((total_tests + 1))
if test_config 1 0 0 0 cornell_box "PBR only"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 3: Multiple lights only
total_tests=$((total_tests + 1))
if test_config 0 1 0 0 cornell_box "Multiple lights only"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 4: Tone mapping only
total_tests=$((total_tests + 1))
if test_config 0 0 1 0 cornell_box "Tone mapping only"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 5: Gamma correction only
total_tests=$((total_tests + 1))
if test_config 0 0 0 1 cornell_box "Gamma correction only"; then
    passed_tests=$((passed_tests + 1))
fi

echo ""
echo "=== Testing Feature Combinations ==="

# Test 6: PBR + Multiple lights
total_tests=$((total_tests + 1))
if test_config 1 1 0 0 cornell_box "PBR + Multiple lights"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 7: PBR + Tone mapping
total_tests=$((total_tests + 1))
if test_config 1 0 1 0 cornell_box "PBR + Tone mapping"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 8: All features enabled
total_tests=$((total_tests + 1))
if test_config 1 1 1 1 cornell_box "All features enabled"; then
    passed_tests=$((passed_tests + 1))
fi

echo ""
echo "=== Testing Scene Selection ==="

# Test 9: GPU demo scene (baseline)
total_tests=$((total_tests + 1))
if test_config 0 0 0 0 gpu_demo "GPU demo scene (baseline)"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 10: GPU demo scene (all features)
total_tests=$((total_tests + 1))
if test_config 1 1 1 1 gpu_demo "GPU demo scene (all features)"; then
    passed_tests=$((passed_tests + 1))
fi

echo ""
echo "=== Testing Edge Cases ==="

# Test 11: Tone mapping without gamma
total_tests=$((total_tests + 1))
if test_config 1 1 1 0 cornell_box "Tone mapping without gamma"; then
    passed_tests=$((passed_tests + 1))
fi

# Test 12: Gamma without tone mapping
total_tests=$((total_tests + 1))
if test_config 1 1 0 1 cornell_box "Gamma without tone mapping"; then
    passed_tests=$((passed_tests + 1))
fi

echo ""
echo "=============================================="
echo "Test Results: $passed_tests / $total_tests passed"
echo "=============================================="

if [ $passed_tests -eq $total_tests ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
