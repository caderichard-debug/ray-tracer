# SIMD Packet Tracing - Debug Context

**Status**: ❌ **BROKEN - Feature Non-Functional**
**Last Updated**: 2025-04-15
**Priority**: High (Phase 3 deliverable)

## Overview

This document provides technical context for debugging the broken SIMD packet tracing feature. The implementation uses AVX2 intrinsics to process 8 rays simultaneously but has critical memory corruption bugs that make it non-functional.

## Expected Behavior

SIMD packet tracing should:
- Process 8 rays in parallel using AVX2 __m256 operations
- Achieve 4-6x speedup over scalar ray tracing
- Produce identical visual output to standard rendering
- Be toggleable via UI button (Settings → SIMD button)

## Current Problems

### 1. Memory Corruption (CRITICAL)
**Symptom**: Material pointer changes within a single function call

```
compute_phong_shading entry:  rec.mat->albedo = (0.65, 0.05, 0.05) ← RED material
diffuse calculation:          rec.mat->albedo = (0.73, 0.73, 0.73) ← GRAY material
```

**Debug Log Evidence**:
```
SIMD compute_phong_shading:
  albedo = (0.65, 0.05, 0.05)  ← Correct red material
  ambient = (0.065, 0.005, 0.005)  ← Correct calculation
...
  diffuse = (0.729, 0.729, 0.729)  ← WRONG (should be red-tinted)
  final = (0.802, 0.802, 0.802)  ← WRONG (gray instead of red)
```

**Why This Is Impossible**:
- HitRecord is passed by value to `compute_phong_shading()`
- AVX2 operations take Vec3 by value, not reference
- Materials are shared_ptr (should be immutable)
- **Something is corrupting memory**

### 2. Wrong Visual Output
- **No visible lighting** (user reported "no lighting")
- **Shapes look "way simpler"** (user feedback)
- **Wrong colors** (gray instead of red)
- **Performance regression** (3.x FPS → 0.x FPS)

### 3. Material Assignment Bug (PARTIALLY FIXED)
**Original Bug**: Materials were assigned during sphere testing loop, causing material/t-value mismatches.

```cpp
// WRONG: Material assigned with wrong t-value
for (sphere_idx = 0; sphere_idx < num_spheres; sphere_idx++) {
    // ... intersection test ...
    hit_records[i].mat = original_spheres[sphere_idx]->mat;  // BUG
    hit_records[i].t = global_minimum_t;  // Wrong t-value!
}
```

**Fix Applied**: Moved material assignment to after all spheres tested:
```cpp
// CORRECT: Track sphere indices, assign materials after testing
for (sphere_idx = 0; sphere_idx < num_spheres; sphere_idx++) {
    // ... intersection test ...
    if (closer_hit) {
        hit_sphere_indices[i] = sphere_idx;  // Track which sphere
        hit_records[i].t = t_arr[i];  // Correct t-value
    }
}

// Assign materials AFTER all spheres tested
for (int i = 0; i < 8; i++) {
    if (hit_sphere_indices[i] >= 0) {
        hit_records[i].mat = original_spheres[hit_sphere_indices[i]]->mat;
    }
}
```

**Status**: ✅ Fix applied, ❌ But memory corruption remains

## Architecture

### Data Flow

```
render_simd_packets()
  ↓
Generate 8 rays for 4x2 pixel block
  ↓
Build RayPacket (Vec3_AVX2: __m256 x, y, z)
  ↓
scene.hit_packet(packet, t_min, t_max, hit_records)
  ↓
For each sphere in scene:
  → simd_spheres[sphere_idx].hit_packet(packet, packet_hits)
  → Track closest hits using __m256 min operations
  → Store hit_sphere_indices[i] = sphere_idx
  ↓
Assign materials: hit_records[i].mat = original_spheres[sphere_idx]->mat
  ↓
Compute positions/normals
  ↓
For each of 8 rays:
  → compute_phong_shading(hit_records[r], scene)  ← MEMORY CORRUPTION HERE
  ↓
Accumulate to framebuffer
```

### Key Data Structures

**Vec3_AVX2** ([src/math/vec3_avx2.h](src/math/vec3_avx2.h)):
```cpp
struct alignas(32) Vec3_AVX2 {
    __m256 x;  // 8 x-coordinates
    __m256 y;  // 8 y-coordinates
    __m256 z;  // 8 z-coordinates
};
```

