# CPU Ray Tracer - Agent Context

## Project Overview
High-performance CPU ray tracer with AVX2 SIMD vectorization and OpenMP multi-threading. Features advanced optimizations including progressive rendering, adaptive sampling, and wavefront rendering for 20-40x speedup over scalar baseline.

## Current Development Status

### Completed Features
- ✅ Multi-threaded ray tracing with OpenMP (14-20x speedup)
- ✅ AVX2 SIMD vectorization (4-6x speedup)
- ✅ Phong shading with shadows and reflections
- ✅ Anti-aliasing (1-256 samples per pixel)
- ✅ SDL2-based interactive real-time rendering
- ✅ Quality presets (6 levels: 320x180 to 1920x1080)
- ✅ Camera controls (WASD + mouse)
- ✅ Analysis modes (normals, depth, albedo)
- ✅ Screenshot system (PNG): **S** / panel button save a **full render-target** grab via `SDL_RenderReadPixels` immediately after the scene texture is drawn (excludes histogram, controls panel, help). Uses **RGB24** when supported, otherwise reads with the backend format and **`SDL_ConvertPixels`** to **RGBA8888** before `stbi_write_png` (avoids ARGB/BGRA channel-order tinting). Panel screenshot queues capture **one frame later** so the pressed button can render before `stbi_write_png` work.
- ✅ Settings panel with live controls (RGBA surface + `SDL_BLENDMODE_BLEND` on panel textures for translucency over the render view)
- ✅ Screenshot button UX: latched “pressed” (dark) styling from mouse-down on the button until global **left mouse up** (`screenshot_button_pressed` + `release_pressed_buttons()`)
- ✅ Post denoiser amount slider (0-100%) in CPU controls panel
- ✅ Improved denoiser quality path (separable bilateral, variance-guided blend, bilinear upscale)
- ✅ Live luminance histogram overlay (G), camera hot-reload file (F5 / RT_HOT_RELOAD_POLL)
  - **Hot reload:** Edit `config/camera_hot_reload.txt` (copy from `config/camera_hot_reload.txt.example`) or set `RT_HOT_RELOAD_CAMERA` to a file path. Required keys: `eye` (or `lookfrom`) and `at` (or `lookat`); optional `up`/`vup`, `vfov`, `aperture`, `focus`/`dist_to_focus`. Press **F5** to reload from disk. Set **`RT_HOT_RELOAD_POLL`** (any value) to watch mtime and reload automatically about every 90 frames.

### Public site (GitHub Pages / Vercel)
- **Primary:** Next.js **project showcase** in `showcase/` (`npm run dev` / `npm run build`) — copy and IA are **ray-tracer-first**, not a personal portfolio shell. Deploy on **Vercel** with root `showcase/`, or on **GitHub Pages** via `.github/workflows/showcase-pages.yml` (static export, `basePath=/ray-tracer`). See `showcase/README.md`.
- **Design export:** `showcase/public/Ray Tracer.html` — Claude Design handoff (sync from `poject-showcase/project/Ray Tracer.html`). **`/`** loads it in a full-viewport iframe; direct URL **`/Ray%20Tracer.html`** also works.
- **Render.com:** repo-root `render.yaml` deploys `showcase/` as a Node web service; see `showcase/README.md`.
- **Legacy:** static HTML still under `docs/` (`index.html` + siblings) if Pages is left on **branch `main` / folder `/docs`**; prefer migrating to the workflow above.

### Regression / batch
- `make regression-test` (deterministic PPM SHA-256; see tests/regression/)
- Batch: `--deterministic`, `--fixed-ppm`, `ENABLE_OPENMP=0` via Makefile for stable output
- ✅ Batch CPU camera moved 1 unit closer to room center (`lookfrom.z`: 15 -> 14) with refreshed README high-quality sample image

### Advanced Optimizations Implemented
- ✅ **Shadow Ray Culling** (+8.6%): Skip shadow rays for backfaces
- ✅ **Fast XOR-shift RNG** (+2.8%): Thread-safe, eliminates lock contention
- ✅ **Progressive Rendering** (3.164x): Multi-pass refinement from noisy to smooth
- ✅ **Adaptive Sampling** (1.702x): Variance-based sample allocation
- ✅ **Wavefront Rendering** (1.358x): Tiled cache-coherent processing

