# SIMD Ray Tracer - Manager Agent Context

## Project Overview
A high-performance ray tracer built from scratch in C++ with three distinct rendering modes: CPU interactive (advanced optimizations), GPU interactive (massive parallelism), and ASCII terminal (retro aesthetics). This manager context provides oversight for coordinating development across all three components.

## Current Development Status

### Active Development Instances
- **CPU Agent**: Working on CPU renderer optimizations and advanced features
- **GPU Agent**: Working on GPU renderer with OpenGL compute shaders
- **ASCII Agent**: Working on terminal-based rendering and aesthetics
- **Documentation**: Keeping all documentation synchronized

### Component Status
- ✅ **CPU Renderer**: Fully functional with 23.2x optimization stack
- ✅ **GPU Renderer**: Standalone GLSL 1.20 implementation (60-300x faster)
- ✅ **ASCII Renderer**: Cross-platform terminal rendering
- ✅ **Build System**: Unified Makefile with feature flags
- ✅ **Documentation**: Comprehensive docs for all components

## Architecture Overview

### Three Rendering Modes

**CPU Interactive (main_cpu_interactive.cpp)**
- Advanced optimization stack (23.2x speedup)
- Progressive, adaptive, wavefront rendering modes
- Full settings panel with live controls
- Post denoise amount slider (0-100%) with improved detail retention
- Analysis modes (normals, depth, albedo)
- **Agent context**: claude-raytracer-cpu.md

**GPU Interactive (main_gpu_interactive.cpp)**
- GLSL 1.20 standalone implementation
- 60-300x speedup over CPU
- Real-time performance focus
- Different feature set (optimizing for throughput)
- **Agent context**: claude-raytracer-gpu.md

**ASCII Terminal (main_ascii.cpp)**
- Pure terminal rendering (no GUI)
- Animated camera orbits
- Cross-platform compatibility
- Retro aesthetic focus
- **Agent context**: claude-raytracer-ascii.md

### Shared Components
- **src/renderer/renderer.cpp**: Core ray tracing engine (CPU + ASCII)
- **src/scene/cornell_box.h**: Standard test scene (all modes)
- **src/material/material.h**: Material definitions (shared)
- **Makefile**: Unified build system with feature flags

## Build System Coordination

### Unified Makefile Structure

**Feature Flag System:**
```bash
# Rendering features (all modes)
ENABLE_SHADOWS=1
ENABLE_REFLECTIONS=1

# CPU-specific optimizations
ENABLE_SHADOW_CULLING=1    # +8.6%
ENABLE_FAST_RNG=1           # +2.8%
ENABLE_LOOP_UNROLL=0        # +3-5%

# Parallelization (CPU mode)
ENABLE_OPENMP=1             # +14-20x
ENABLE_PTHREADS=0           # Alternative: +12-18x

# SIMD (CPU mode)
ENABLE_AVX=1                # +4-6x

# Advanced CPU rendering modes
ENABLE_PROGRESSIVE=1        # 3.164x
ENABLE_ADAPTIVE=1           # 1.702x
ENABLE_WAVEFRONT=1          # 1.358x
```

**Build Targets:**
```bash
# CPU rendering
make batch-cpu         # CPU batch with feature flags
make interactive-cpu   # CPU interactive (SDL2)

# GPU rendering
make batch-gpu         # GPU batch
make interactive-gpu   # GPU interactive (GLSL 1.20)

# ASCII rendering
make ascii             # ASCII terminal
make runa              # Run ASCII terminal

# Performance comparison
make benchmark         # CPU feature combinations
make benchmark-cpu-gpu  # CPU vs GPU comparison
```

## Performance Coordination

### Performance Comparison Matrix

| Renderer | Baseline | Optimized | Speedup | Best Use Case |
|----------|----------|-----------|---------|---------------|
| **CPU** | 67.2s | 1.5s | 43.7x | Flexibility, features |
| **GPU** | N/A | 0.25s | 160x+ | Raw throughput |
| **ASCII** | N/A | N/A | N/A | Aesthetics, education |

### Performance Stack Breakdown

