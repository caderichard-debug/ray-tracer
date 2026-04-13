# SIMD Ray Tracer - Phase-based Build System
# Each phase adds new features and optimizations

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Xpreprocessor -fopenmp
OPTFLAGS = -O3 -march=native -mavx2 -mfma -ffast-math
INCLUDES = -Isrc -Iexternal -I/usr/local/opt/libomp/include
LDFLAGS = -L/usr/local/opt/libomp/lib -lomp
SDL_LDFLAGS = -L/usr/local/opt/sdl2/lib -lSDL2 -lSDL2_ttf
SDL_INCLUDES = -I/usr/local/opt/sdl2/include
OPENGL_LDFLAGS = -L/usr/local/opt/glew/lib -lGLEW -framework OpenGL
OPENGL_INCLUDES = -I/usr/local/opt/glew/include

# Source files (will vary by phase)
MATH_SRC = src/math/vec3.h src/math/ray.h
PRIMITIVES_SRC = src/primitives/primitive.h src/primitives/sphere.h
MATERIAL_SRC = src/material/material.h
CAMERA_SRC = src/camera/camera.h
SCENE_SRC = src/scene/scene.h src/scene/light.h
RENDERER_SRC = src/renderer/renderer.h src/renderer/renderer.cpp
MAIN_SRC = src/main.cpp

# All sources for current phase (Phase 2)
# Only .cpp files, headers are included automatically
ALL_SRCS = $(MAIN_SRC) src/renderer/renderer.cpp

# Output
BINARY = raytracer
BUILD_DIR = build

# Default target - build current phase (Phase 2)
.PHONY: all
all: phase2

# Phase 1: Foundation (basic scalar ray tracer)
# Features: Vec3, Ray, Sphere, Material, Camera
.PHONY: phase1
phase1: $(BUILD_DIR)
	@echo "Building Phase 1: Foundation (scalar ray tracer)"
	@echo "Features: Vec3, Ray, Sphere, Material, Camera"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) \
		$(MAIN_SRC) src/renderer/renderer.cpp \
		-o $(BUILD_DIR)/raytracer_phase1
	@echo "✓ Phase 1 built: $(BUILD_DIR)/raytracer_phase1"

# Phase 2: Basic Rendering (current working version)
# Features: Scene graph, Lights, Phong shading, Shadows, Reflections, Anti-aliasing, PNG output, OpenMP
.PHONY: phase2
phase2: $(BUILD_DIR)
	@echo "Building Phase 2: Basic Rendering + AA + PNG + OpenMP"
	@echo "Features: Scene, Lights, Phong shading, Shadows, Reflections, AA, PNG, Multi-threading"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) \
		$(ALL_SRCS) \
		-o $(BUILD_DIR)/raytracer_phase2 $(LDFLAGS)
	@echo "✓ Phase 2 built: $(BUILD_DIR)/raytracer_phase2"
	@ln -sf $(BUILD_DIR)/raytracer_phase2 $(BINARY)

# Interactive real-time ray tracer with SDL2
# Features: Real-time rendering, Camera movement, Quality switching
.PHONY: interactive
interactive: $(BUILD_DIR)
	@echo "Building Interactive Real-time Ray Tracer (CPU)"
	@echo "Features: SDL2 window, Camera controls, Quality levels 1-3"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) \
		src/main_interactive.cpp src/renderer/renderer.cpp \
		-o $(BUILD_DIR)/raytracer_interactive $(LDFLAGS) $(SDL_LDFLAGS)
	@echo "✓ Interactive built: $(BUILD_DIR)/raytracer_interactive"
	@ln -sf $(BUILD_DIR)/raytracer_interactive raytracer_interactive

# Simple GPU test - renders a green window to verify OpenGL works
.PHONY: gpu-test
gpu-test: $(BUILD_DIR)
	@echo "Building Simple GPU Test"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) \
		src/main_gpu.cpp \
		-o $(BUILD_DIR)/gpu_test $(LDFLAGS) $(SDL_LDFLAGS) -framework OpenGL
	@echo "✓ GPU Test built: $(BUILD_DIR)/gpu_test"
	@echo "Run with: ./build/gpu_test"

# Simple SDL test - renders a blue window without OpenGL
.PHONY: sdl-test
sdl-test: $(BUILD_DIR)
	@echo "Building Simple SDL Test"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) \
		src/main_sdl_test.cpp \
		-o $(BUILD_DIR)/sdl_test $(LDFLAGS) $(SDL_LDFLAGS)
	@echo "✓ SDL Test built: $(BUILD_DIR)/sdl_test"
	@echo "Run with: ./build/sdl_test"

