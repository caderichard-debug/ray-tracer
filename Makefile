# SIMD Ray Tracer - Phase-based Build System
# Each phase adds new features and optimizations

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -Wno-deprecated-declarations -Xpreprocessor -fopenmp -flto
OPTFLAGS = -O3 -march=native -mavx2 -mfma -ffast-math
INCLUDES = -Isrc -Iexternal -I/usr/local/opt/libomp/include
LDFLAGS = -L/usr/local/opt/libomp/lib -lomp -flto
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

# GPU renderer sources
GPU_RENDERER_SRC = src/renderer/gpu_renderer.cpp src/renderer/shader_manager.cpp

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

# Interactive real-time ray tracer with GPU support
# Features: GPU-accelerated rendering, Camera movement, Quality switching
.PHONY: interactive-gpu
interactive-gpu: $(BUILD_DIR)
	@echo "Building Interactive Real-time Ray Tracer (GPU)"
	@echo "Features: OpenGL compute shaders, SDL2 window, Camera controls, Quality levels 1-3"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		-DGPU_RENDERING \
		src/main_interactive.cpp src/renderer/renderer.cpp $(GPU_RENDERER_SRC) \
		-o $(BUILD_DIR)/raytracer_interactive_gpu $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ Interactive GPU built: $(BUILD_DIR)/raytracer_interactive_gpu"
	@ln -sf $(BUILD_DIR)/raytracer_interactive_gpu raytracer_interactive_gpu

# Simple GPU infrastructure test
.PHONY: gpu-infra-test
gpu-infra-test: $(BUILD_DIR)
	@echo "Building GPU Infrastructure Test"
	@echo "This tests OpenGL context creation and shader compilation"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		-DGPU_INFRASTRUCTURE_TEST \
		src/renderer/gpu_renderer.cpp src/renderer/shader_manager.cpp \
		-o $(BUILD_DIR)/gpu_infra_test $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ GPU Infrastructure Test built: $(BUILD_DIR)/gpu_infra_test"
	@echo "Run with: ./build/gpu_infra_test"

# Standalone GPU ray tracer (compute shaders only) - REQUIRES OpenGL 4.3+
.PHONY: gpu-only
gpu-only: $(BUILD_DIR)
	@echo "Building Standalone GPU Ray Tracer (OpenGL 4.3+ Compute Shaders)"
	@echo "Features: OpenGL compute shaders, ray tracing, no CPU fallback"
	@echo "WARNING: Requires OpenGL 4.3+ or higher"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		src/main_gpu_only.cpp \
		-o $(BUILD_DIR)/gpu_only $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ Standalone GPU Ray Tracer built: $(BUILD_DIR)/gpu_only"
	@echo "Run with: ./build/gpu_only"

# Legacy GPU ray tracer (fragment shaders) - Works with OpenGL 2.0+
.PHONY: gpu-legacy
gpu-legacy: $(BUILD_DIR)
	@echo "Building Legacy GPU Ray Tracer (OpenGL 2.0+ Fragment Shaders)"
	@echo "Features: Fragment shader ray tracing, works on older GPUs"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		src/main_legacy_gpu.cpp \
		-o $(BUILD_DIR)/gpu_legacy $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ Legacy GPU Ray Tracer built: $(BUILD_DIR)/gpu_legacy"
	@echo "Run with: ./build/gpu_legacy"

# GPU ray tracer with full CPU feature parity (fragment shaders) - Works with OpenGL 3.3+
.PHONY: gpu-fragment
gpu-fragment: $(BUILD_DIR)
	@echo "Building GPU Ray Tracer with Full CPU Feature Parity (OpenGL 3.3+ Fragment Shaders)"
	@echo "Features: Phong shading, shadows, reflections, triangles, dielectric materials, anti-aliasing"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		src/main_gpu_fragment.cpp \
		-o $(BUILD_DIR)/gpu_fragment $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ GPU Fragment Ray Tracer built: $(BUILD_DIR)/gpu_fragment"
	@echo "Run with: ./build/gpu_fragment"

# Working GPU ray tracer (GLSL 1.20 compatible) - Works with OpenGL 2.0+
.PHONY: gpu-working
gpu-working: $(BUILD_DIR)
	@echo "Building Working GPU Ray Tracer (GLSL 1.20 - OpenGL 2.0+ Compatible)"
	@echo "Features: Cornell Box, Phong shading, spheres, triangles, metal and glass materials"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		src/main_gpu_working.cpp \
		-o $(BUILD_DIR)/gpu_working $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ Working GPU Ray Tracer built: $(BUILD_DIR)/gpu_working"
	@echo "Run with: ./build/gpu_working"

# Interactive GPU ray tracer (same scene as CPU, with camera controls)
.PHONY: gpu-interactive
gpu-interactive: $(BUILD_DIR)
	@echo "Building Interactive GPU Ray Tracer (Same Scene as CPU + Camera Controls)"
	@echo "Features: Same scene as CPU, WASD movement, mouse look, real-time rendering"
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) $(INCLUDES) $(SDL_INCLUDES) $(OPENGL_INCLUDES) \
		src/main_gpu_interactive.cpp \
		-o $(BUILD_DIR)/gpu_interactive $(LDFLAGS) $(SDL_LDFLAGS) $(OPENGL_LDFLAGS)
	@echo "✓ Interactive GPU Ray Tracer built: $(BUILD_DIR)/gpu_interactive"
	@echo "Run with: ./build/gpu_interactive"

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
	@echo "Controls: WASD=Move, Mouse=Look, 1-3=Quality, R=Toggle GPU, H=Help, Space=Pause, ESC=Quit"
	./raytracer_interactive