**CPU Optimizations (cumulative):**
- Scalar baseline: 67.2s
- + AVX SIMD: 13.4s (5.0x)
- + OpenMP: 3.4s (19.8x)
- + Optimizations: 1.5s (43.7x total)

**GPU Performance:**
- No CPU feature parity needed
- Direct GPU implementation
- 60-300x faster depending on GPU tier
- Optimized for throughput over flexibility

## Agent Coordination Strategy

### Manager Responsibilities

**Build System Management:**
- Maintain unified Makefile with all targets
- Ensure feature flags work across all modes
- Coordinate build dependencies
- Handle platform-specific issues

**Performance Tracking:**
- Centralized benchmark_results/ directory
- Standardized performance measurement
- Cross-component performance comparisons
- Track optimization impact over time

**Documentation Synchronization:**
- Keep README.md current with all features
- Maintain LLM_CONTEXT.md as manager context
- Coordinate agent-specific documentation
- Ensure consistency across docs

**Quality Assurance:**
- Standardized testing (Cornell Box scene)
- Verify feature combinations work
- Cross-platform compatibility testing
- Performance regression detection

### Agent Communication Protocols

**CPU Agent → Manager:**
- Report optimization results with measurements
- Request feature flag additions
- Report performance regressions
- Coordinate interactive features

**GPU Agent → Manager:**
- Report GPU compatibility issues
- Request GLSL version changes
- Report performance by GPU tier
- Coordinate scene format changes

**ASCII Agent → Manager:**
- Report terminal compatibility issues
- Request cross-platform features
- Report aesthetic improvements
- Coordinate with shared renderer changes

**Manager → All Agents:**
- Coordinate scene format changes (Cornell Box)
- Ensure build system changes don't break components
- Standardize performance measurement
- Facilitate cross-component optimization

## Shared Resources

### Scene Definition
**Cornell Box Scene (src/scene/cornell_box.h):**
- Standard test scene for all modes
- 10 spheres with different materials
- 1 point light source
- Consistent across CPU, GPU, ASCII

**Why standardization matters:**
- Fair performance comparisons
- Consistent visual output
- Easier debugging
- Cross-mode compatibility

### Renderer Engine
**Shared Core (src/renderer/renderer.cpp):**
- CPU and ASCII both use this engine
- GPU has separate implementation
- Material system shared
- Camera model shared

**Coordination points:**
- Scene format changes affect all modes
- Material changes affect CPU, GPU, ASCII
- Camera changes affect all modes

## Development Workflow

### Adding New Features

**Manager Process:**
1. **Evaluate impact**: Which components are affected?
2. **Design coordination**: How should feature work across modes?
3. **Feature flags**: Add appropriate flags to Makefile
4. **Agent delegation**: Assign work to appropriate agents
5. **Testing**: Verify feature works in all affected modes
6. **Documentation**: Update all relevant docs
7. **Performance tracking**: Measure impact

**Example: Adding new material type**
1. Manager identifies need for new material
2. Discuss with agents: CPU (easy), GPU (shader changes), ASCII (automatic)
3. Add to material.h (shared)
4. CPU agent implements in renderer.cpp
5. GPU agent implements GLSL functions
6. Test in all three modes
7. Update documentation

### Performance Optimization Workflow

**Manager Process:**
1. **Profile**: Identify bottlenecks across all modes
2. **Prioritize**: Which optimizations give most benefit?
3. **Delegate**: Assign optimizations to appropriate agents
4. **Measure**: Use standardized benchmarking
5. **Compare**: Cross-mode performance impact
6. **Document**: Update performance tables

**Example: Loop unrolling optimization**
1. Manager identifies sample loop as bottleneck
2. CPU agent implements loop unrolling
3. Add ENABLE_LOOP_UNROLL flag to Makefile
4. Benchmark impact: +3-5% improvement
5. Document in README and agent contexts
6. GPU/ASCII agents evaluate if applicable

## Quality Assurance

### Standardized Testing

**Performance Testing:**
```bash
make benchmark         # CPU optimization stack
make benchmark-cpu-gpu  # Cross-mode comparison
```

**Functional Testing:**
- Test with Cornell Box scene
- Verify all quality levels work
- Check feature combinations
- Test interactive controls

