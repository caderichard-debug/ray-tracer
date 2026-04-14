# CPU Renderer Performance Optimization Plan

## Current State Analysis

**Current Performance:**
- Compiler flags: `-O3 -march=native -mavx2 -mfma -ffast-math`
- Parallelization: OpenMP with dynamic scheduling
- Ray tracing: Recursive with shadow rays for each light
- Samples: 16 (configurable)
- Max depth: 5 (configurable)

**Bottlenecks Identified:**
1. **Shadow ray overhead**: Every hit point shoots a shadow ray for each light
2. **Function call overhead**: Virtual function calls for materials, recursive ray_color calls
3. **Memory allocations**: Temporary Vec3 objects in hot paths
4. **Branch prediction failures**: Complex conditional logic in shading
5. **Cache misses**: Scene graph traversal causes random memory access

## Performance Optimization Strategies

### 1. Inline Critical Functions (Expected: 5-10% improvement)

**What:** Mark hot-path functions as `inline` to eliminate function call overhead

**Functions to inline:**
- `Renderer::ray_color()` - called 16 × pixels times
- `Renderer::compute_phong_shading()` - called for every hit
- `Scene::hit()` - called for every ray intersection test
- `Material::scatter()` - called for every reflection

**Implementation:**
```cpp
// In renderer.h
inline Color ray_color(const Ray& r, const Scene& scene, int depth) const;
inline Color compute_phong_shading(const HitRecord& rec, const Scene& scene) const;

// In scene.h
inline bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const;

// In material.h
virtual inline bool scatter(const Ray& r, const HitRecord& rec, Color& attenuation, Ray& scattered) const = 0;
```

### 2. Early Shadow Ray Culling (Expected: 15-25% improvement)

**What:** Skip shadow rays for surfaces facing away from the light (backface culling)

**Logic:**
```cpp
// Before shooting shadow ray
Vec3 to_light = light.position - rec.p;
float dot_product = dot(rec.normal, to_light);

if (dot_product > 0) {
    // Surface faces toward light - shoot shadow ray
    Ray shadow_ray(rec.p, light_dir_normalized);
    bool in_shadow = scene.is_shadowed(shadow_ray, light_distance);
    // ... compute shading
} else {
    // Surface faces away - no shadow needed, use only ambient
    color = scene.ambient_light * rec.mat->albedo;
}
```

**Expected Impact:** Eliminates ~50% of shadow rays (backfaces)

### 3. Fast Random Number Generation (Expected: 5-10% improvement)

**What:** Replace `rand()` with faster XOR-shift or PCG random for anti-aliasing

**Implementation:**
```cpp
// Fast XOR-shift RNG (inline, no state)
inline float fast_random_float(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return (seed & 0xFFFFFF) / 16777216.0f;
}
```

**Usage:** Thread-local seed instead of global `rand()`

### 4. Optimize Sphere Intersection (Expected: 10-15% improvement)

**What:** Simplify and inline ray-sphere intersection math

**Current bottleneck:**
- Multiple `sqrt()` calls
- Complex quadratic formula with branches
- Temporary Vec3 allocations

**Optimizations:**
- Pre-compute radius² once (during scene setup)
- Use early exit for obvious misses
- Inline the intersection test
- Minimize branching

### 5. Reduce Allocations in Hot Paths (Expected: 5-10% improvement)

**What:** Eliminate temporary object allocations in ray tracing loops

**Changes:**
- Use stack-allocated HitRecord instead of dynamic allocations
- Pre-allocate Vec3 objects where possible
- Avoid unnecessary Vec3 copying

### 6. Optimize Scene Traversal (Expected: 20-40% improvement for complex scenes)

**What:** Add Bounding Volume Hierarchy (BVH) for complex scenes

**For current scene (10 spheres):**
- Not worth the overhead
- Linear scan is actually faster for small N (< 20 objects)

**Future optimization:** Implement BVH for scenes with 50+ objects

### 7. SIMD Vectorization (Expected: 10-20% improvement)

**What:** Process 4-8 rays simultaneously using AVX2 intrinsics

**Approach:**
- Vectorize ray-sphere intersection tests
- Process multiple pixels in parallel
- Use AVX2 for floating-point operations

**Implementation complexity:** High
**Benefit:** Best for batch processing of coherent rays

### 8. Compiler Optimizations (Expected: 5-10% improvement)

**Add to Makefile:**
```makefile
CXXFLAGS += -flto                    # Link-time optimization
CXXFLAGS += -funroll-loops           # Unroll small loops
CXXFLAGS += -finline-limit=1000      # Aggressive inlining
CXXFLAGS += -ffast-math              # Already enabled, ensure stays
CXXFLAGS += -fno-omit-frame-pointer   # Better debugging, minimal perf cost
```

## Recommended Implementation Order

### Phase 1: Quick Wins (1-2 hours)
1. ✅ Inline critical functions (5-10%)
2. ✅ Early shadow ray culling (15-25%)
3. ✅ Fast random number generation (5-10%)

**Expected total improvement: 25-45%**

### Phase 2: Medium Effort (2-3 hours)
4. Optimize sphere intersection (10-15%)
5. Reduce allocations (5-10%)
6. Compiler optimization flags (5-10%)

**Expected additional improvement: 20-35%**

### Phase 3: Advanced (4-6 hours)
7. SIMD vectorization (10-20%)
8. BVH for larger scenes (20-40% for 50+ objects)

**Expected additional improvement: 30-60%**

## Performance Targets

**Current baseline:**
- 800x450 resolution
- 16 samples per pixel
- ~2-5 seconds per frame
- ~1-10 MRays/sec

**Target after Phase 1:**
- 0.8-1.5 seconds per frame (2-3x faster)
- ~3-8 MRays/sec

**Target after Phase 2:**
- 0.5-0.8 seconds per frame (4-6x faster overall)
- ~6-15 MRays/sec

**Target after Phase 3:**
- 0.2-0.4 seconds per frame (8-15x faster overall)
- ~15-50 MRays/sec

## Testing Strategy

**Benchmarking:**
1. Render Cornell box scene at 800x450 with 16 samples
2. Measure wall-clock time with `std::chrono`
3. Count total rays traced
4. Calculate MRays/sec (millions of rays per second)

**Validation:**
- Visual comparison of output (must be identical)
- Numerical comparison (allow 0.1% tolerance for floating-point)
- Performance regression testing

## Risk Assessment

**Low risk:**
- Inlining functions
- Fast RNG
- Compiler flags

**Medium risk:**
- Early shadow culling (verify correctness)
- Optimize intersection (math must be exact)

**High risk:**
- SIMD vectorization (complex, potential for bugs)
- BVH implementation (significant code changes)

## Implementation Priority

**Start with Phase 1** (quick wins with high impact):
1. Inline critical functions
2. Early shadow ray culling
3. Fast random number generation

These give the best performance/effort ratio and can be implemented quickly.
