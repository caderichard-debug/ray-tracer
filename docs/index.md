# SIMD Ray Tracer - Implementation Guide

A high-performance ray tracer built from scratch in C++ with AVX2 SIMD vectorization and OpenMP multi-threading.

## Overview

This project implements a modern ray tracer with progressive complexity levels, from basic scalar ray casting to fully vectorized multi-threaded rendering with advanced shading features.

## Performance Targets

| Phase | Description | Speedup | MRays/sec |
|-------|-------------|---------|-----------|
| 1-2 | Scalar foundation | 1x | ~1-2 |
| 3 | AVX2 SIMD (8-ray packets) | 4-6x | ~6-10 |
| 5 | OpenMP (4 cores) | 3.5-3.8x | ~20-40 |
| Combined | SIMD + Multi-threading | 14-20x | ~20-40 |

## Implementation Phases

### [Phase 1: Foundation](phase1-foundation.md) ✓ COMPLETE
- Vec3 math library
- Ray representation
- Sphere primitive with analytic intersection
- Material system (Lambertian, Metal)
- Camera with perspective projection

**Build:** `make phase1`

### [Phase 2: Basic Rendering](phase2-rendering.md) ✓ COMPLETE
- Scene graph with multiple objects
- Point lights
- Phong shading (ambient + diffuse + specular)
- Shadow rays for hard shadows
- Recursive reflections
- Gamma correction
- PPM output

**Build:** `make phase2`

### [Phase 3: SIMD Vectorization](phase3-simd.md) 🚧 IN PROGRESS
- AVX2 Vec3 operations using `__m256`
- Ray packet structure (8 rays)
- Vectorized ray-sphere intersection
- Coherent ray traversal
- Hybrid scalar/SIMD strategy

**Build:** `make phase3`

### [Phase 4: Advanced Features](phase4-advanced.md) ⏳ PENDING
- Triangle primitive (Möller-Trumbore intersection)
- Plane primitive
- Complex scene support
- Soft shadows (multi-sample)
- Anti-aliasing (supersampling)

**Build:** `make phase4`

### [Phase 5: Multi-threading](phase5-multithreading.md) ⏳ PENDING
- OpenMP integration
- Tile-based parallelization
- Thread-safe framebuffer
- Load balancing
- Performance tuning

**Build:** `make phase5`

### [Phase 6: Polish](phase6-polish.md) ⏳ PENDING
- PNG output (stb_image_write)
- Tone mapping (Reinhard)
- Command-line arguments
- Example scenes
- Benchmarking tools

**Build:** `make phase6`

## Quick Start

```bash
# Build specific phase
make phase2  # Build current working version

# Run ray tracer
./raytracer > output.ppm

# Build all phases (as implemented)
make all

# Clean build artifacts
make clean
```

## Architecture

```
ray-tracer/
├── CMakeLists.txt          # CMake build config
├── Makefile                 # GNU Make workflow
├── src/
│   ├── math/               # Vec3, Ray, SIMD operations
│   ├── primitives/         # Sphere, Triangle, Plane
│   ├── material/           # Lambertian, Metal, Dielectric
│   ├── camera/             # Camera and viewport
│   ├── scene/              # Scene graph, Lights
│   └── renderer/           # Ray tracing logic
├── docs/                   # This documentation
└── external/               # External libraries (stb_image_write)
```

## Technical Highlights

### SIMD Strategy
- **Primary rays:** Processed in packets of 8 (AVX2 width)
- **Shadow rays:** Scalar (incoherent directions)
- **Reflection rays:** Hybrid approach based on coherence
- **Memory:** Structure of Arrays (SoA) for ray packets

### Rendering Features
- **Phong shading:** Ambient + Diffuse + Specular
- **Shadows:** Hard shadows with shadow rays
- **Reflections:** Recursive with configurable depth
- **Anti-aliasing:** Supersampling (4x4 grid)
- **Materials:** Lambertian, Metal, Dielectric

### Compiler Flags
```bash
-march=native          # Enable all CPU-specific instructions
-mavx2                 # AVX2 (256-bit SIMD)
-mfma                  # Fused multiply-add
-fopenmp               # OpenMP multi-threading
-O3                    # Maximum optimization
-ffast-math            # Aggressive FP optimizations
```

## Performance Benchmarks

Run on Intel Core i7 quad-core (your system):

```bash
make benchmark
```

Expected results:
- Scalar (Phase 2): ~1-2 MRays/sec
- SIMD (Phase 3): ~6-10 MRays/sec
- Multi-threaded (Phase 5): ~20-40 MRays/sec

## References

- [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [PBRT](https://www.pbrt.org/)
- [SmallVCM](https://github.com/SmallVCM/SmallVCM)

## License

MIT License - Feel free to use this code for learning and experimentation.