**Cross-Platform Testing:**
- macOS (Homebrew dependencies)
- Linux (package manager dependencies)
- Windows (MinGW-w64 or MSVC)

### Regression Prevention

**Performance Regressions:**
- Track benchmark_results/ over time
- Alert if performance degrades
- Require optimization improvements

**Feature Regressions:**
- Test suite for core features
- Visual output comparison
- Interactive controls verification

## Troubleshooting Coordination

### Cross-Mode Issues

**Scene format problems:**
- Affects: All three modes
- Coordination: Update Cornell Box, test all modes
- Agent responsible: All agents, coordinated by manager

**Build system issues:**
- Affects: All modes
- Coordination: Fix Makefile, verify all targets
- Agent responsible: Manager

**Performance anomalies:**
- Affects: Usually specific mode
- Coordination: Agent investigates, manager tracks
- Agent responsible: Specific agent

### Platform-Specific Issues

**macOS:**
- OpenGL version limitations (GPU agent)
- OpenMP path issues (CPU agent)
- Terminal detection (ASCII agent)
- Manager coordination needed

**Linux:**
- Driver compatibility (GPU agent)
- Package variations (all agents)
- Terminal diversity (ASCII agent)
- Most compatible platform overall

**Windows:**
- Different paths and libraries (all agents)
- Terminal limitations (ASCII agent)
- Visual Studio compatibility (manager)
- Requires special handling

## Important Manager Decisions

### Feature Parity Strategy
**Decision: NOT pursue feature parity between CPU and GPU**

**Rationale:**
- CPU: Flexibility and advanced features
- GPU: Raw throughput and simplicity
- Different design goals
- Performance vs. feature richness trade-off

**Implementation:**
- CPU gets: progressive, adaptive, wavefront, analysis modes
- GPU gets: raw speed, real-time performance
- ASCII gets: aesthetic appeal, education value
- Each mode optimized for its use case

### Optimization Priorities
**CPU Agent:** Focus on advanced rendering modes
- Progressive rendering (3.164x)
- Adaptive sampling (1.702x)
- Wavefront rendering (1.358x)
- Loop unrolling (+3-5%)
- Not: Micro-optimizations (diminishing returns)

**GPU Agent:** Focus on shader efficiency
- Better GLSL optimization
- Advanced GPU features (OpenGL 4.3+)
- Compute shader migration
- Not: CPU feature parity

**ASCII Agent:** Focus on aesthetics
- Better character sets
- Smoother animation
- Cross-platform compatibility
- Not: Performance (terminal I/O bound)

## Project Status Tracking

### Completed Milestones
- ✅ Three working renderers (CPU, GPU, ASCII)
- ✅ Unified build system with feature flags
- ✅ Comprehensive performance benchmarking
- ✅ Cross-platform compatibility
- ✅ Complete documentation system
- ✅ Agent-specific context files

### Current Priorities
1. **Performance optimization**: CPU agent pursuing advanced features
2. **GPU enhancement**: GPU agent exploring compute shaders
3. **Aesthetic improvements**: ASCII agent enhancing visual appeal
4. **Documentation**: Keeping all docs synchronized

### Future Roadmap
**Short Term (High Priority):**
- True variance-based adaptive sampling (CPU)
- BVH acceleration structure (CPU)
- OpenGL 4.3+ compute shaders (GPU)
- Color ASCII art (ASCII)

**Long Term (Research):**
- Path tracing integration (CPU)
- Advanced lighting models (all modes)
- Scene file format (JSON/glTF)
- Denoising support (post-processing)

## Success Metrics

### Overall Project Goals
- **Educational value**: Teach ray tracing fundamentals
- **Performance**: Demonstrate optimization techniques
- **Code quality**: Maintainable, well-documented code
- **Cross-platform**: Work on macOS, Linux, Windows

### Component Success Metrics
**CPU Renderer:**
- Performance: 43.7x speedup achieved
- Features: Progressive, adaptive, wavefront working
- Usability: Smooth interactive experience

**GPU Renderer:**
- Performance: 60-300x speedup achieved
- Compatibility: OpenGL 2.0+ working
- Real-time: 60+ FPS at reasonable quality

