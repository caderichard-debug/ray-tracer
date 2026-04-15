#!/bin/bash
# Quick Showcase Scene Launcher
# Demonstrates different scenes with optimal settings

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}GPU Ray Tracer - Showcase Launcher${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build exists
if [ ! -f "$BUILD_DIR/raytracer_interactive_gpu" ]; then
    echo -e "${YELLOW}Building GPU ray tracer...${NC}"
    make phase4-complete > /dev/null 2>&1
    echo -e "${GREEN}✓ Build complete${NC}"
    echo ""
fi

echo "Select a showcase scene:"
echo ""
echo "1) Cornell Box (Classic Test Scene)"
echo "   - Best for: Color bleeding, reflections, glass"
echo "   - Features: Red/green walls, metallic spheres, glass sphere"
echo ""
echo "2) GPU Demo (PBR Material Showcase)"
echo "   - Best for: Material quality, roughness comparisons"
echo "   - Features: Roughness sweep, metallic sweep, gold/copper"
echo ""
echo "3) PBR Showcase (Complete Material Sweep)"
echo "   - Best for: Comprehensive material testing"
echo "   - Features: 7-sphere roughness sweep, metallic sweep"
echo ""
echo "4) Launch with Custom Scene"
echo "   - Use your own scene name"
echo ""

read -p "Enter choice (1-4): " choice

case $choice in
    1)
        scene="cornell_box"
        name="Cornell Box"
        ;;
    2)
        scene="gpu_demo"
        name="GPU Demo"
        ;;
    3)
        scene="pbr_showcase"
        name="PBR Showcase"
        ;;
    4)
        read -p "Enter scene name: " scene
        name="Custom ($scene)"
        ;;
    *)
        echo -e "${YELLOW}Using default: Cornell Box${NC}"
        scene="cornell_box"
        name="Cornell Box (Default)"
        ;;
esac

echo ""
echo -e "${GREEN}Selected: $name${NC}"
echo ""

# Check if scene exists
if ! grep -q "$scene" src/main_gpu_interactive.cpp; then
    echo -e "${YELLOW}Warning: Scene '$scene' may not exist${NC}"
    echo -e "${YELLOW}Available scenes: cornell_box, gpu_demo, pbr_showcase${NC}"
    echo ""
    read -p "Continue anyway? (y/n): " continue
    if [ "$continue" != "y" ]; then
        echo "Cancelled"
        exit 0
    fi
fi

echo -e "${BLUE}Building with: $name${NC}"
echo ""

# Build with selected scene (use GPU_SCENE not SCENE_NAME)
make clean > /dev/null 2>&1
make phase4-complete GPU_SCENE=$scene

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
    echo -e "${YELLOW}Launching application...${NC}"
    echo ""
    echo -e "${BLUE}Recommended Controls for Showcase:${NC}"
    echo -e "  ${GREEN}G${NC}      - Enable Global Illumination (color bleeding)"
    echo -e "  ${GREEN}Shift+S${NC} - Enable Screen-Space Reflections"
    echo -e "  ${GREEN}E${NC}       - Enable Environment Mapping"
    echo -e "  ${GREEN}O${NC}       - Enable SSAO (depth)"
    echo -e "  ${GREEN}V${NC}       - Enable Vignette (cinematic)"
    echo -e "  ${GREEN}N${NC}       - Enable Film Grain"
    echo -e "  ${GREEN}T${NC}       - Cycle Tone Mapping operators"
    echo -e "  ${GREEN}S${NC}       - Save Screenshot"
    echo ""
    echo -e "${YELLOW}Camera Tips:${NC}"
    case $scene in
        cornell_box)
            echo "  Position: (0, 1, 6) → Look at: (0, 1, 0) - Full room view"
            echo "  Position: (-2, 1, 4) → Look at: (0, 1, 0) - Color bleeding"
            echo "  Position: (0, 0.8, 2) → Look at: (0, 0.8, 0) - Close-up metals"
            ;;
        gpu_demo)
            echo "  Position: (-4, 1, 6) → Look at: (-1, 0.5, -2) - Roughness sweep"
            echo "  Position: (3, 1, 5) → Look at: (3, 0, 2) - Metallic sweep"
            echo "  Position: (6, 0.5, 4) → Look at: (6, 0.5, 0) - Gold sphere"
            ;;
        pbr_showcase)
            echo "  Position: (0, 2, 8) → Look at: (0, 0.5, 0) - Full overview"
            echo "  Position: (-2, 0.5, 5) → Look at: (-1.5, 0.5, 0) - Roughness sweep"
            echo "  Position: (3, 0.5, 5) → Look at: (3, 0, 0) - Metallic sweep"
            ;;
    esac
    echo ""
    read -p "Press Enter to launch..."
    echo ""

    cd "$SCRIPT_DIR"
    ./build/raytracer_interactive_gpu
else
    echo -e "${RED}Build failed!${NC}"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check scene name is correct"
    echo "  2. Verify dependencies: make deps"
    echo "  3. Try: make clean && make phase4-complete"
    exit 1
fi
