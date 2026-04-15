# GPU Ray Tracer Implementation Plan - OpenGL Compute Shaders

## Overview
This document provides a comprehensive implementation plan for adding GPU-based ray tracing to the existing CPU ray tracer using OpenGL 4.3+ compute shaders. The GPU implementation will run alongside the existing CPU renderer, providing users with a choice between CPU compatibility and GPU performance.

## Current Status
- ✅ **CPU Ray Tracer**: Fully functional with Phong shading, shadows, reflections, multi-material support, and interactive mode
- ✅ **Code Cleanup**: All previous GPU code removed while preserving 100% CPU functionality  
- ✅ **Build System**: Clean Makefile with CPU-only targets
- 🚀 **Goal**: Add GPU compute shader implementation for 60-300x performance improvement

## Architecture Strategy

### Hybrid CPU/GPU Approach
- **Maintain CPU renderer** as fallback and reference implementation
- **Add GPU renderer** alongside (not replacement) for maximum performance
- **Runtime switching** between CPU/GPU renderers via command-line flag or hotkey
- **Shared scene graph** using same data structures
- **User choice** in renderer selection based on hardware and needs

### Why Compute Shaders?
- **Direct control** over work groups and thread synchronization  
- **Better performance** for ray tracing than fragment shaders
- **Natural fit** for parallel ray-primitive intersection tests
- **Future-proof** path to advanced features (BVH, path tracing)
- **Wide compatibility** with OpenGL 4.3+ (most modern GPUs)

## Performance Targets

### Expected Speedup by GPU Tier
- **Low-end GPUs**: 30-60 MRays/sec (30-75x faster than CPU)
- **Mid-range GPUs**: 100-200 MRays/sec (100-250x faster)  
- **High-end GPUs**: 300-500 MRays/sec (300-600x faster)

### Interactive Mode Performance
- **Quality 2** (640x360, 1 sample): 120-240 FPS
- **Quality 3** (800x450, 4 samples): 30-60 FPS
- **Quality 4** (800x450, 16 samples): 8-15 FPS

### Batch Mode Performance  
- **1920x1080, 64 samples, depth 8**: ~2-5 seconds (vs 5-10 minutes on CPU)

## Implementation Phases

### Phase 1: Infrastructure Setup (Week 1)

#### Goals
Create OpenGL context, shader system, and buffer management foundation.

#### Tasks

**1. OpenGL Context with SDL2**
- Modify SDL2 window creation to include OpenGL context
- Request OpenGL 4.3+ core profile  
- Add GLEW or use SDL2's OpenGL loading functions
- Test with simple triangle rendering to verify setup

**2. Shader Compilation System**
- Create `ShaderManager` class for compilation/linking
- Implement `load_shader()`, `link_program()` methods
- Add comprehensive error checking and logging
- Test with simple compute shader that clears a texture

**3. GPU Buffer Management**
- Create SSBO (Shader Storage Buffer Object) structures for scene data
- Implement buffer allocation, upload, and update methods
- Add scene change detection for buffer updates
- Test with simple single-sphere scene rendering

#### Verification
- OpenGL context created successfully with correct version
- Simple compute shader runs and outputs to texture
- Triangle renders in window confirming GL setup
- Scene data uploads to GPU without errors

### Phase 2: Core Ray Tracing (Week 2)

#### Goals
Implement compute shader ray tracing with Phong shading matching CPU quality.

#### Tasks

**1. Shader Structure Setup**
```glsl
// Define SSBO layouts matching CPU structures
struct Camera { vec4 position, lookat, vup; float vfov, aspect; };
struct Sphere { vec4 center; float radius; int material_id; };
struct Material { vec4 albedo; float fuzz; int type; };
struct Light { vec4 position, intensity; };

// Camera ray generation matching CPU camera model
// Background gradient fallback
```

**2. Primitive Intersections**
```glsl
// Port ray-sphere intersection to GLSL
// Port ray-triangle intersection (Möller-Trumbore) to GLSL  
// Implement nearest-hit search loop
// Handle all material types (Lambertian, Metal, Dielectric)
```

**3. Phong Shading**
```glsl
// Ambient component
// Diffuse component (Lambertian)
// Specular component (Blinn-Phong)
// Hard shadows via shadow rays
// Multiple light support
```

**4. Reflections**
```glsl  
// Recursive ray tracing in shader
// Handle Lambertian vs Metal materials
// Add max depth termination
// Fresnel effects for dielectric materials
```

#### Verification
- Cornell box scene renders correctly
- Output matches CPU renderer (bit-identical or within floating-point error)
- Performance is 50-100x faster than CPU baseline
- All material types render correctly

### Phase 3: Advanced Features (Week 3)

#### Goals
Add anti-aliasing, gamma correction, and performance tracking.

#### Tasks

**1. Anti-aliasing**
```glsl
// Multi-sample per pixel in shader
// Random jitter for supersampling
// Integration with existing quality level system
// Progressive refinement for interactive preview
```

