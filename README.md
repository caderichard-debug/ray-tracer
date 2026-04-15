# SIMD Ray Tracer

A high-performance ray tracer built from scratch in C++ with AVX2 SIMD vectorization and OpenMP multi-threading. Features both batch rendering and real-time interactive modes.

## Status

✅ **Phase 1:** Foundation (Scalar math, geometry, materials)
✅ **Phase 2:** Basic Rendering (Phong shading, shadows, reflections)
✅ **Phase 4:** Advanced Features (triangles, anti-aliasing, PNG output)
✅ **Phase 5:** Multi-threading (OpenMP)
✅ **Phase 6:** Polish (CLI arguments, PNG output)
✅ **Interactive Mode:** Real-time rendering with camera controls

## Features

### ✅ Implemented
- **Batch Rendering**: High-quality offline rendering
  - Vec3 math library with complete vector operations
  - Ray representation and ray-sphere/triangle intersection
  - Material system (Lambertian diffuse, Metal reflective)
  - Perspective camera model
  - Scene graph with multiple primitives and lights
  - Phong shading (ambient + diffuse + specular)
  - Hard shadows via shadow rays
  - Recursive reflections (configurable depth)
  - Gamma correction
  - Anti-aliasing (supersampling up to 256 samples)
  - Triangle primitive with Möller-Trumbore intersection
  - PNG output (stb_image_write)
  - CLI arguments (resolution, samples, depth, output)
  - OpenMP multi-threading (8 threads)

- **Interactive Mode**: Real-time exploration
  - SDL2-based window system with full color graphics
  - WASD + mouse camera controls
  - 6 quality levels (320x180 to 1920x1080)
  - Real-time FPS display
  - Dynamic quality switching
  - Pause/resume rendering
  - Advanced rendering features (progressive, adaptive, wavefront)

- **ASCII Terminal Mode**: Retro text-based rendering
  - Pure terminal rendering (no GUI required)
  - Automatic camera animation (orbits scene)
  - Real-time ASCII art output
  - Cross-platform (macOS/Linux/Windows)
  - Adaptive terminal sizing
  - Quality presets (1-3)

## Quick Start

### Prerequisites

This project requires the following dependencies:

**Core Requirements:**
- **C++17 compiler** (g++, clang++, or MSVC)
- **x86_64 CPU** with AVX2 support (Intel Haswell+ or AMD Ryzen+)
- **OpenMP** (usually included with compiler)
- **SDL2** (for interactive mode)
- **SDL2_ttf** (for UI text rendering)
- **Make** (or CMake)

#### Installing Dependencies by Platform

**macOS (Homebrew):**
```bash
# Install compiler and dependencies
brew install gcc sdl2 sdl2_ttf

# OpenMP is included with GCC
```

**Ubuntu/Debian:**
```bash
# Install all dependencies
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev libomp-dev

# OpenMP is included with GCC
```

**Windows (MinGW-w64):**
```bash
# Install MinGW-w64 from https://www.mingw-w64.org/
# Or use MSYS2: https://www.msys2.org/

# In MSYS2 terminal:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf

# Or use vcpkg (recommended for Windows):
# Install vcpkg from https://github.com/Microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.bat
./vcpkg/vcpkg install sdl2 sdl2-ttf
```

**Windows (Visual Studio):**
```bash
# Install Visual Studio Community (free)
# Install "Desktop development with C++" workload

# Install vcpkg for dependencies
git clone https://github.com/Microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install sdl2 sdl2-ttf

# Then open the project in Visual Studio
```

**Alternative: Using vcpkg (Cross-platform)**
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh  # or .bat on Windows

# Install dependencies
./vcpkg install sdl2 sdl2-ttf

# Integrate with your build system
./vcpkg integrate install
```

### Build & Run

```bash
# Build batch ray tracer
make phase2

# Run batch ray tracer with default settings
./raytracer

# Run with custom settings
./raytracer -w 1920 -s 64 -d 8 -o my_scene

# Build and run interactive ray tracer (SDL2 graphics)
make interactive
make runi

# Build and run ASCII terminal ray tracer (retro style)
make ascii
make runa
```

**⚠️ IMPORTANT:** When the interactive ray tracer starts, **press H** immediately to see the help overlay with all controls! The help overlay shows:

- **Movement:** WASD + Arrow keys to move, mouse to look around
- **Quality:** Press 1-6 to change quality levels  
- **Screenshot:** Press S to save screenshots
- **Controls:** Press C to toggle the controls panel
- **Navigation:** Click window to capture mouse for camera control
- **Quit:** Press ESC or H to close help overlay

## Makefile Targets

```bash
# Building
make phase1          # Build Phase 1 (foundation)
make phase2          # Build Phase 2 (rendering) [default]
make interactive     # Build interactive real-time ray tracer
make all             # Build all implemented phases