# Run interactive real-time ray tracer (GPU)
runi-gpu: interactive-gpu
	@echo "Starting interactive real-time ray tracer (GPU)..."
	@echo "Controls: WASD=Move, Mouse=Look, 1-3=Quality, R=Toggle GPU, H=Help, Space=Pause, ESC=Quit"
	@echo ""
	@echo "GPU Renderer: Press R to toggle between CPU/GPU rendering"
	@echo "Expected performance: 60-300x faster than CPU depending on GPU"
	./raytracer_interactive_gpu

# Run legacy GPU ray tracer (OpenGL 2.0+ compatible)
.PHONY: run-gpu-legacy
run-gpu-legacy: gpu-legacy
	@echo "Starting Legacy GPU Ray Tracer..."
	@echo "Features: Fragment shader ray tracing, compatible with older GPUs"
	@echo "Controls: ESC to quit"
	./build/gpu_legacy

# Run GPU fragment ray tracer with full CPU feature parity (OpenGL 3.3+)
.PHONY: run-gpu-fragment
run-gpu-fragment: gpu-fragment
	@echo "Starting GPU Fragment Ray Tracer (Full CPU Feature Parity)..."
	@echo "Features: Phong shading, shadows, reflections, triangles, dielectric materials"
	@echo "Controls: ESC to quit"
	./build/gpu_fragment

# Run working GPU ray tracer (GLSL 1.20 compatible)
.PHONY: run-gpu-working
run-gpu-working: gpu-working
	@echo "Starting Working GPU Ray Tracer (GLSL 1.20 Compatible)..."
	@echo "Features: Cornell Box, Phong shading, spheres, triangles, metal and glass materials"
	@echo "Controls: ESC to quit"
	./build/gpu_working

# Run interactive GPU ray tracer (same scene as CPU, with camera controls)
.PHONY: run-gpu-interactive
run-gpu-interactive: gpu-interactive
	@echo "Starting Interactive GPU Ray Tracer (Same Scene as CPU + Camera Controls)..."
	@echo "Features: Same scene as CPU renderer, WASD + mouse controls"
	@echo "Controls: Click window to capture mouse, WASD to move, mouse to look, ESC to quit"
	./build/gpu_interactive

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
	@echo "  Phase 2: $(shell [ - $(BUILD_DIR)/raytracer_phase2 ] && echo '✓ Built' || echo '✗ Not built')"
	@echo "  Interactive: $(shell [ -f $(BUILD_DIR)/raytracer_interactive ] && echo '✓ Built' || echo '✗ Not built')"
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
	@echo "  make gpu-only      - Build GPU ray tracer (OpenGL 4.3+ compute shaders)"
	@echo "  make gpu-legacy    - Build GPU ray tracer (OpenGL 2.0+ fragment shaders)"
	@echo "  make gpu-fragment  - Build GPU ray tracer with full CPU feature parity (OpenGL 3.3+)"
	@echo "  make gpu-working   - Build WORKING GPU ray tracer (GLSL 1.20 - OpenGL 2.0+)"
	@echo "  make gpu-interactive - Build INTERACTIVE GPU ray tracer (same scene as CPU + controls)"
	@echo "  make gpu-infra-test - Build GPU infrastructure test"
	@echo "  make all           - Build all implemented phases"
	@echo ""
	@echo "Running:"
	@echo "  make run           - Build and run batch ray tracer"
	@echo "  make runi          - Build and run interactive ray tracer (CPU)"
	@echo "  make runi-gpu      - Build and run interactive ray tracer (GPU)"
	@echo "  make run-gpu-legacy - Build and run legacy GPU ray tracer"
	@echo "  make run-gpu-fragment - Build and run GPU fragment ray tracer (full CPU parity)"
	@echo "  make run-gpu-working - Build and run WORKING GPU ray tracer"
	@echo "  make run-gpu-interactive - Build and run INTERACTIVE GPU (same scene as CPU, recommended)"
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
	@echo "Interactive Controls (make runi/runi-gpu):"
	@echo "  WASD          - Move camera"
	@echo "  Arrow Keys    - Move up/down"
	@echo "  Mouse         - Look around (when captured)"
	@echo "  Left Click    - Capture/release mouse"
	@echo "  1-3           - Quality level (capped for real-time)"
	@echo "  R             - Toggle CPU/GPU renderer (GPU mode only)"
	@echo "  H             - Toggle help overlay"
	@echo "  Space         - Pause rendering"
	@echo "  ESC           - Quit"
	@echo ""
	@echo "GPU Options:"
	@echo "  make run-gpu-legacy - Run legacy GPU ray tracer (works with OpenGL 2.0+)"
	@echo "  make gpu-only       - Run modern GPU ray tracer (requires OpenGL 4.3+)"

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