**2. Gamma Correction & Tone Mapping**
```glsl
// Apply gamma 2.0 in final output
// Match CPU output exactly
// Add optional HDR tone mapping
```

**3. Performance Tracking**
```glsl
// GPU timer queries for accurate measurement
// Calculate rays/sec through shader
// Integrate with existing PerformanceTracker
// Real-time FPS display
```

**4. Optimizations**
```glsl
// Work group size tuning (16x16 vs 8x8 vs 32x32)
// Shared memory for coherent rays
// Early ray termination for shadows
// Memory coalescing improvements
```

#### Verification
- Anti-aliasing works and matches CPU quality levels
- Performance metrics display accurately in UI
- Interactive mode runs at 60+ FPS at quality level 3+
- Output quality is identical to CPU renderer

### Phase 4: Integration (Week 4)

#### Goals
Integrate GPU renderer with existing batch and interactive modes.

#### Tasks

**1. Interactive Mode Integration**
```cpp
// Add GPU renderer option to main_interactive.cpp
// Runtime renderer switching (CPU/GPU hotkey)
// Maintain all quality level switching
// Add GPU status indicator in UI
// Preserve all existing controls (WASD, mouse, etc.)
```

**2. Batch Mode Integration**  
```cpp
// Add --renderer flag to main.cpp
// Support both CPU and GPU rendering
// Maintain PNG output compatibility
// Add performance comparison output
```

**3. Makefile Updates**
```makefile
# Add OpenGL/linker flags
OPENGL_LDFLAGS = -lGLEW -framework OpenGL
OPENGL_INCLUDES = -I/usr/local/opt/glew/include

# Create GPU targets
make gpu           # Build GPU ray tracer
make run-gpu       # Run GPU batch mode
make runi-gpu      # Run GPU interactive mode
```

**4. Testing & Optimization**
- Benchmark all quality levels vs CPU
- Profile bottlenecks and optimize
- Test on different GPUs (NVIDIA, AMD, Intel)
- Add fallback for unsupported GPUs

#### Verification
- Both batch and interactive modes work seamlessly with GPU
- Output quality matches CPU renderer exactly
- Performance achieves target speedup (60-300x)
- All existing functionality preserved
- Cross-platform compatibility verified

## Technical Implementation Details

### GPU Buffer Layout (std430)

```glsl
// Camera SSBO
layout(std430, binding = 0) buffer CameraBuffer {
    Camera cameras[];
};

// Primitive SSBOs
layout(std430, binding = 1) buffer SphereBuffer {
    Sphere spheres[];
};

layout(std430, binding = 2) buffer TriangleBuffer {  
    Triangle triangles[];
};

// Material SSBO
layout(std430, binding = 3) buffer MaterialBuffer {
    Material materials[];
};

// Light SSBO
layout(std430, binding = 4) buffer LightBuffer {
    Light lights[];
};

// Output image
layout(binding = 5, rgba32f) uniform image2D output_image;

// Scene parameters
uniform int num_spheres;
uniform int num_triangles; 
uniform int num_lights;
uniform int max_depth;
uniform int samples_per_pixel;
uniform vec2 resolution;
uniform uint frame_count;  // For progressive refinement
```

### Compute Shader Work Distribution

```glsl
layout(local_size_x = 16, local_size_y = 16) in;

void main() {
    vec2 pixel = vec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= resolution.x || pixel.y >= resolution.y) return;

    // Initialize random number generator
    uint seed = pixel.y * uint(resolution.x) + pixel.x + frame_count;
    
    vec3 color = vec3(0.0);
    
    // Multi-sample anti-aliasing loop
    for (int s = 0; s < samples_per_pixel; s++) {
        // Generate ray from camera
        Ray ray = generate_camera_ray(pixel, seed);
        
        // Trace ray through scene
        color += trace_ray(ray, max_depth, seed);
    }
    
    // Average samples
    color /= float(samples_per_pixel);
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.0));
    
    // Store final color
    imageStore(output_image, ivec2(pixel), vec4(color, 1.0));
}
```

### CPU Integration Points

**Interactive Mode (src/main_interactive.cpp)**
```cpp
enum RendererType { CPU, GPU };
RendererType current_renderer = GPU;

// In main loop:
if (current_renderer == CPU) {
    // Existing CPU rendering code
    cpu_renderer.render(scene, camera, framebuffer);
} else {
    // New GPU rendering code
    gpu_renderer.render(scene, camera, framebuffer);
}

// Add key binding for renderer switching
case SDLK_r:  // Toggle CPU/GPU
    current_renderer = (current_renderer == CPU) ? GPU : CPU;
    break;
```