# Working GPU ray tracer - simple fragment shader ray tracing
.PHONY: working-gpu
working-gpu: $(BUILD_DIR)
	@echo "Building Working GPU Ray Tracer"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) \
		src/main_working_gpu.cpp \
		-o $(BUILD_DIR)/working_gpu $(LDFLAGS) $(SDL_LDFLAGS) -framework OpenGL
	@echo "✓ Working GPU built: $(BUILD_DIR)/working_gpu"
	@echo "Run with: ./build/working_gpu"

# Interactive real-time ray tracer with GPU compute shader support
.PHONY: interactive-gpu
interactive-gpu: $(BUILD_DIR)
	@echo "Building Interactive Real-time Ray Tracer (GPU)"
	@echo "Features: SDL2 window, OpenGL fragment shaders, Quality levels 1-3"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		-DUSE_GPU_RENDERER \
		src/main_interactive.cpp src/renderer/renderer.cpp \
		-o $(BUILD_DIR)/raytracer_interactive_gpu $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ Interactive GPU built: $(BUILD_DIR)/raytracer_interactive_gpu"
	@ln -sf $(BUILD_DIR)/raytracer_interactive_gpu raytracer_interactive_gpu

# Phase 3: SIMD Vectorization (to be implemented)
# Features: AVX2 Vec3, Ray packets, Vectorized intersection
.PHONY: phase3
phase3:
	@echo "Building Phase 3: SIMD Vectorization"
	@echo "Features: AVX2 SIMD, Ray packets (8x), Vectorized intersection"
	@echo "⚠️  Phase 3 not yet implemented"
	@# $(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) \
	@# 	-DPHASE=3 \
	@# 	$(ALL_SRCS) src/math/vec3_avx2.cpp \
	@# 	-o $(BUILD_DIR)/raytracer_phase3
	@# @echo "✓ Phase 3 built: $(BUILD_DIR)/raytracer_phase3"

# Phase 4: Advanced Features (to be implemented)
# Features: Triangles, Planes, Soft shadows, Anti-aliasing
.PHONY: phase4
phase4:
	@echo "Building Phase 4: Advanced Features"
	@echo "Features: Triangles, Planes, Soft shadows, Anti-aliasing"
	@echo "⚠️  Phase 4 not yet implemented"

# Phase 5: Multi-threading (to be implemented)
# Features: OpenMP, Tile-based rendering, Thread-safe framebuffer
.PHONY: phase5
phase5:
	@echo "Building Phase 5: Multi-threading"
	@echo "Features: OpenMP, Tile-based parallelization"
	@echo "⚠️  Phase 5 not yet implemented"

# Phase 6: Polish (to be implemented)
# Features: PNG output, Tone mapping, CLI arguments
.PHONY: phase6
phase6:
	@echo "Building Phase 6: Polish"
	@echo "Features: PNG output, Tone mapping, CLI args"
	@echo "⚠️  Phase 6 not yet implemented"

# Run current phase
.PHONY: run runi runi-gpu test-int
run: phase2
	@echo "Running ray tracer (Phase 2)..."
	./$(BINARY) > output.ppm
	@echo "✓ Output written to output.ppm"

# Run interactive real-time ray tracer (CPU)
runi: interactive
	@echo "Starting interactive real-time ray tracer (CPU)..."
	@echo "Controls: WASD=Move, Mouse=Look, 1-3=Quality, H=Help, Space=Pause, ESC=Quit"
	./raytracer_interactive

# Run interactive real-time ray tracer (GPU - EXPERIMENTAL)
runi-gpu: interactive-gpu
	@echo "Starting interactive real-time ray tracer (GPU)..."
	@echo "⚠️  GPU mode is experimental - may have rendering issues"
	@echo "Controls: WASD=Move, Mouse=Look, 1-3=Quality, H=Help, Space=Pause, ESC=Quit"
	./raytracer_interactive_gpu

# Run with test scene
.PHONY: test
test: phase2
	@echo "Running Cornell box test scene..."
	./$(BINARY) > cornell.ppm
	@echo "✓ Cornell box written to cornell.ppm"



# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(BINARY)
	rm -f *.ppm
	rm -f *.png
	rm -f *.jpg
	rm -rf benchmark_results
	@echo "✓ Clean complete"

# Clean and rebuild
.PHONY: rebuild
rebuild: clean all