**RayPacket** ([src/math/ray_packet.h](src/math/ray_packet.h)):
```cpp
struct RayPacket {
    Vec3_AVX2 origins;
    Vec3_AVX2 directions;
    __m256 t_min;
    __m256 t_max;
    int valid_mask;
    int size;
};
```

**HitRecordPacket** (from sphere_simd.h):
```cpp
struct HitRecordPacket {
    __m256 t;                    // 8 t-values
    Vec3_AVX2 positions;         // 8 positions
    Vec3_AVX2 normals;           // 8 normals
    __m256 valid;                // 8 validity masks
};
```

## Files Involved

### Core SIMD Implementation
- **src/math/vec3_avx2.h** - AVX2 vector operations (8-wide Vec3)
- **src/math/ray_packet.h** - Ray packet structure (8 rays)
- **src/primitives/sphere_simd.h** - AVX2 sphere intersection
- **src/scene/scene.h** - Scene::hit_packet() (lines 70-217)

### Rendering
- **src/renderer/renderer.cpp** - render_simd_packets() (lines 357-500)
- **src/main_cpu_interactive.cpp** - UI toggle button

### Debug Output
- **simd_debug.log** - Debug log file (auto-generated)

## What Has Been Tried

### Attempt 1: Initial Implementation
- Created Vec3_AVX2, RayPacket, HitRecordPacket structures
- Implemented AVX2 sphere-sphere intersection
- Created hybrid approach: SIMD intersection + scalar shading
- **Result**: ❌ Crashes, wrong output, performance regression

### Attempt 2: Fix Material Assignment
- Moved material assignment to after sphere testing loop
- Used hit_sphere_indices[] to track which sphere each ray hit
- **Result**: ✅ Fix applied, ❌ But memory corruption remains

### Attempt 3: Debug Logging
- Added extensive debug output to track:
  - Material assignments per ray
  - HitRecord pointers
  - Albedo values at different stages
- **Result**: ❌ Confirmed memory corruption, but couldn't locate source

### Attempt 4: Check AVX2 Operations
- Verified _mm256_set_ps parameter ordering
- Checked Vec3 component layout
- Verified _mm256_storeu_ps alignment
- **Result**: ✅ AVX2 operations are correct

## Root Cause Analysis

### Most Likely Culprits

#### 1. **Shared Material Corruption**
**Hypothesis**: Multiple OpenMP threads are modifying the same material objects simultaneously.

**Evidence**:
- Materials are shared_ptr across scene
- OpenMP is enabled for parallelization
- Memory corruption suggests race condition

**Test**: Disable OpenMP, test if corruption persists

#### 2. **Stack Overwrite**
**Hypothesis**: HitRecord array is being overwritten due to incorrect sizing or alignment.

**Evidence**:
- hit_records.resize(8) should create 8 elements
- AVX2 operations require 32-byte alignment
- Stack corruption could change material pointers

**Test**: Add padding/alignment checks, verify array bounds

#### 3. **original_spheres Array Corruption**
**Hypothesis**: The original_spheres[] array is not properly maintained during SIMD conversion.

**Evidence**:
- Spheres are converted: Sphere → Sphere_SIMD
- original_spheres stores shared_ptr<Sphere>
- Array index might not match actual sphere index

**Test**: Log sphere indices during conversion and assignment

#### 4. **AVX2 Store Overwrite**
**Hypothesis**: _mm256_storeu_ps is writing beyond array bounds.

**Evidence**:
- AVX2 writes 256 bits (32 bytes) = 8 floats
- If array is smaller than 8 elements, overwrites memory
- float t_arr[8] should be safe, but...

**Test**: Add memory boundary checks, use valgrind

### Less Likely (But Possible)

- **Compiler optimization bug**: -O3 with AVX2 might have issues
- **Struct alignment**: Vec3_AVX2 needs 32-byte alignment
- **Pointer aliasing**: strict aliasing rules might be violated
- **Debug flag interaction**: static bool flags might cause issues

## Debugging Strategy