**Batch Mode (src/main.cpp)**
```cpp
bool use_gpu = false;
for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--gpu") {
        use_gpu = true;
    }
}

if (use_gpu) {
    GPURenderer gpu_renderer;
    gpu_renderer.set_scene(scene);
    gpu_renderer.render(cam, framebuffer);
} else {
    // Existing CPU rendering
    cpu_renderer.render(cam, framebuffer);
}
```

## File Structure After Implementation

```
ray-tracer/
├── src/
│   ├── main.cpp                    # Batch mode (CPU + GPU)
│   ├── main_interactive.cpp        # Interactive mode (CPU + GPU)
│   ├── renderer/
│   │   ├── renderer.h/cpp          # CPU renderer (existing)
│   │   ├── gpu_renderer.h/cpp      # GPU renderer (new)
│   │   ├── shader_manager.h/cpp    # Shader system (new)
│   │   └── performance.h           # Performance tracking (enhanced)
│   ├── shaders/
│   │   ├── raytrace.comp           # Compute shader (new)
│   │   ├── quad.vert               # Vertex shader (new)
│   │   └── quad.frag               # Fragment shader (new)
│   └── [existing files unchanged...]
├── docs/
│   └── GPU_IMPLEMENTATION_PLAN.md  # This document
└── Makefile                        # Updated with GPU targets
```

## Risk Mitigation

### Potential Risks & Solutions

**1. OpenGL Driver Compatibility Issues**
- Risk: Different GPU drivers may have bugs or varying feature support
- Solution: Extensive testing on NVIDIA, AMD, Intel GPUs; provide CPU fallback

**2. Shader Compilation Differences**  
- Risk: GLSL compilers may behave differently across vendors
- Solution: Extensive validation testing; conservative GLSL usage; validation layers

**3. Precision Differences CPU vs GPU**
- Risk: Floating-point precision may cause output differences
- Solution: Allow tolerance in comparison tests; validate visually

**4. Memory Limitations on Lower-end GPUs**
- Risk: Large scenes may exceed GPU memory
- Solution: Implement scene LOD or intelligent fallback to CPU for large scenes

**5. Integration Complexity**
- Risk: Adding GPU may break existing CPU functionality
- Solution: Maintain separate code paths; extensive testing; never modify CPU path

## Testing Strategy

### Unit Testing
1. **Intersection Tests**: Verify sphere/triangle intersections match CPU exactly
2. **Material Tests**: Validate Lambertian, Metal, Dielectric scattering  
3. **Lighting Tests**: Phong shading components match CPU output
4. **Math Tests**: Random number generation, vector operations

### Integration Testing  
1. **Scene Rendering**: Render test scenes at all quality levels
2. **Output Comparison**: Automated image difference tests (tolerance: 0.1%)
3. **Performance Benchmarks**: Measure speedup across quality levels
4. **Interactive Stress Test**: Extended runtime testing, camera movement

### Validation
- **Visual Comparison**: Side-by-side GPU vs CPU rendering
- **Automated Tests**: Image difference within tolerance
- **Performance Regression**: Ensure no performance degradation
- **Cross-Platform**: Test on NVIDIA, AMD, Intel GPUs

## Dependencies & Requirements

### New Dependencies
- **GLEW** (OpenGL Extension Wrangler): Loading OpenGL functions
- **OpenGL 4.3+**: For compute shader support
- **Existing SDL2**: Already used for window management

### Installation

**macOS:**
```bash
brew install glew
```

**Ubuntu/Debian:**
```bash  
sudo apt install libglew-dev
```

**Windows:**
```bash
# Using vcpkg
vcpkg install glew
```

## Success Criteria

1. **Functionality Preserved**: All existing features work identically on CPU
2. **Performance Achieved**: 60-300x speedup over CPU baseline
3. **Interactive Mode**: 60+ FPS at quality level 2-3
4. **Output Quality**: Bit-identical or within floating-point tolerance
5. **User Experience**: Seamless switching between CPU/GPU renderers
6. **Code Quality**: Clean integration, maintainable codebase
7. **Cross-Platform**: Works on NVIDIA, AMD, Intel GPUs

## Future Enhancements

Beyond the initial implementation, consider adding:

- **BVH Acceleration**: For complex scenes with many objects
- **Path Tracing**: For photorealistic lighting and soft shadows
- **Denoising**: AI-based or traditional denoising for fewer samples
- **Multi-GPU**: SLI/CrossFire support for extreme performance
- **Hybrid Rendering**: CPU for primary rays, GPU for secondary rays
- **Progressive Refinement**: Iterative quality improvement in interactive mode

## References

- [OpenGL 4.3 Compute Shaders](https://www.khronos.org/opengl/wiki/Compute_Shader)
- [Ray Tracing in Vulkan](https://github.com/GPSnoopy/RayTracingInVulkan)
- [SmallVCM](https://github.com/SmallVCM/SmallVCM)
- [PBRT](https://www.pbrt.org/)

---

**Status**: Ready for implementation
**Estimated Timeline**: 4 weeks
**Priority**: High (major performance improvement)