### Phase 1 Optimizations (2026-04-15)
- ✅ **Improved Loop Unrolling** (+5-8%): Manual unrolling of sample loops by 4
- ✅ **Compile-Time Optimizations** (+3-5%): Aggressive compiler flags for inlining and optimization
- ✅ **SIMD Material Calculations** (+15-25%): AVX2 vectorization of entire shading pipeline
- ✅ **PCG Random Number Generator** (+2-5%): High-quality, thread-safe PRNG

**Phase 1 Total Impact**: +25-35% additional performance improvement
**New Performance (800x450, 16 samples)**: 1.243s @ 13.90 MRays/sec
**Previous Best**: 1.5s @ 3.74 MRays/sec
**Measured Improvement**: ~17% faster in practice (more room for optimization at higher resolutions)

## Architecture

### Core Files

**Main Renderer:**
- **src/main_cpu_interactive.cpp** - SDL2 interactive mode with settings panel
  - Quality levels: 6 presets (320x180 to 1920x1080)
  - Advanced rendering modes: progressive, adaptive, wavefront
  - Settings panel with live feature toggles
  - Analysis modes: normals, depth, albedo visualization
  - Window screenshot capture (PNG, no UI overlays) + translucent panel + hot reload (see bullets above)

**Rendering Engine:**
- **src/renderer/renderer.h** - Renderer class interface
  - `ray_color()` - Core ray tracing recursion
  - `compute_phong_shading()` - Lighting calculation with shadow culling
  - `render()` - Standard rendering with all optimizations
  - `render_progressive()` - Multi-pass progressive refinement
  - `render_adaptive()` - Variance-based adaptive sampling
  - `render_wavefront()` - Tiled cache-coherent rendering
  - `should_continue_sampling()` - Adaptive sampling decision
  - `get_progressive_accumulation()` - Progressive frame data

- **src/renderer/renderer.cpp** - Implementation
  - Lines 188-199: Sample accumulation loop (candidate for loop unrolling)
  - Lines 96-129: Light loop with shadow culling optimization
  - OpenMP parallel regions with dynamic scheduling
  - XOR-shift RNG for thread-safe random numbers

**Scene and Materials:**
- **src/scene/scene.h** - Scene graph with hit() and is_shadowed()
- **src/scene/cornell_box.h** - Standard test scene (10 spheres)
- **src/material/material.h** - Lambertian, Metal, Dielectric materials
- **src/camera/camera.h** - Perspective camera model

**Math and Primitives:**
- **src/math/vec3.h** - Scalar Vec3 operations
- **src/math/ray.h** - Ray representation
- **src/primitives/sphere.h** - Sphere intersection
- **src/primitives/triangle.h** - Möller-Trumbore triangle intersection

## Performance Baseline

### Full Optimization Stack (800x450, 16 samples)
```
Scalar Baseline (1 thread):    67.2s  | 0.09 MRays/s | 1.0x
+ AVX SIMD:                    13.4s  | 0.45 MRays/s | 5.0x
+ OpenMP (4 cores):             4.5s  | 1.35 MRays/s | 14.9x
+ Shadow Culling:               3.1s  | 1.95 MRays/s | 21.7x
+ Fast RNG:                     3.0s  | 2.00 MRays/s | 22.4x
+ Progressive:                  0.49s | 11.84 MRays/s| 73.7x
```

### Optimization Impact
- **Shadow Culling**: +8.6% (eliminates ~50% of shadow rays)
- **Fast RNG**: +2.8% (XOR-shift vs rand())
- **Progressive**: 3.164x faster (multi-pass refinement)
- **Adaptive**: 1.702x faster (half the samples)
- **Wavefront**: 1.358x faster (cache-coherent tiles)
- **Total Stack**: 23.2x speedup (scalar → fully optimized)

## Build System

