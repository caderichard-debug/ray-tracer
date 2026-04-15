#!/bin/bash

# ASCII Ray Tracer - Interactive Controls Demo
# This script demonstrates the WASD + arrow key controls

echo "=============================================="
echo "  ASCII RAY TRACER - INTERACTIVE CONTROLS"
echo "=============================================="
echo ""
echo "This demo will show you how to control the ASCII ray tracer"
echo "using WASD keys and arrow keys entirely in the terminal!"
echo ""
echo "BUILDING..."
echo ""

# Build the ASCII ray tracer
make ascii

echo ""
echo "=============================================="
echo "  QUICK CONTROL REFERENCE"
echo "=============================================="
echo ""
echo "MOVEMENT:"
echo "  W/S - Move forward/backward"
echo "  A/D - Move left/right"
echo "  Q/E - Move up/down"
echo ""
echo "LOOK:"
echo "  Arrow Keys - Look around"
echo ""
echo "OTHER:"
echo "  H - Toggle help"
echo "  ESC - Quit"
echo ""
echo "=============================================="
echo "  STARTING ASCII RAY TRACER"
echo "=============================================="
echo ""
echo "INSTRUCTIONS:"
echo "1. Choose quality level 2 (recommended)"
echo "2. Read the help screen, press any key"
echo "3. Use WASD to move around the Cornell Box"
echo "4. Use arrow keys to look around"
echo "5. Press H anytime for help"
echo "6. Press ESC to quit"
echo ""
echo "Starting in 3 seconds..."
echo ""

sleep 3

# Run the ASCII ray tracer
./raytracer_ascii

echo ""
echo "=============================================="
echo "  DEMO COMPLETE"
echo "=============================================="
echo ""
echo "Thanks for trying the ASCII Ray Tracer!"
echo ""
echo "To run again:"
echo "  make runa"
echo ""
echo "For more information:"
echo "  docs/ASCII_INTERACTIVE.md"
echo ""