**ASCII Renderer:**
- Aesthetics: Recognizable 3D scenes
- Cross-platform: Works on all major platforms
- Education: Shows ray tracing fundamentals

## Manager Agent Responsibilities

### Daily Operations
- Monitor agent progress and coordination
- Review and integrate agent changes
- Update project documentation
- Handle cross-agent issues
- Performance tracking and reporting

### Strategic Planning
- Prioritize features and optimizations
- Allocate development resources
- Balance across three components
- Plan for future enhancements

### Quality Assurance
- Ensure consistent quality across components
- Cross-platform compatibility testing
- Performance regression prevention
- Documentation accuracy

### Communication
- Coordinate between agents
- Resolve conflicts and dependencies
- Ensure information sharing
- Maintain project coherence

---

**Manager Role**: Project coordination and oversight
**Primary Focus**: Balance between three components, ensure project coherence
**Coordination**: Work with all specialized agents, maintain big-picture view

### Project Structure
```
ray-tracer/
├── src/
│   ├── main.cpp                    # Batch ray tracer
│   ├── main_cpu_interactive.cpp    # CPU interactive mode (SDL2)
│   ├── main_gpu_interactive.cpp    # GPU interactive mode (OpenGL)
│   ├── main_ascii.cpp              # ASCII terminal mode
│   ├── math/
│   │   ├── vec3.h                  # Scalar Vec3 operations
│   │   ├── vec3_avx2.h             # SIMD Vec3 (8-wide)
│   │   └── ray.h                   # Ray representation
│   ├── primitives/
│   │   ├── sphere.h                # Sphere intersection
│   │   ├── sphere_avx2.h           # SIMD sphere intersection
│   │   └── triangle.h              # Triangle with Möller-Trumbore
│   ├── material/
│   │   └── material.h              # Lambertian, Metal, Dielectric
│   ├── camera/
│   │   └── camera.h                # Perspective camera
│   ├── scene/
│   │   ├── scene.h                 # Scene graph
│   │   ├── light.h                 # Point lights
│   │   └── cornell_box.h           # Standard test scene
│   ├── renderer/
│   │   ├── renderer.h/cpp          # CPU ray tracing logic
│   │   ├── gpu_renderer.h/cpp      # GPU renderer
│   │   └── shader_manager.h/cpp    # OpenGL shader management
│   └── texture/
│       └── texture.h               # Texture system
├── docs/                           # Comprehensive documentation
├── build/                          # Build artifacts
└── Makefile                        # Build system
```

## Key Features

### Rendering Modes

**CPU Interactive (main_cpu_interactive.cpp)**
- SDL2-based real-time rendering
- Advanced optimization modes:
  - Progressive Rendering (3.164x speedup)
  - Adaptive Sampling (1.702x speedup)
  - Wavefront Rendering (1.358x speedup)
- Settings panel with live adjustments
- Analysis modes (normals, depth, albedo)
- Screenshot capture

**GPU Interactive (main_gpu_interactive.cpp)**
- Standalone GPU ray tracer (GLSL 1.20, OpenGL 2.0+)
- No CPU feature parity - pure GPU implementation
- Exact CPU Cornell Box scene replication
- 60-300x faster than CPU (GPU-dependent)

**ASCII Terminal (main_ascii.cpp)**
- Pure terminal rendering (no GUI)
- Animated camera orbits
- Adaptive terminal sizing
- Cross-platform (macOS/Linux/Windows)

**Batch (main.cpp)**
- Offline high-quality rendering
- PNG output via stb_image_write
- Command-line interface

### Rendering Features

**Lighting & Shading:**
- Phong shading (ambient + diffuse + specular)
- Hard shadows with shadow rays
- Shadow ray culling (backface optimization)
- Recursive reflections (configurable depth)
- Gamma correction

**Materials:**
- Lambertian (diffuse)
- Metal (reflective)
- Dielectric (glass/refraction)

**Optimizations:**
- Early shadow ray culling (8.6% faster)
- Fast XOR-shift RNG (2.8% faster)
- OpenMP dynamic scheduling
- Cache-coherent wavefront rendering

## Performance Baseline

### Full Optimization Stack (800x450, 16 samples)

