# SIMD Ray Tracer - Claude Context

## Project Overview
A high-performance ray tracer built from scratch in C++ with AVX2 SIMD vectorization and OpenMP multi-threading. Features both batch rendering and real-time interactive modes with quality switching.

## Architecture

### Build System
- **Primary**: Makefile with phase-based builds
- **Compiler**: g++ with C++17
- **Flags**: -O3 -march=native -mavx2 -mfma -ffast-math -fopenmp
- **Dependencies**: SDL2, SDL2_ttf, OpenMP

### Project Structure
```
ray-tracer/
├── src/
│   ├── math/           # Vec3, Ray, SIMD operations
│   ├── primitives/     # Sphere, Triangle, Primitive base
│   ├── material/       # Lambertian, Metal, Dielectric
│   ├── camera/         # Camera with perspective projection
│   ├── scene/          # Scene graph, Lights, Cornell Box
│   ├── renderer/       # Ray tracing logic, GPU renderer
│   └── texture/        # Texture system
├── docs/               # Comprehensive documentation
├── build/              # Build artifacts
└── Makefile           # Build system
```

## Key Files

### Core Renderer
- **src/renderer/renderer.h** - Renderer class with ray_color() and compute_phong_shading()
- **src/renderer/renderer.cpp** - Implementation with shadow ray culling and OpenMP
- **src/main_cpu_interactive.cpp** - SDL2-based interactive mode with controls (CPU)
- **src/main_gpu_interactive.cpp** - OpenGL-based interactive mode with controls (GPU)

### Scene and Materials
- **src/scene/scene.h** - Scene graph with hit() and is_shadowed()
- **src/material/material.h** - Lambertian, Metal, Dielectric materials
- **src/scene/cornell_box.h** - Standard test scene setup

### Math and Primitives
- **src/math/vec3.h** - Scalar Vec3 operations
- **src/math/ray.h** - Ray representation
- **src/primitives/sphere.h** - Sphere intersection

## Current Features

### Implemented
- ✅ Multi-threaded ray tracing with OpenMP
- ✅ Phong shading with shadows and reflections
- ✅ Anti-aliasing with configurable samples
- ✅ Interactive mode with SDL2
- ✅ Quality presets (6 levels)
- ✅ Camera controls (WASD + mouse)
- ✅ Analysis modes (normals, depth, albedo)
- ✅ Screenshot system
- ✅ Settings panel with controls

### Performance Optimizations
- ✅ OpenMP dynamic scheduling
- ✅ Shadow ray culling (backface optimization)
- ✅ Early ray termination
- ✅ Inline critical functions

## Known Issues
- GPU rendering is experimental and may not work on all systems
- High sample counts (> 8) at high resolutions can cause crashes
- Some analysis modes may show artifacts

## Build Targets

### CPU Rendering
```bash
make interactive        # Build interactive CPU ray tracer
make runi              # Run interactive mode
```

### GPU Rendering (Experimental)
```bash
make interactive-gpu    # Build with GPU support
make runi-gpu          # Run with GPU rendering
```

## Interactive Controls

### Camera Movement
- **WASD** - Move forward/left/backward/right
- **Arrow Keys** - Move up/down
- **Mouse** - Look around (when captured)
- **Left Click** - Capture/release mouse

### Rendering Controls
- **1-6** - Quality levels
- **M** - Cycle analysis modes
- **Space** - Pause/resume rendering
- **S** - Save screenshot
- **C** - Toggle controls panel
- **H** - Toggle help overlay
- **ESC** - Quit

### Settings Panel
- Quality level buttons (1-6)
- Samples per pixel (1, 4, 8, 16)
- Max depth (1, 3, 5, 8)
- Resolution presets (Low to Max)
- Feature toggles (Shadows, Reflections)
- Debug modes (Normals, Depth, Albedo)
- Screenshot button

## Development Guidelines

### Adding New Features
1. Add feature toggle to Renderer class if needed
2. Implement rendering logic in renderer.cpp
3. Add UI controls to main_cpu_interactive.cpp ControlsPanel
4. Update settings panel with new controls
5. Test with various quality levels
6. Document in this file

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

## Current Work

### Active Development
- **GPU Renderer**: Another instance is working on GPU rendering
- **CPU Optimizations**: This instance is optimizing CPU renderer

### Planned CPU Optimizations
1. **Progressive Rendering** - Multi-pass refinement for immediate feedback
2. **Adaptive Sampling** - Variance-based sample allocation
3. **Wavefront Rendering** - Ray sorting for cache coherence
4. **Progressive Photon Mapping** - Global illumination with photons

## Performance Baseline

### Current Performance
- **Resolution**: 1280x720 (typical)
- **Samples**: 1-64 (configurable)
- **Throughput**: ~20-40 MRays/sec (4 cores)
- **Render time**: 0.5-2 seconds per frame (interactive)

### Optimization Targets
- Progressive rendering: < 100ms initial preview
- Adaptive sampling: 2-4x improvement
- Wavefront rendering: 1.5-2x improvement
- Combined: 3-8x total improvement

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

## Platform-Specific Notes

### macOS
- Homebrew dependencies: `brew install gcc sdl2 sdl2_ttf`
- OpenMP via: `/usr/local/opt/libomp`
- System fonts: `/System/Library/Fonts/`

### Linux
- Package manager dependencies vary
- System fonts: `/usr/share/fonts/truetype/`

### Windows
- MinGW-w64 or MSVC supported
- vcpkg recommended for dependencies
- Font paths differ significantly

## References

### Learning Resources
- [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [PBRT](https://www.pbrt.org/)

### Project Documentation
- [docs/index.md](docs/index.md) - Main documentation
- [docs/cpu-performance-optimization-plan.md](docs/cpu-performance-optimization-plan.md) - Optimization strategies
- [INTERACTIVE_GUIDE.md](INTERACTIVE_GUIDE.md) - Interactive mode guide

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

#### Black Screen
- Check enable_shadows and enable_reflections
- Verify scene setup
- Check camera position

#### Crashes
- Reduce sample count
- Lower resolution
- Check memory usage
- Verify OpenMP threads

#### Performance Issues
- Check thread count: `omp_get_max_threads()`
- Verify AVX2 support
- Profile with specific quality levels
- Check for unnecessary allocations

## Future Work

### Short Term
- Implement progressive rendering
- Add adaptive sampling
- Create performance benchmarking tools

### Long Term
- BVH acceleration structure
- More material types
- Scene file format (JSON/glTF)
- Denoising support
- More analysis modes