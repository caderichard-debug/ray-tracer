# Phase 4: Advanced Features

**Status:** ⏳ PENDING
**Build Target:** `make phase4`
**Dependencies:** [Phase 3](phase3-simd.md)

## Overview

Phase 4 adds advanced geometric primitives and rendering features for more complex scenes.

## Planned Features

### 1. Triangle Primitive
- Möller-Trumbore intersection algorithm
- Fast, robust ray-triangle intersection
- Support for mesh objects
- OBJ file loading capability

### 2. Plane Primitive
- Analytic ray-plane intersection
- Useful for ground planes, walls
- Infinite surface

### 3. Soft Shadows
- Multiple samples per light
- Area light sources
- Smooth penumbra regions
- Configurable quality vs. performance

### 4. Anti-Aliasing
- Supersampling (4x4, 8x8)
- Stratified sampling
- Jittered sampling
- Reduce jagged edges

### 5. Advanced Materials
- Dielectric (glass/water)
- Emissive (light sources)
- Translucent

## Implementation Status

- ⏳ Triangle primitive
- ⏳ Plane primitive
- ⏳ Soft shadows
- ⏳ Anti-aliasing
- ⏳ OBJ mesh loading

## Next Steps

[→ Phase 5: Multi-threading](phase5-multithreading.md)