# Running
make run             # Build and run batch ray tracer
make runi            # Build and run interactive ray tracer
make test            # Run Cornell box test scene

# Utilities
make clean           # Remove build artifacts
make info            # Show build information
make docs            # Show documentation
make help            # Show all targets
```

## Performance

| Phase | Description | Speedup | MRays/sec |
|-------|-------------|---------|-----------|
| 1-2 | Scalar baseline | 1x | ~0.8-1.5 |
| 3 | AVX2 SIMD | 4-6x | ~4-8 |
| 5 | + OpenMP (4 cores) | 14-20x | ~20-40 |

*Benchmarks on Intel Core i7 quad-core*

## Project Structure

```
ray-tracer/
├── README.md              # This file
├── Makefile               # Build system
├── CMakeLists.txt         # CMake config
├── docs/                  # Documentation
│   ├── index.md           # Main documentation index
│   ├── phase1-foundation.md
│   ├── phase2-rendering.md
│   ├── phase3-simd.md
│   ├── phase4-advanced.md
│   ├── phase5-multithreading.md
│   └── phase6-polish.md
├── src/
│   ├── main.cpp
│   ├── math/
│   │   ├── vec3.h              # Scalar Vec3
│   │   ├── vec3_avx2.h         # SIMD Vec3 (8-wide)
│   │   ├── ray.h
│   │   └── ray_packet.h        # 8-ray packets
│   ├── primitives/
│   │   ├── primitive.h
│   │   ├── sphere.h
│   │   └── sphere_simd.h       # SIMD intersection
│   ├── material/
│   │   └── material.h
│   ├── camera/
│   │   └── camera.h
│   ├── scene/
│   │   ├── scene.h
│   │   └── light.h
│   └── renderer/
│       ├── renderer.h
│       └── renderer.cpp
└── build/                  # Build artifacts
```

## Documentation

Comprehensive documentation is available in the [docs/](docs/) folder:

- **[Overview](docs/index.md)** - Project overview and roadmap
- **[Phase 1](docs/phase1-foundation.md)** - Mathematical foundation
- **[Phase 2](docs/phase2-rendering.md)** - Rendering pipeline
- **[Phase 3](docs/phase3-simd.md)** - SIMD vectorization
- **[Phase 4](docs/phase4-advanced.md)** - Advanced features (planned)
- **[Phase 5](docs/phase5-multithreading.md)** - Multi-threading (planned)
- **[Phase 6](docs/phase6-polish.md)** - Polish and deployment (planned)

## Technical Highlights

### SIMD Architecture
- **8-wide AVX2** processes 8 rays simultaneously
- **Structure of Arrays (SoA)** for efficient vectorization
- **Hybrid strategy:** SIMD for coherent rays, scalar for incoherent
- **Ray packets:** Primary rays (SIMD) + shadow rays (scalar)

### Rendering Pipeline
1. Generate primary rays from camera (SIMD packet of 8)
2. Find closest intersection (vectorized sphere test)
3. Calculate Phong shading
4. Cast shadow rays (scalar - incoherent)
5. Cast reflection rays (hybrid - semi-coherent)
6. Recurse until max depth reached

### Memory Layout
```cpp
// Array of Structures (AoS) - BAD for SIMD
struct Vec3AoS {
    float x, y, z;
};

// Structure of Arrays (SoA) - GOOD for SIMD
struct Vec3SoA {
    __m256 x;  // 8 x-coordinates
    __m256 y;  // 8 y-coordinates
    __m256 z;  // 8 z-coordinates
};
```

## Advanced Features (Planned)

### Geometry
- Triangles (Möller-Trumbore)
- Meshes (OBJ loading)
- Planes
- CSG operations

### Rendering
- Soft shadows (area lights)
- Anti-aliasing (supersampling)
- Depth of field
- Motion blur
- Path tracing

### Materials
- Glass (dielectric)
- Subsurface scattering
- Emissive
- Procedural textures

### Performance
- BVH acceleration structure
- OpenMP multi-threading
- Task-based parallelism

## Contributing

This is an educational project. Feel free to:
- Study the code and documentation
- Implement additional features
- Optimize performance
- Report bugs or issues

## References

- [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [PBRT](https://www.pbrt.org/)
- [SmallVCM](https://github.com/SmallVCM/SmallVCM)

## License

MIT License - Feel free to use for learning and experimentation.

## Authors

Built with guidance from Claude (Anthropic) and modern ray tracing techniques.

---

**Current Status:** Phase 3 (SIMD Vectorization) implemented with comprehensive documentation and Makefile workflow. Ready for advanced features and multi-threading.
