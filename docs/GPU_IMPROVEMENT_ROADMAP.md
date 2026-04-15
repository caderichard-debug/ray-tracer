# GPU Ray Tracer - Improvement Roadmap

**Status:** Phase 4 Complete ✅ | **Current:** Phase 5 Planning  
**Last Updated:** 2026-04-15  
**GLSL Constraint:** 1.20 (OpenGL 2.0+) for Intel i7 integrated GPU compatibility

---

## 📋 Table of Contents

- [Quick Win Recommendations](#quick-win-recommendations)
- [Phase 5: Post-Processing + Polish](#phase-5-post-processing--polish)
- [Phase 6: Advanced Lighting](#phase-6-advanced-lighting)
- [Phase 7: Performance Optimization](#phase-7-performance-optimization)
- [Phase 8: Advanced Features](#phase-8-advanced-features)
- [User Experience Improvements](#user-experience-improvements)
- [Implementation Status](#implementation-status)

---

## 🎯 Quick Win Recommendations

### **Immediate Impact (1-2 hours each)**

| # | Feature | Impact | Complexity | GLSL 1.20 |
|---|---------|--------|------------|-----------|
| 1 | Depth of Field | ⭐⭐⭐⭐ | Very Low | ✅ Yes |
| 2 | Chromatic Aberration | ⭐⭐⭐ | Very Low | ✅ Yes |
| 3 | Motion Blur | ⭐⭐⭐⭐ | Low | ✅ Yes |
| 4 | Adaptive Quality | ⭐⭐⭐⭐⭐ | Medium | ✅ Yes |
| 5 | Lens Flares | ⭐⭐⭐ | Low | ✅ Yes |

**Start Here:** These features give the biggest visual impact for minimal implementation time.

---

## 🚀 Phase 5: Post-Processing + Polish

**Goal:** Cinematic quality through advanced post-processing effects  
**Estimated Time:** 20-30 hours  
**Performance Impact:** -5 to -15 FPS  
**Visual Impact:** ⭐⭐⭐⭐⭐

### 5.1 Temporal Anti-Aliasing (TAA) ⭐ **HIGH PRIORITY**

**Description:** Eliminates jagged edges by accumulating samples over multiple frames

**Implementation:**
```glsl
// Need to add:
- Previous frame buffer (velocity + color)
- Temporal accumulation shader
- Camera velocity calculation
- Neighborhood clamping (ghosting prevention)
```

**Benefits:**
- Smooth edges without supersampling cost
- Reduces shimmering in motion
- Better image quality overall

**Performance:** ~5-10 FPS cost
**Complexity:** Medium (needs frame history management)
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 4-6 hours

**Files to Modify:**
- [src/main_gpu_interactive.cpp](src/main_gpu_interactive.cpp) - Add velocity buffer
- Shader code - Add TAA pass

---

### 5.2 Depth of Field (Bokeh)

**Description:** Cinematic camera blur based on focus distance

**Implementation:**
```glsl
// Post-process depth-based blur:
- Read depth buffer
- Calculate circle of confusion based on focus distance
- Apply variable-radius blur (bokeh)
- Optional: Hexagonal bokeh shape
```

**Benefits:**
- Professional camera look
- Directs viewer attention to focal point
- Separates foreground/background

**Performance:** ~3-5 FPS cost
**Complexity:** Low - straightforward post-processing
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 1-2 hours

**Controls:**
- **F** - Toggle DOF
- **[ / ]** - Adjust focus distance
- **- / =** - Adjust aperture (blur amount)

---

### 5.3 Motion Blur

**Description:** Smooth motion trails for moving objects/camera

**Implementation:**
```glsl
// Need to add:
- Velocity buffer (per-pixel motion vectors)
- Temporal motion blur shader
- Camera motion extraction
- Object motion (if animated)
```

**Benefits:**
- Eliminates strobing effect
- Smooths camera movement
- Adds cinematic feel

**Performance:** ~2-4 FPS cost
**Complexity:** Medium - need velocity tracking
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 3-4 hours

**Controls:**
- **M** - Toggle motion blur
- **, / .** - Adjust blur intensity

---

### 5.4 Chromatic Aberration

**Description:** Color fringing like real camera lenses

**Implementation:**
```glsl
// Simple RGB channel offset:
vec3 chromatic_aberration(vec2 uv) {
    float strength = 0.003;
    vec2 direction = normalize(uv - 0.5);
    
    float r = texture2D(framebuffer, uv + direction * strength).r;
    float g = texture2D(framebuffer, uv).g;
    float b = texture2D(framebuffer, uv - direction * strength).b;
    
    return vec3(r, g, b);
}
```

**Benefits:**
- Adds realism
- Cheap cinematic effect
- Subtle visual polish

**Performance:** <1 FPS cost (very cheap)
**Complexity:** Very Low - just RGB offset
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 0.5-1 hour

**Controls:**
- **C** - Toggle chromatic aberration
- **[ / ]** - Adjust strength

---

### 5.5 Lens Flares

**Description:** Realistic camera lens effects from bright lights

**Implementation:**
```cpp
// Need to add:
- Detect bright light sources in screen space
- Render flare sprites along light-to-center vector
- Multiple flare types: streak, ring, glow
- Optional: Anamorphic lens flares
```

**Benefits:**
- Dramatic light effects
- Adds production value
- Great for sunset scenes

**Performance:** ~2-3 FPS cost
**Complexity:** Low - sprite-based rendering
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 2-3 hours

**Controls:**
- **L** - Toggle lens flares
- **- / =** - Adjust intensity

---

### Phase 5 Summary

**Total Features:** 5
**Total Estimated Time:** 10-16 hours
**Performance Impact:** -12 to -22 FPS combined
**Visual Impact:** ⭐⭐⭐⭐⭐ (dramatic cinematic improvement)

**Recommended Order:** DOF → Chromatic Aberration → Motion Blur → TAA → Lens Flares

---

## 💡 Phase 6: Advanced Lighting

**Goal:** More realistic lighting effects and dynamic environments  
**Estimated Time:** 25-35 hours  
**Performance Impact:** -10 to -25 FPS  
**Visual Impact:** ⭐⭐⭐⭐⭐

### 6.1 Screen-Space Global Illumination (SSGI) ⭐ **HIGH PRIORITY**

**Description:** Better indirect lighting than current GI by sampling color in screen space

**Implementation:**
```glsl
// Extend current SSAO to sample color:
- For each pixel, sample nearby pixels in hemisphere
- Instead of just occlusion, sample color
- Accumulate indirect light contribution
- Blur to reduce noise
```

**Benefits:**
- More realistic color bleeding
- Better than current GI implementation
- Reuses SSAO infrastructure

**Performance:** Similar to current SSAO (~3-5 FPS cost)
**Complexity:** Medium - extend existing SSAO
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 4-6 hours

**Controls:**
- **I** - Toggle SSGI
- **[ / ]** - Sample count (4-32)
- **- / =** - Intensity (0.0-1.0)
- **, / .** - Radius (0.1-2.0)

---

### 6.2 Volumetric Lighting (God Rays) ⭐ **HIGH PRIORITY**

**Description:** Dramatic light shafts through atmospheric particles

**Implementation:**
```glsl
// Ray march through light volume:
- For each pixel, march toward light source
- Sample shadow map at each step
- Accumulate light transmission
- Apply dithering to reduce banding
```

**Benefits:**
- Very impressive visual effect
- Great for atmospheric scenes
- Adds depth and drama

**Performance:** ~10-15 FPS cost (expensive)
**Complexity:** Medium-High
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 6-8 hours

**Controls:**
- **V** - Toggle volumetric lighting
- **[ / ]** - Ray march steps (16-128)
- **- / =** - Density (0.1-2.0)

---

### 6.3 Procedural Sky System

**Description:** Dynamic skies with day/night cycle and clouds

**Implementation:**
```cpp
// Sky generation system:
- Rayleigh scattering (blue sky)
- Mie scattering (atmospheric haze)
- Procedural clouds using noise textures
- Sun position based on time of day
- Optional: Stars at night
```

**Benefits:**
- Dynamic environments
- No need for static skyboxes
- Day/night cycle capability

**Performance:** ~2-4 FPS cost
**Complexity:** Medium
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 5-7 hours

**Controls:**
- **K** - Toggle sky system
- **[ / ]** - Time of day
- **- / =** - Cloud density
- **, / .** - Cloud brightness

---

### 6.4 Cascaded Shadow Maps

**Description:** Softer, more realistic shadows for outdoor scenes

**Implementation:**
```cpp
// Multiple shadow cascades:
- Split view frustum into cascades
- Render shadow map for each cascade
- Blend between cascades
- Variable resolution per cascade
```

**Benefits:**
- Crisp nearby shadows
- Soft distant shadows
- Better shadow quality overall

**Performance:** ~5-8 FPS cost
**Complexity:** High - multiple shadow passes
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 8-10 hours

**Controls:**
- **;** - Toggle cascaded shadows
- **[ / ]** - Cascade count (2-4)
- **- / =** - Shadow softness

---

### 6.5 Color Grading Presets

**Description:** Cinematic color looks (Blockbuster, Horror, etc.)

**Implementation:**
```cpp
// LUT-based or procedural color grading:
- Predefined looks: Blockbuster, Horror, Sci-Fi, Vintage
- 3-way color wheels (lift, gamma, gain)
- Contrast curves
- Saturation controls
```

**Benefits:**
- Instant cinematic looks
- Great for screenshots
- Easy to customize

**Performance:** <1 FPS cost (very cheap)
**Complexity:** Low
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 2-3 hours

**Controls:**
- **U / J** - Cycle color grading presets
- **I / O / P** - Adjust lift/gamma/gain

---

### Phase 6 Summary

**Total Features:** 5
**Total Estimated Time:** 25-34 hours
**Performance Impact:** -20 to -35 FPS combined
**Visual Impact:** ⭐⭐⭐⭐⭐ (massive lighting quality improvement)

**Recommended Order:** Color Grading → SSGI → Procedural Sky → Volumetric Lighting → Cascaded Shadows

---

## ⚡ Phase 7: Performance Optimization

**Goal:** Maintain 60+ FPS with maximum quality  
**Estimated Time:** 15-25 hours  
**Performance Impact:** +15 to +30 FPS  
**Visual Impact:** ⭐⭐⭐ (maintained quality at higher FPS)

### 7.1 Adaptive Quality Scaling ⭐ **HIGH PRIORITY**

**Description:** Automatically adjust quality to maintain target FPS

**Implementation:**
```cpp
// Dynamic quality adjustment:
if (fps < target_fps) {
    // Reduce quality:
    - Lower sample count
    - Reduce resolution slightly
    - Disable expensive effects
    - Reduce effect quality
} else if (fps > target_fps + 10) {
    // Increase quality:
    - Higher sample count
    - Enable more effects
    - Increase effect quality
}
```

**Benefits:**
- Consistent frame rate
- Best possible quality at all times
- Automatic optimization

**Performance:** Net positive (maintains 60 FPS)
**Complexity:** Medium
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 3-4 hours

**Controls:**
- **A** - Toggle adaptive quality
- **[ / ]** - Target FPS (30/60/90/120)
- **- / =** - Aggressiveness

---

### 7.2 LOD (Level of Detail) System

**Description:** Render distant objects with fewer samples

**Implementation:**
```cpp
// Per-object sample counts based on distance:
for (object : scene) {
    distance = length(object.position - camera.position);
    if (distance < 10) samples = 16;
    else if (distance < 20) samples = 8;
    else if (distance < 40) samples = 4;
    else samples = 1;
}
```

**Benefits:**
- 10-20% performance improvement
- Quality where it matters
- Automatic optimization

**Performance:** +10-20% FPS
**Complexity:** Low-Medium
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 2-3 hours

**Controls:**
- **;** - Toggle LOD
- **[ / ]** - LOD distance multiplier

---

### 7.3 Frustum Culling Optimization

**Description:** Skip off-screen objects entirely

**Implementation:**
```cpp
// GPU-side bounding box test:
for (object : scene) {
    if (!in_frustum(object.bounding_box)) {
        continue; // Skip ray tracing
    }
    trace_rays(object);
}
```

**Benefits:**
- 5-15% performance improvement
- Simple to implement
- Big win for dense scenes

**Performance:** +5-15% FPS
**Complexity:** Low
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 2-3 hours

**Controls:**
- **H** - Toggle frustum culling (debug view)

---

### 7.4 Texture Compression

**Description:** Reduce memory bandwidth for textures

**Implementation:**
```cpp
// Compress textures on GPU:
- Convert RGB to compressed format
- Use texture compression (DXT/S3TC)
- Cache compressed textures
```

**Benefits:**
- 5-10% performance improvement
- Lower memory usage
- Better cache performance

**Performance:** +5-10% FPS
**Complexity:** Medium
**GLSL 1.20 Compatible:** ⚠️ Maybe (depends on GPU support)
**Estimated Time:** 3-4 hours

---

### 7.5 GPU Instancing

**Description:** Render thousands of identical objects efficiently

**Implementation:**
```cpp
// Instanced rendering for repeated geometry:
- Group identical objects
- Single draw call per group
- Per-instance transforms
- Massive reduction in draw calls
```

**Benefits:**
- Massive improvement for many objects
- Essential for particle systems
- Scales to thousands of objects

**Performance:** 10-100x improvement for instanced objects
**Complexity:** High
**GLSL 1.20 Compatible:** ✅ Yes
**Estimated Time:** 5-6 hours

---

### Phase 7 Summary

**Total Features:** 5
**Total Estimated Time:** 15-20 hours
**Performance Impact:** +35 to +75 FPS combined
**Visual Impact:** ⭐⭐⭐ (maintained quality at higher FPS)

**Recommended Order:** Adaptive Quality → LOD → Frustum Culling → Texture Compression → GPU Instancing

---

## 🔬 Phase 8: Advanced Features

**Goal:** Cutting-edge rendering techniques  
**Estimated Time:** 40-60 hours  
**Performance Impact:** Variable (expensive but optional)  
**Visual Impact:** ⭐⭐⭐⭐⭐ (state-of-the-art quality)

### 8.1 Compute Shader Migration ⭐ **EXPERIMENTAL**

**Description:** Rewrite ray tracer in OpenGL 4.3+ compute shaders

**Implementation:**
```cpp
// Compute shader ray tracer:
- Migrate ray tracing to compute shaders
- Shared memory optimization
- Better thread utilization
- Fallback to current GLSL 1.20 for old GPUs
```

**Benefits:**
- 2-3x performance improvement
- Modern GPU features
- Better memory access patterns

**Performance:** +100-200% on modern GPUs
**Complexity:** Very High
**GLSL 1.20 Compatible:** ❌ No (requires OpenGL 4.3+)
**Estimated Time:** 15-20 hours

**Compatibility:**
- **Modern GPUs:** Use compute shaders (OpenGL 4.3+)
- **Old GPUs:** Fall back to current implementation
- **Auto-detect:** Select best implementation at runtime

---

### 8.2 BVH Acceleration Structure

**Description:** Bounding Volume Hierarchy for fast ray intersections

**Implementation:**
```cpp
// GPU-side BVH:
- Build BVH tree on CPU
- Upload to GPU
- Traverse BVH in shader
- Cull entire branches at once
```

**Benefits:**
- 2-5x improvement for 100+ objects
- Essential for complex scenes
- Scales to thousands of objects

**Performance:** +100-400% for complex scenes
**Complexity:** Very High
**GLSL 1.20 Compatible:** ⚠️ Maybe (limited by recursion depth)
**Estimated Time:** 12-15 hours

---

### 8.3 Multi-Bounce Reflections

**Description:** Mirrors reflect mirrors reflect mirrors...

**Implementation:**
```glsl
// Recursive reflection rays:
int max_bounces = 4;
for (int i = 0; i < max_bounces; i++) {
    trace_ray();
    if (hit.material == reflective) {
        continue; // Trace reflection
    } else {
        break; // Stop at diffuse
    }
}
```

**Benefits:**
- Physically accurate reflections
- Infinite mirror corridors
- More realistic scenes

**Performance:** High cost (10-20 FPS per bounce)
**Complexity:** High
**GLSL 1.20 Compatible:** ✅ Yes (with limited recursion)
**Estimated Time:** 6-8 hours

**Controls:**
- **R** - Toggle multi-bounce
- **[ / ]** - Max bounces (1-8)

---

### 8.4 Path Tracing Integration

**Description:** Physically accurate global illumination via path tracing

**Implementation:**
```glsl
// Iterative path tracing:
for (int sample = 0; sample < max_samples; sample++) {
    accumulate_path(samples[sample]);
}
// Apply denoising
```

**Benefits:**
- Ground truth rendering
- Physically accurate lighting
- Reference quality images

**Performance:** Very high cost (1-5 FPS)
**Complexity:** Very High
**GLSL 1.20 Compatible:** ⚠️ Maybe (limited iterations)
**Estimated Time:** 15-20 hours

---

### 8.5 Real-time Denoising

**Description:** Clean up noisy renders instantly

**Implementation:**
```glsl
// SVGF or BMFR denoising:
- Temporal accumulation
- Gradient-domain reconstruction
- Edge-aware filtering
- Wavelet denoising
```

**Benefits:**
- Clean renders with fewer samples
- 2-4x quality improvement
- Essential for path tracing

**Performance:** Moderate cost (5-10 FPS)
**Complexity:** Very High
**GLSL 1.20 Compatible:** ⚠️ Maybe (simplified version)
**Estimated Time:** 12-15 hours

---

### Phase 8 Summary

**Total Features:** 5
**Total Estimated Time:** 60-78 hours
**Performance Impact:** Variable (compute shaders = +100-200%)
**Visual Impact:** ⭐⭐⭐⭐⭐ (state-of-the-art rendering)

**Recommended Order:** Multi-Bounce Reflections → Compute Shader Migration → BVH → Real-time Denoising → Path Tracing

---

## 🎮 User Experience Improvements

**Goal:** Better usability and workflow  
**Estimated Time:** 20-30 hours  
**Performance Impact:** Neutral  
**User Impact:** ⭐⭐⭐⭐⭐

### 9.1 Interactive Scene Editor

**Description:** Add/move/delete objects in real-time

**Features:**
- Click to select objects
- Drag to move objects
- Right-click to delete
- Add new objects (sphere, box, plane)
- Adjust material properties visually
- Save/load custom scenes

**Implementation:**
- Mouse picking (ray cast from mouse)
- Transform gizmos (translate/rotate/scale)
- Object property panel
- Scene save/load system (JSON)

**Estimated Time:** 10-12 hours

---

### 9.2 Screenshot Comparison Tool

**Description:** Side-by-side before/after comparisons

**Features:**
- Split screen comparison
- Slider for before/after
- Multiple screenshots in one image
- Automatic quality comparison reports
- Export comparison montages

**Implementation:**
- Screenshot history buffer
- Comparison rendering
- Image montage generation
- Quality metrics (PSNR, SSIM)

**Estimated Time:** 4-5 hours

---

### 9.3 Performance Profiling Overlay

**Description:** Real-time performance metrics

**Features:**
- FPS counter (with graph)
- MS per frame breakdown
- GPU memory usage
- Per-feature performance cost
- Bottleneck identification

**Implementation:**
- Performance timers
- GPU queries
- Overlay rendering
- Performance graph

**Estimated Time:** 2-3 hours

---

### 9.4 Preset Quality Modes

**Description:** Pre-configured quality settings

**Modes:**
- **Ultra Quality** - Max everything (15-20 FPS)
- **Quality** - Balanced (30-45 FPS)
- **Balanced** - 60 FPS target
- **Performance** - 120 FPS target
- **Screenshot Mode** - Maximum quality for stills

**Controls:**
- **F5-F9** - Quick quality presets
- Auto-save last preset

**Estimated Time:** 1-2 hours

---

### 9.5 Keyboard Shortcut Customization

**Description:** Remap all controls

**Features:**
- Remap all keyboard controls
- Save/load control schemes
- Preset schemes: WASD, Arrow Keys, DVORAK
- Mouse sensitivity adjustment
- Dead zone configuration

**Implementation:**
- Key binding system
- Configuration file (JSON)
- In-game remapping UI
- Preset management

**Estimated Time:** 3-4 hours

---

## 📊 Implementation Status

### Phase 4 (Current) ✅ **COMPLETE**
- [x] SSAO (Screen-Space Ambient Occlusion)
- [x] Bloom/Glow Effect
- [x] Cinematic Vignette
- [x] Film Grain
- [x] Advanced Tone Mapping (5 operators)
- [x] Color Grading (Exposure, Contrast, Saturation)
- [x] Scene Selection (3 showcase scenes)
- [x] Enhanced Textures (7 texture types)

### Phase 5: Post-Processing + Polish 🔄 **PLANNING**
- [ ] TAA (Temporal Anti-Aliasing)
- [ ] Depth of Field
- [ ] Motion Blur
- [ ] Chromatic Aberration
- [ ] Lens Flares

### Phase 6: Advanced Lighting 📋 **FUTURE**
- [ ] Screen-Space Global Illumination (SSGI)
- [ ] Volumetric Lighting (God Rays)
- [ ] Procedural Sky System
- [ ] Cascaded Shadow Maps
- [ ] Color Grading Presets

### Phase 7: Performance Optimization 📋 **FUTURE**
- [ ] Adaptive Quality Scaling
- [ ] LOD (Level of Detail) System
- [ ] Frustum Culling
- [ ] Texture Compression
- [ ] GPU Instancing

### Phase 8: Advanced Features 📋 **FUTURE**
- [ ] Compute Shader Migration
- [ ] BVH Acceleration Structure
- [ ] Multi-Bounce Reflections
- [ ] Path Tracing Integration
- [ ] Real-time Denoising

### User Experience 📋 **FUTURE**
- [ ] Interactive Scene Editor
- [ ] Screenshot Comparison Tool
- [ ] Performance Profiling Overlay
- [ ] Preset Quality Modes
- [ ] Keyboard Customization

---

## 🎯 Recommended Implementation Priority

### **Quick Wins (Start Here)**

1. **Chromatic Aberration** - 1 hour, instant visual improvement
2. **Depth of Field** - 2 hours, cinematic quality
3. **Adaptive Quality** - 3 hours, maintains 60 FPS
4. **Lens Flares** - 3 hours, dramatic light effects
5. **Motion Blur** - 4 hours, smooth motion

### **High Impact (Next Steps)**

1. **TAA** - 6 hours, eliminates jagged edges
2. **SSGI** - 6 hours, better color bleeding
3. **Volumetric Lighting** - 8 hours, god rays
4. **Procedural Sky** - 7 hours, dynamic environments
5. **Compute Shader Migration** - 20 hours, 2-3x performance

### **Long-term Projects**

1. **BVH Acceleration** - 15 hours, complex scene support
2. **Path Tracing** - 20 hours, physically accurate
3. **Scene Editor** - 12 hours, major UX improvement
4. **Real-time Denoising** - 15 hours, clean renders

---

## 📝 Notes

### GLSL 1.20 Constraints
- No compute shaders (need OpenGL 4.3+)
- Limited texture formats
- No integer texture operations
- Recursion depth limited
- Must use `texture2D()` instead of `texture()`

### Performance Targets
- **Intel i7 Integrated:** 15-30 FPS (current), 20-45 FPS (Phase 7)
- **Mid-range GPU:** 60-120 FPS (current), 90-180 FPS (Phase 7)
- **High-end GPU:** 120-300 FPS (current), 200-600 FPS (Phase 7)

### Memory Constraints
- Current: ~500MB limit for 1280x720
- Phase 7: ~300MB with optimizations
- Phase 8: Variable based on features

### Compatibility
- **Minimum:** OpenGL 2.0+ (GLSL 1.20)
- **Recommended:** OpenGL 3.0+ (GLSL 1.30)
- **Optimal:** OpenGL 4.3+ (compute shaders)

---

## 🚀 Quick Start Guide

### **Implement Your First Feature (Chromatic Aberration)**

1. **Open [src/main_gpu_interactive.cpp](src/main_gpu_interactive.cpp)**
2. **Find the tone mapping function**
3. **Add chromatic aberration shader code**
4. **Add keyboard toggle: `C`**
5. **Test and iterate**

**Estimated time:** 30-60 minutes

---

## 📚 References

- [GLSL 1.20 Specification](https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.1.20.pdf)
- [Real-Time Rendering](https://www.realtimerendering.com/)
- [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
- [Learn OpenGL - Advanced Lighting](https://learnopengl.com/Advanced-Lighting)

---

**Last Updated:** 2026-04-15  
**Status:** Phase 4 Complete ✅ | Phase 5 Planning 🔄  
**Next Feature:** Chromatic Aberration (Quick Win)

---

**Want to start implementing?** Choose any feature from the **Quick Wins** section and I'll help you build it! 🚀