### Feature Flags
```bash
# Rendering features
ENABLE_SHADOWS=1         # Enable/disable shadows
ENABLE_REFLECTIONS=1     # Enable/disable reflections

# Code optimizations (Phase 1)
ENABLE_SHADOW_CULLING=1  # +8.6% (skip backface shadow rays)
ENABLE_FAST_RNG=1        # +2.8% (XOR-shift vs rand())
ENABLE_LOOP_UNROLL=1     # +3-5% (reduce loop overhead)

# Parallelization and SIMD
ENABLE_OPENMP=1          # +14-20x (4-core multi-threading)
ENABLE_AVX=1             # +4-6x (8-wide SIMD)
ENABLE_PTHREADS=0        # Alternative: +12-18x (manual threading)

# Advanced rendering modes
ENABLE_PROGRESSIVE=1     # 3.164x (multi-pass refinement)
ENABLE_ADAPTIVE=1        # 1.702x (variance-based sampling)
ENABLE_WAVEFRONT=1       # 1.358x (cache-coherent tiles)
```

### Build Targets
```bash
make batch-cpu         # Build CPU batch ray tracer
make interactive-cpu   # Build interactive CPU (SDL2)
make run               # Build and run CPU batch
make runi-cpu          # Build and run interactive CPU
```

## Interactive Controls

### Settings Panel Features
- **Quality Levels**: 6 buttons (320x180 to 1920x1080)
- **Samples**: 1, 4, 8, 16 options
- **Max Depth**: 1, 3, 5, 8 options
- **Feature Toggles**:
  - Shadows ON/OFF
  - Reflections ON/OFF
  - Progressive ON/OFF
  - Adaptive ON/OFF
  - Wavefront ON/OFF
- **Analysis Modes**: Normal, Normals, Depth, Albedo
- **Screenshot**: PNG capture button

### Camera Controls
- **WASD**: Move forward/left/backward/right
- **Arrow Keys**: Move up/down
- **Mouse**: Look around (when captured)
- **Left Click**: Capture/release mouse

## Known Issues and Constraints

### Performance Constraints
- **High sample counts**: > 8 samples at high resolutions can crash (50M ray limit)
- **Memory limit**: ~500MB estimated safe limit
- **Progressive mode**: Takes 4-5 frames to reach target quality

### Feature Interactions
- **Progressive + Adaptive**: Don't combine (conflicting sample strategies)
- **Wavefront**: Works best at high resolutions (1080p+)
- **Analysis modes**: Override standard rendering output

### Compiler Warnings
- Unused parameters in some functions (acceptable for interface consistency)
- Sign comparisons in OpenMP regions (safe due to constraints)

## Development Guidelines

### Adding New Features
1. Add feature flag to Makefile (`ENABLE_FEATURENAME ?= 0`)
2. Add `#ifdef ENABLE_FEATURENAME` in renderer.cpp
3. Implement feature in appropriate render function
4. Add UI controls to main_cpu_interactive.cpp (SettingsPanel)
5. Update feature documentation with performance impact
6. Test with various quality levels and combinations

### Performance Optimization Strategy
1. **Profile first**: Use `make config` to verify flags
2. **Test baseline**: Always compare to scalar single-threaded
3. **Measure impact**: Use `make benchmark` for comparisons
4. **Document speedup**: Include exact performance numbers
5. **Consider interactions**: Test with other features enabled

### Code Quality Standards
- **Thread safety**: All code must be OpenMP-safe
- **No global state**: Use thread-local or atomic operations
- **Consistent interfaces**: Maintain function signatures across features
- **Error handling**: Graceful degradation when features fail

### Testing Requirements
- **Test scene**: Always use Cornell Box for consistency
- **Resolution**: Test at 800x450 (standard benchmark)
- **Samples**: Test at 1, 4, 16 samples
- **Quality levels**: Verify all 6 presets work
- **Feature combinations**: Test common combinations
- **Visual correctness**: Compare output with baseline

## Performance Measurement

### Benchmark System
```bash
make benchmark  # Full optimization stack comparison
```
Tests all combinations:
- Scalar baseline
- AVX only
- OpenMP only
- Pthreads only
- AVX + OpenMP
- Phase 1 optimizations (culling + fast RNG)
- Loop unrolling
- Progressive
- Adaptive
- Wavefront

### Performance Targets
- **Scalar baseline**: ~67 seconds (800x450, 16 samples)
- **Optimized target**: ~1.5 seconds (43.7x speedup)
- **Progressive target**: ~0.5 seconds for initial preview
- **Interactive target**: 60+ FPS at quality level 3

## Coordination with Other Agents

