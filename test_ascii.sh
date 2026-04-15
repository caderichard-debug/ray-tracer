#!/bin/bash

# Quick test script for ASCII ray tracer
# Runs for 5 frames then exits

echo "Testing ASCII Ray Tracer (5 frames)"
echo "====================================="
echo ""

# Create input file that will select quality 2 and then let it run
cat > /tmp/ascii_test_input.txt << 'EOF'
2
EOF

# Run the ASCII ray tracer with input
timeout 10s ./build/raytracer_ascii < /tmp/ascii_test_input.txt

echo ""
echo "Test complete!"
echo ""
echo "To run the full ASCII ray tracer:"
echo "  make runa"
echo ""
echo "The ASCII ray tracer will:"
echo "- Display Cornell Box scene in ASCII art"
echo "- Automatically orbit camera around scene"
echo "- Show real-time performance stats"
echo "- Map brightness to ASCII characters:  .:-=+*#%@"