**Parallelization and SIMD:**
| Configuration | Time | Throughput | Speedup |
|--------------|------|------------|---------|
| Baseline (scalar, 1 thread) | ~67.2s | ~0.09 MRays/s | 1.0x |
| + AVX SIMD | ~13.4s | ~0.45 MRays/s | **5.0x** |
| + OpenMP (4 cores) | ~4.5s | ~1.35 MRays/s | **14.9x** |
| AVX + OpenMP | ~3.4s | ~1.80 MRays/s | **19.8x** |

**Phase 1 Code Optimizations (on AVX+OpenMP):**
| Optimization | Time | Throughput | Speedup | Improvement |
|--------------|------|------------|---------|-------------|
| AVX + OpenMP baseline | 3.4s | 1.80 MRays/s | 1.0x | - |
| + Shadow Culling | 3.1s | 1.95 MRays/s | 1.09x | +8.6% |
| + Fast RNG | 3.0s | 2.00 MRays/s | 1.11x | +2.8% |
| + Loop Unrolling | 2.9s | 2.07 MRays/s | 1.15x | +3.5% |

**Advanced Rendering Modes (on fully optimized base):**
| Rendering Mode | Time | Throughput | Speedup |
|----------------|------|------------|---------|
| Standard (all optimizations) | 1.540s | 3.740 MRays/s | 1.0x |
| Progressive | 0.487s | 11.836 MRays/s | **3.164x** |
| Adaptive | 0.905s | 6.367 MRays/s | **1.702x** |
| Wavefront | 1.134s | 5.079 MRays/s | **1.358x** |

### Total Performance Improvement
- **Scalar baseline → Final**: 67.2s → 1.5s = **43.7x total speedup**
- **Breakdown**: AVX (5x) × OpenMP (4x) × Optimizations (1.15x) = **23x combined**
- **With Progressive**: Up to **73x faster** than baseline

### Optimization Details

**Parallelization:**
- **AVX SIMD (5x)**: 8-wide vector operations, processes 8 rays simultaneously
- **OpenMP (4x)**: Multi-threading across 4 CPU cores, dynamic scheduling

**Code Optimizations:**
- **Shadow Culling (+8.6%)**: Skips shadow rays for backfaces, eliminates ~50% of shadow ray casts
- **Fast RNG (+2.8%)**: XOR-shift algorithm replaces rand(), eliminates lock contention
- **Loop Unrolling (+3-5%)**: Reduces loop overhead in sample accumulation and light loops

**Rendering Modes:**
- **Progressive (3.164x)**: Multi-pass refinement from noisy to smooth over multiple frames
- **Adaptive (1.702x)**: Variance-based sampling, uses half the samples
- **Wavefront (1.358x)**: Tiled rendering for better cache coherence

### GPU Performance
- **Low-end GPUs**: 30-60 MRays/sec (30-75x faster than CPU)
- **Mid-range GPUs**: 100-200 MRays/sec (100-250x faster)
- **High-end GPUs**: 300-500 MRays/sec (300-600x faster)
- Real-time 1080p at 60+ FPS
- Best for interactive exploration
- GLSL 1.20 compatible (OpenGL 2.0+)

### CPU vs GPU Comparison

**Use CPU when:**
- Developing and debugging new features
- Need analysis modes (normals, depth, albedo)
- Testing progressive/adaptive/wavefront optimizations
- Working on complex scenes
- No GPU available
- Need maximum flexibility

**Use GPU when:**
- Rendering final high-quality images
- Real-time interactive exploration
- High sample counts required
- Benchmarking raw performance
- GPU available and compatible
- Need maximum throughput

**Performance Comparison (1920x1080, 16 samples):**
| Renderer | Time | Speedup |
|----------|------|---------|
| CPU (Standard) | 40.0s | 1x |
| CPU (Progressive) | 12.6s | 3.164x |
| GPU | 0.25s | 160x |

## Build System

### Makefile Targets

**Building:**
```bash
make batch-cpu         # Build CPU batch ray tracer
make batch-gpu         # Build GPU batch ray tracer
make interactive-cpu   # Build CPU interactive (SDL2)
make interactive       # Alias for interactive-cpu
make interactive-gpu   # Build GPU interactive (GLSL 1.20)
make ascii             # Build ASCII terminal
make all               # Build default (batch-cpu)
```