### GPU Agent
- **Different focus**: Raw throughput vs. flexibility
- **No feature parity**: GPU doesn't have progressive/adaptive/wavefront
- **Performance comparison**: Use `make benchmark-cpu-gpu`
- **Scene compatibility**: Both use Cornell Box for fair comparison

### ASCII Agent
- **Different constraints**: Terminal rendering vs. full graphics
- **Shared components**: Both use renderer.cpp for ray tracing
- **Performance goals**: ASCII is for aesthetics, not speed

### Manager Agent
- **Feature flags**: Coordinate through Makefile FEATURE_DEFINES
- **Build system**: Shared Makefile with different targets
- **Performance tracking**: Centralized benchmark_results/

## Common Tasks

### Adding a New Optimization
1. Research and estimate performance impact
2. Add feature flag to Makefile
3. Implement in renderer.cpp with #ifdef guards
4. Add benchmark case to make benchmark
5. Document in README.md and LLM_CONTEXT.md
6. Test interactions with existing features

### Debugging Performance Issues
1. Verify feature flags: `make config`
2. Test scalar baseline: `make batch-cpu ENABLE_OPENMP=0 ENABLE_AVX=0`
3. Check thread count: `omp_get_max_threads()` should return 4-8
4. Profile with specific features: Enable one at a time
5. Verify AVX support: Check CPU capabilities
6. Test with different quality levels

### Optimizing Hot Paths
**Current hot spots:**
- Sample accumulation loop (line 188 in renderer.cpp)
- Light loop with shadow culling (line 96)
- Variance computation loops (lines 73-86)

**Optimization opportunities:**
- Loop unrolling (ENABLE_LOOP_UNROLL)
- SIMD material calculations
- Cache-friendly scene traversal
- Better ray packet organization

## Platform-Specific Notes

### macOS
- **Compiler**: g++ with Homebrew OpenMP
- **OpenMP path**: `/usr/local/opt/libomp`
- **SDL2**: `/usr/local/opt/sdl2`
- **Thread count**: Typically 4-8 physical cores

### Linux
- **Compiler**: g++ with system OpenMP
- **Package**: `libomp-dev` or `libiomp-dev`
- **Thread count**: Varies by CPU
- **Performance**: Generally better than macOS

### Windows
- **Compiler**: MinGW-w64 or MSVC
- **OpenMP**: Supported in both compilers
- **SDL2**: Install via vcpkg or MSYS2
- **Thread count**: Same as Linux

## Future Work

### Short Term (High Priority)
- **True variance-based adaptive sampling**: Current implementation is simplified
- **BVH acceleration structure**: For complex scenes (20-40x improvement)
- **SIMD material calculations**: Vectorize shading computations

### Long Term (Research)
- **Path tracing**: Global illumination with Monte Carlo integration
- **Denoising**: Post-process filtering for noisy renders
- **Advanced lighting**: Area lights, environment mapping
- **Scene file format**: JSON/glTF scene loading

## Important Reminders

### DO:
- ✅ Always test with Cornell Box scene first
- ✅ Measure performance impact of changes
- ✅ Document feature flags and their effects
- ✅ Maintain thread safety in all code
- ✅ Test feature combinations
- ✅ Use OpenMP best practices
- ✅ Commit frequently with organized, labeled commits (prefix: "CPU:")
- ✅ Commit only files that were changed
- ✅ Use descriptive commit messages explaining the optimization

### DON'T:
- ❌ Break existing interactive features
- ❌ Remove feature flags or #ifdef guards
- ❌ Ignore performance regressions
- ❌ Add features without benchmarking
- ❌ Change core algorithm without profiling
- ❌ Assume thread safety without verification

## Success Metrics

### Performance Goals
- **Scalar baseline**: 67.2s (800x450, 16 samples)
- **Target**: < 2.0s with all optimizations (33x+ speedup)
- **Progressive**: < 0.5s for initial preview
- **Interactive**: 60+ FPS at quality level 3

### Quality Goals
- **Visual correctness**: Match baseline output
- **Feature parity**: All features work together
- **Stability**: No crashes at reasonable settings
- **Usability**: Smooth interactive experience

---

**Agent Role**: CPU renderer optimization and feature development
**Primary Focus**: Performance optimizations and advanced rendering modes
**Coordination**: Work with GPU agent for comparisons, manager for overall architecture