# Install build dependencies
.PHONY: deps
deps:
	@echo "Checking dependencies..."
	@which $(CXX) > /dev/null || (echo "✗ g++ not found"; exit 1)
	@echo "✓ g++ found: $(shell $(CXX) --version | head -1)"
	@echo ""
	@echo "Testing AVX2 support..."
	@echo "int main() { __asm__ volatile(\".byte 0xc5\"); return 0; }" \
		| $(CXX) -x c - -mavx2 -o /dev/null 2>/dev/null \
		&& echo "✓ AVX2 supported" \
		|| echo "⚠️  AVX2 not supported (will fail at runtime)"
	@echo ""
	@echo "Dependencies check complete"

# Show build info
.PHONY: info
info:
	@echo "=== Ray Tracer Build Info ==="
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS) $(OPTFLAGS)"
	@echo "Includes: $(INCLUDES)"
	@echo ""
	@echo "Phases:"
	@echo "  Phase 1: $(shell [ -f $(BUILD_DIR)/raytracer_phase1 ] && echo '✓ Built' || echo '✗ Not built')"
	@echo "  Phase 2: $(shell [ -f $(BUILD_DIR)/raytracer_phase2 ] && echo '✓ Built' || echo '✗ Not built')"
	@echo "  Interactive (CPU): $(shell [ -f $(BUILD_DIR)/raytracer_interactive ] && echo '✓ Built' || echo '✗ Not built')"
	@echo "  Interactive (GPU): $(shell [ -f $(BUILD_DIR)/raytracer_interactive_gpu ] && echo '✓ Built' || echo '✗ Not built')"
	@echo ""
	@echo "Binary: $(BINARY) -> $(shell ls -l $(BINARY) 2>/dev/null | awk '{print $$NF}' || echo 'Not built')"

# Documentation
.PHONY: docs
docs:
	@echo "=== Documentation ==="
	@echo "Main index: docs/index.md"
	@echo ""
	@echo "Phase documentation:"
	@echo "  Phase 1: docs/phase1-foundation.md"
	@echo "  Phase 2: docs/phase2-rendering.md"
	@echo "  Phase 3: docs/phase3-simd.md (pending)"
	@echo "  Phase 4: docs/phase4-advanced.md (pending)"
	@echo "  Phase 5: docs/phase5-multithreading.md (pending)"
	@echo "  Phase 6: docs/phase6-polish.md (pending)"
	@echo ""
	@echo "View docs: open docs/index.md"

# Help
.PHONY: help
help:
	@echo "=== SIMD Ray Tracer - Makefile Targets ==="
	@echo ""
	@echo "Building:"
	@echo "  make phase1        - Build Phase 1 (scalar foundation)"
	@echo "  make phase2        - Build Phase 2 (basic rendering) [default]"
	@echo "  make interactive   - Build real-time interactive ray tracer (CPU)"
	@echo "  make interactive-gpu - Build real-time interactive ray tracer (GPU)"
	@echo "  make all           - Build all implemented phases"
	@echo ""
	@echo "Running:"
	@echo "  make run           - Build and run batch ray tracer"
	@echo "  make runi          - Build and run interactive ray tracer (CPU)"
	@echo "  make runi-gpu      - Build and run interactive ray tracer (GPU)"
	@echo "  make test          - Run Cornell box test scene"
	@echo ""
	@echo "Testing:"
	@echo "  make benchmark     - Benchmark all phases"
	@echo "  make compare       - Compare phase outputs"
	@echo ""
	@echo "Utilities:"
	@echo "  make clean         - Remove build artifacts"
	@echo "  make rebuild       - Clean and rebuild"
	@echo "  make deps          - Check dependencies"
	@echo "  make info          - Show build information"
	@echo "  make docs          - Show documentation"
	@echo "  make help          - Show this help"
	@echo ""
	@echo "Documentation: docs/index.md"
	@echo ""
	@echo "Interactive Controls (make runi):"
	@echo "  WASD          - Move camera"
	@echo "  Arrow Keys    - Move up/down"
	@echo "  Mouse         - Look around (when captured)"
	@echo "  Left Click    - Capture/release mouse"
	@echo "  1-3           - Quality level (capped for real-time)"
	@echo "  H             - Toggle help overlay"
	@echo "  Space         - Pause rendering"
	@echo "  ESC           - Quit"
	@echo ""
	@echo "GPU Rendering (make runi-gpu):"
	@echo "  Uses OpenGL 4.3+ compute shaders for 60-300x faster rendering"
	@echo "  Requires: OpenGL 4.3+, GLEW"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Ensure build directory exists before building
.PHONY: $(BUILD_DIR)