### Step 1: Isolate the Corruption
Add comprehensive logging to track a single ray:
```cpp
// At start of compute_phong_shading
debug_log << "ENTRY: rec=" << &rec << " rec.mat=" << rec.mat.get()
          << " rec.mat->albedo=" << rec.mat->albedo << std::endl;

// Before diffuse calculation
debug_log << "BEFORE_DIFFUSE: rec.mat=" << rec.mat.get()
          << " rec.mat->albedo=" << rec.mat->albedo << std::endl;
```

### Step 2: Check for Thread Safety
Disable OpenMP:
```makefile
make interactive-cpu ENABLE_OPENMP=0
```
Test if memory corruption persists.

### Step 3: Verify Material Pointers
Add validation to ensure materials aren't being modified:
```cpp
// After material assignment in scene.h
assert(hit_records[i].mat->albedo.x >= 0.0f && hit_records[i].mat->albedo.x <= 1.0f);
assert(hit_records[i].mat->albedo.y >= 0.0f && hit_records[i].mat->albedo.y <= 1.0f);
assert(hit_records[i].mat->albedo.z >= 0.0f && hit_records[i].mat->albedo.z <= 1.0f);
```

### Step 4: Memory Sanitizer
Compile with AddressSanitizer:
```bash
g++ -fsanitize=address -fno-omit-frame-pointer -g ...
```
Run with SIMD enabled, check for memory errors.

### Step 5: Valgrind
Run under valgrind to detect memory corruption:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./raytracer_interactive_cpu
```

## Performance Impact

**Current State**:
- Standard rendering: 3.x FPS
- SIMD rendering: 0.x FPS
- **Performance regression: 10x slower**

**Expected**: 4-6x faster than standard

**Why It's Slow**:
- Overhead of packet organization
- Scalar shading negates SIMD benefits
- Memory corruption causing re-renders
- Debug logging overhead

## Alternative Approaches

If memory corruption cannot be fixed:

### Option 1: Pure SIMD
Redesign for full SIMD pipeline:
- SIMD intersection → SIMD shading → SIMD framebuffer
- No scalar/simd mixing
- Requires complete rewrite of shading code

### Option 2: Abandon SIMD
Focus on other optimizations:
- BVH acceleration (cleaner separation)
- Adaptive sampling (working)
- Wavefront rendering (working)
- GPU implementation (60-300x faster, already working)

### Option 3: Limited SIMD
Use SIMD only for specific operations:
- Vector operations (already working in vec3_avx2.h)
- Color transformations
- Keep ray tracing scalar

## Quick Start for New Agent

### 1. Reproduce the Bug
```bash
cd /Users/caderichard/Ray\ Tracer/ray-tracer
make clean && make interactive-cpu
./raytracer_interactive_cpu
# Click "SIMD" button in settings panel
# Observe: gray output instead of proper lighting
# Check: simd_debug.log shows memory corruption
```

### 2. Key Files to Read
- src/scene/scene.h (lines 115-175) - Material assignment logic
- src/renderer/renderer.cpp (lines 357-500) - SIMD rendering loop
- src/math/vec3_avx2.h - AVX2 operations
- simd_debug.log - Debug output

### 3. First Debug Step
Add pointer tracking to compute_phong_shading:
```cpp
// In renderer.cpp, line ~100
debug_log << "PTR: rec=" << &rec << " mat=" << rec.mat.get()
          << " mat_ptr=" << &rec.mat->albedo << std::endl;
```

### 4. Verify Fix
After any fix, verify:
- [ ] Visual output matches standard rendering
- [ ] Performance is better or equal to standard
- [ ] No memory corruption in debug log
- [ ] Materials remain consistent through shading
- [ ] Toggle button works correctly

## Critical Constraints

1. **Do not break existing features**: Standard, wavefront, morton rendering must continue working
2. **Must be toggleable**: SIMD button must work correctly
3. **Performance must improve**: Cannot be slower than standard
4. **Visual output must match**: SIMD and standard must produce identical images

## Success Criteria

✅ **SIMD is working when**:
- Toggle button successfully enables/disables SIMD
- Visual output is identical to standard rendering
- Performance is 2-4x better than standard
- No memory corruption or crashes
- All materials are correct (no gray where should be red)
- Lighting calculations are correct (diffuse, specular, ambient)

## Contact

For questions about this feature, refer to:
- Phase 3 plan: docs/phase3-implementation-plan.md
- CPU agent context: claude-raytracer-cpu.md
- Main documentation: CLAUDE.md