**Running:**
```bash
make run               # Build and run CPU batch
make run-gpu           # Build and run GPU batch
make runi-cpu          # Build and run CPU interactive
make runi              # Alias for runi-cpu
make runi-gpu          # Build and run GPU interactive
make runa              # Build and run ASCII terminal
```

**Feature Flags (CPU):**
```bash
# Rendering features
make batch-cpu ENABLE_SHADOWS=0          # Disable shadows
make batch-cpu ENABLE_REFLECTIONS=0      # Disable reflections

# CPU optimizations (Phase 1)
make batch-cpu ENABLE_SHADOW_CULLING=1  # Shadow ray culling (+8.6%)
make batch-cpu ENABLE_FAST_RNG=1        # XOR-shift RNG (+2.8%)
make batch-cpu ENABLE_LOOP_UNROLL=1     # Loop unrolling (+3-5%)

# Parallelization and SIMD
make batch-cpu ENABLE_OPENMP=1          # Multi-threading (+14-20x)
make batch-cpu ENABLE_AVX=1             # SIMD (+4-6x)
make batch-cpu ENABLE_OPENMP=0 ENABLE_AVX=0  # Scalar baseline

# Advanced rendering modes
make batch-cpu ENABLE_PROGRESSIVE=1      # Progressive rendering (3.164x)
make batch-cpu ENABLE_ADAPTIVE=1         # Adaptive sampling (1.702x)
make batch-cpu ENABLE_WAVEFRONT=1        # Wavefront rendering (1.358x)

# Performance stack comparison
make benchmark  # Test all optimization combinations
```

**Benchmarking:**
```bash
make benchmark         # Benchmark CPU feature combinations
make benchmark-cpu-gpu  # Compare CPU vs GPU performance
```

**Utilities:**
```bash
make clean             # Remove build artifacts
make rebuild           # Clean and rebuild
make config            # Show build configuration
make help              # Show all targets
make deps              # Check dependencies
```

## Interactive Controls

### Camera Movement
- **WASD** - Move forward/left/backward/right
- **Arrow Keys** - Move up/down
- **Mouse** - Look around (when captured)
- **Left Click** - Capture/release mouse

### Rendering Controls
- **1-6** - Quality levels (320x180 to 1920x1080)
- **M** - Cycle analysis modes (Normal/Normals/Depth/Albedo)
- **Space** - Pause/resume rendering
- **S** - Save screenshot (PNG)
- **C** - Toggle controls panel
- **H** - Toggle help overlay
- **ESC** - Quit

### Settings Panel
- Quality level buttons (1-6)
- Samples per pixel (1, 4, 8, 16)
- Max depth (1, 3, 5, 8)
- Resolution presets (Low to Max)
- Feature toggles (Shadows, Reflections)
- Advanced rendering modes (Progressive, Adaptive, Wavefront)
- Debug modes (Normals, Depth, Albedo)
- Screenshot button

## Known Issues

### GPU Rendering
- GPU rendering is experimental
- May not work on all systems
- Requires OpenGL 2.0+ (GLSL 1.20)
- Check GPU compatibility if issues arise

### High Sample Counts
- Samples > 8 at high resolutions can cause crashes
- Conservative limit: 50M rays total
- Estimated memory limit: 500MB

### Performance
- High resolutions (1920x1080) may be slow on CPU
- Use GPU mode for better performance
- Lower quality levels for smoother interaction

## Development Guidelines

### Adding New Features
1. Add feature toggle to Renderer class if needed
2. Implement rendering logic in renderer.cpp
3. Add UI controls to main_cpu_interactive.cpp ControlsPanel
4. Update settings panel with new controls
5. Test with various quality levels
6. Document in CHANGELOG.md

### Performance Guidelines
- Profile before optimizing
- Use OpenMP for parallelization
- Consider cache coherence
- Test with different thread counts
- Measure MRays/sec throughput

### Testing Guidelines
- Test with Cornell Box scene
- Verify visual correctness
- Check for memory leaks
- Test all quality levels
- Verify interactive responsiveness

## Platform-Specific Notes