# Comprehensive benchmark - build all phases, run them, generate performance log
.PHONY: benchmark-all
benchmark-all:
	@echo "=========================================="
	@echo "  COMPREHENSIVE RAY TRACER BENCHMARK"
	@echo "=========================================="
	@echo ""
	@mkdir -p benchmark_results
	@rm -f benchmark_results/performance.log
	@echo "# Ray Tracer Performance Benchmark" > benchmark_results/performance.log
	@echo "========================================" >> benchmark_results/performance.log
	@echo "" >> benchmark_results/performance.log
	@#
	@# Build and benchmark Phase 1
	@echo "Building Phase 1..."
	@$(MAKE) --no-print-directory phase1 > /dev/null 2>&1 && \
		echo "✓ Phase 1 built" || echo "⚠️  Phase 1 build failed"
	@echo "" >> benchmark_results/performance.log
	@echo "## Phase 1: Scalar Foundation" >> benchmark_results/performance.log
	@echo "====" >> benchmark_results/performance.log
	@if [ -f $(BUILD_DIR)/raytracer_phase1 ]; then \
		echo "Running Phase 1 benchmark..."; \
			$(BUILD_DIR)/raytracer_phase1 > benchmark_results/phase1_output.ppm 2> benchmark_results/phase1_stderr.log;
			cat benchmark_results/phase1_stderr.log >> benchmark_results/performance.log || true;
		echo "✓ Phase 1 complete: benchmark_results/phase1_output.ppm"; \
	else \
		echo "Phase 1 not available" >> benchmark_results/performance.log; \
	fi
	@echo "====" >> benchmark_results/performance.log
	@echo "" >> benchmark_results/performance.log
	@#
	@# Build and benchmark Phase 2
	@echo "Building Phase 2..."
	@$(MAKE) --no-print-directory phase2 > /dev/null 2>&1 && \
		echo "✓ Phase 2 built" || echo "⚠️  Phase 2 build failed"
	@echo "## Phase 2: Basic Rendering" >> benchmark_results/performance.log
	@echo "====" >> benchmark_results/performance.log
	@if [ -f $(BUILD_DIR)/raytracer_phase2 ]; then \
		echo "Running Phase 2 benchmark..."; \
			$(BUILD_DIR)/raytracer_phase2 > benchmark_results/phase2_output.ppm 2> benchmark_results/phase2_stderr.log;
			cat benchmark_results/phase2_stderr.log >> benchmark_results/performance.log || true;
		echo "✓ Phase 2 complete: benchmark_results/phase2_output.ppm"; \
	else \
		echo "Phase 2 not available" >> benchmark_results/performance.log; \
	fi
	@echo "====" >> benchmark_results/performance.log
	@echo "" >> benchmark_results/performance.log
	@#
	@echo "=========================================="
	@echo "  BENCHMARK COMPLETE"
	@echo "=========================================="
	@echo ""
	@echo "Results:"
	@echo "  📊 Performance log:  benchmark_results/performance.log"
	@echo "  🖼️  Phase 1 output:   benchmark_results/phase1_output.ppm"
	@echo "  🖼️  Phase 2 output:   benchmark_results/phase2_output.ppm"
	@echo ""
	@cat benchmark_results/performance.log

# Quick benchmark (current phase only)
.PHONY: bench
bench:
	@echo "=== Quick Benchmark ==="
	@$(MAKE) --no-print-directory phase2 > /dev/null 2>&1
	@echo ""
	@echo "Running Phase 2 benchmark..."
	@./$(BINARY) > /dev/null
	@echo ""
	@echo "For detailed benchmark with all phases: make benchmark-all"

# Alias benchmark to benchmark-all
.PHONY: benchmark
benchmark:
	@$(MAKE) --no-print-directory benchmark-all

# Extract and compare performance metrics from log
.PHONY: compare
compare:
	@echo "=== Performance Comparison ==="
	@echo ""
	@if [ -f benchmark_results/performance.log ]; then \
		echo "Reading benchmark_results/performance.log..."; \
		echo ""; \
		grep -E "(Image Size|Render Time|Throughput|Pixel Rate)" benchmark_results/performance.log | sed 's/^/  /'; \
	else \
		echo "⚠️  No benchmark log found. Run: make benchmark-all"; \
	fi

# Show benchmark help
.PHONY: bench-help
bench-help:
	@echo "=== Benchmark Help ==="
	@echo ""
	@echo "Available targets:"
	@echo "  make benchmark-all  - Build and benchmark ALL phases"
	@echo "  make bench          - Quick benchmark (current phase only)"
	@echo "  make benchmark     - Same as benchmark-all"
	@echo "  make compare       - Compare performance metrics"
	@echo ""
	@echo "Results stored in benchmark_results/:"
	@echo "  performance.log         - Detailed performance log"
	@echo "  phase1_output.ppm      - Phase 1 rendered image"
	@echo "  phase2_output.ppm      - Phase 2 rendered image"
	@echo ""
