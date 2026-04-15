#!/bin/bash
# GPU Ray Tracer - Screenshot Comparison Script
# Captures comparison screenshots across all quality presets and phases

set -e

# Configuration
SCREENSHOT_DIR="screenshots/comparisons"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BUILD_DIR="build"
RAYTRACER="$BUILD_DIR/raytracer_interactive_gpu"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create screenshot directory
mkdir -p "$SCREENSHOT_DIR"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}GPU Ray Tracer - Screenshot Comparisons${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if ray tracer exists
if [ ! -f "$RAYTRACER" ]; then
    echo -e "${RED}Error: Ray tracer not found at $RAYTRACER${NC}"
    echo -e "${YELLOW}Building ray tracer first...${NC}"
    make interactive-gpu
fi

# Function to build and capture
build_and_capture() {
    local preset_name=$1
    local make_target=$2
    local screenshot_name=$3
    local description=$4

    echo -e "${GREEN}Building: $preset_name${NC}"
    echo -e "${YELLOW}  Description: $description${NC}"

    # Build the preset
    make $make_target > /dev/null 2>&1

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  ✓ Build successful${NC}"
        echo ""
        echo -e "${YELLOW}  Instructions for screenshot capture:${NC}"
        echo -e "  1. Run: ${GREEN}./$RAYTRACER${NC}"
        echo -e "  2. Position camera for good view"
        echo -e "  3. Press ${GREEN}S${NC} to save screenshot"
        echo -e "  4. Screenshot will be saved to: ${GREEN}screenshots/${NC}"
        echo -e "  5. Move screenshot to: ${GREEN}$SCREENSHOT_DIR/${screenshot_name}.png${NC}"
        echo ""
        echo -e "${YELLOW}  Press enter when ready to continue...${NC}"
        read

        # Check if screenshot was captured
        if ls screenshots/screenshot_*.png 1> /dev/null 2>&1; then
            LATEST=$(ls -t screenshots/screenshot_*.png | head -n1)
            mv "$LATEST" "$SCREENSHOT_DIR/${screenshot_name}.png"
            echo -e "${GREEN}  ✓ Screenshot saved: ${screenshot_name}.png${NC}"
        else
            echo -e "${RED}  ✗ No screenshot found${NC}"
        fi
    else
        echo -e "${RED}  ✗ Build failed${NC}"
    fi
    echo ""
}

# Build all presets and capture screenshots
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Phase 1: Baseline PBR${NC}"
echo -e "${BLUE}========================================${NC}"
build_and_capture \
    "Baseline (PBR Only)" \
    "gpu-fast" \
    "01_baseline_pbr" \
    "Basic PBR lighting without advanced features"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Phase 1.5: Tone Mapping${NC}"
echo -e "${BLUE}========================================${NC}"
build_and_capture \
    "PBR + Tone Mapping" \
    "gpu-interactive" \
    "02_pbr_tone_mapping" \
    "PBR with ACES tone mapping and gamma correction"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Phase 2: Soft Shadows & AO${NC}"
echo -e "${BLUE}========================================${NC}"
build_and_capture \
    "PBR + Soft Shadows + AO" \
    "gpu-production" \
    "03_pbr_soft_shadows_ao" \
    "Enhanced realism with soft shadows and ambient occlusion"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Phase 3: Global Illumination${NC}"
echo -e "${BLUE}========================================${NC}"
build_and_capture \
    "GI Fast" \
    "gi-fast" \
    "04_gi_fast" \
    "Fast GI for testing (2 samples, low intensity)"

build_and_capture \
    "GI Balanced" \
    "gi-balanced" \
    "05_gi_balanced" \
    "Balanced GI for interactive use (4 samples, medium intensity)"

build_and_capture \
    "GI Quality" \
    "gi-quality" \
    "06_gi_quality" \
    "Maximum quality GI (8 samples, high intensity)"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Phase 3.5: Advanced Reflections${NC}"
echo -e "${BLUE}========================================${NC}"
build_and_capture \
    "SSR Fast" \
    "ssr-fast" \
    "07_ssr_fast" \
    "Fast screen-space reflections (8 samples)"

build_and_capture \
    "SSR Quality" \
    "ssr-quality" \
    "08_ssr_quality" \
    "High-quality screen-space reflections (24 samples)"

build_and_capture \
    "Environment Mapping" \
    "env-quality" \
    "09_env_mapping" \
    "Procedural sky and environment lighting"

build_and_capture \
    "Phase 3.5 Complete" \
    "phase35-complete" \
    "10_phase35_complete" \
    "All features: GI + SSR + Environment Mapping"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Screenshot Capture Complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${GREEN}All screenshots saved to: $SCREENSHOT_DIR/${NC}"
echo ""
echo -e "${YELLOW}To create comparison montage:${NC}"
echo -e "  Use ImageMagick or similar tool to combine images"
echo -e "  Example: montage -tile 5x2 -geometry 800x450+2+2 $SCREENSHOT_DIR/*.png comparison.png"
echo ""