### macOS
- Homebrew dependencies: `brew install gcc sdl2 sdl2_ttf`
- OpenMP via: `/usr/local/opt/libomp`
- System fonts: `/System/Library/Fonts/`

### Linux
- Package manager dependencies vary
- System fonts: `/usr/share/fonts/truetype`

### Windows
- MinGW-w64 or MSVC supported
- vcpkg recommended for dependencies
- Font paths differ significantly

## Memory Management

### Framebuffers
- CPU framebuffer: `std::vector<unsigned char>`
- Analysis buffers: Separate buffers for normals/depth/albedo
- Texture updating: SDL_LockTexture/SDL_UnlockTexture

### Thread Safety
- OpenMP handles thread safety for rendering
- Each thread writes to disjoint pixels
- No mutex locks needed for framebuffer writes

## Compiler Flags

### Optimization Flags
```makefile
-O3                    # Maximum optimization
-march=native          # CPU-specific instructions
-mavx2                 # AVX2 SIMD
-mfma                  # Fused multiply-add
-ffast-math            # Aggressive FP optimizations
-fopenmp               # OpenMP multi-threading
-flto                  # Link-time optimization
```

### Warning Flags
```makefile
-Wall -Wextra          # All warnings
-Wno-missing-field-initializers
-Wno-deprecated-declarations
```

## Documentation

### Key Documentation Files
- **README.md** - Project overview and quick start
- **INTERACTIVE_GUIDE.md** - Interactive mode guide
- **CHANGELOG.md** - Development history
- **docs/index.md** - Main documentation index
- **docs/cpu-performance-results.md** - Performance benchmarks
- **docs/GPU_RENDERER.md** - GPU implementation and performance
- **docs/ASCII_RENDERER.md** - ASCII mode documentation

### Keeping Documentation Updated
When adding features:
1. Update CHANGELOG.md with description
2. Add performance measurements
3. Update relevant docs/*.md files
4. Update LLM_CONTEXT.md if architectural change

## References

### Learning Resources
- [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [PBRT](https://www.pbrt.org/)

### Project Documentation
- [docs/index.md](docs/index.md) - Main documentation
- [docs/cpu-performance-optimization-plan.md](docs/cpu-performance-optimization-plan.md) - Optimization strategies

## Future Work

### Short Term
- True variance-based adaptive sampling
- BVH acceleration structure for complex scenes
- More material types
- Scene file format (JSON/glTF)

### Long Term
- Denoising support
- More analysis modes
- Path tracing integration
- Advanced lighting models

## Git Workflow

### Commit Strategy
- Commit frequently after each feature
- Use descriptive commit messages
- Test before committing
- Keep commits atomic

### Branch Strategy
- `main` - Stable development
- Feature branches for major work
- GPU rendering work separate from CPU

## Troubleshooting

### Common Issues

**Black Screen**
- Check enable_shadows and enable_reflections
- Verify scene setup
- Check camera position

**Crashes**
- Reduce sample count
- Lower resolution
- Check memory usage
- Verify OpenMP threads

**Performance Issues**
- Check thread count: `omp_get_max_threads()`
- Verify AVX2 support
- Profile with specific quality levels
- Check for unnecessary allocations

**GPU Issues**
- Verify OpenGL version (2.0+ required)
- Check GLSL version compatibility
- Test with CPU mode first
- Update graphics drivers

## Important Notes for AI Assistants

### Active Development Context
- Multiple instances may be working simultaneously
- GPU instance: Focus on GPU rendering features
- CPU/ASCII instance: Focus on CPU optimizations and ASCII
- This instance: Focus on documentation and cleanup

### Coordination
- Check git status before editing files
- Look for recent commits to understand what's changing
- Be aware of file renames (e.g., main_interactive.cpp → main_cpu_interactive.cpp)
- Don't interfere with active development work

### File Modifications
- Safe to edit: Documentation (*.md), build files (Makefile)
- Check before editing: Source files (may have active work)
- Always read: git status, CHANGELOG.md, recent commits

### Understanding the Codebase
- Start with docs/index.md for overview
- Check CHANGELOG.md for recent changes
- Look at performance results for optimization context
- Use grep to find feature implementations
