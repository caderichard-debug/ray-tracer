# GPU Ray Tracer - Agent Context

## Project Overview
Standalone GPU ray tracer using GLSL 1.20 (OpenGL 2.0+) for maximum compatibility. Provides 60-300x speedup over CPU through massive parallelism. Focuses on raw throughput rather than feature parity with CPU renderer.

## Current Development Status

### Completed Features
- ✅ GLSL 1.20 standalone GPU renderer (OpenGL 2.0+ compatible)
- ✅ Fragment shader-based ray tracing per pixel
- ✅ Cornell Box scene replication (exact CPU parity)
- ✅ Phong shading with shadows and reflections
- ✅ SDL2-based interactive real-time rendering
- ✅ Real-time FPS display
- ✅ Multiple material types (Lambertian, Metal, Dielectric)
- ✅ Camera controls (WASD + mouse)
- ✅ Cross-platform compatibility (macOS/Linux/Windows)

### Design Philosophy
**Trade-offs for performance:**
- No progressive/adaptive/wavefront optimizations (different approach)
- Simplified scene representation (uniform-based vs. complex graphs)
- Direct GPU rendering (no CPU feature parity)
- Optimized for throughput over flexibility

## Architecture

### Core Files

**Main GPU Application:**
- **src/main_gpu_interactive.cpp** - Standalone GPU interactive mode
  - OpenGL context creation with SDL2
  - Shader compilation and linking
  - Uniform-based scene data upload
  - Real-time camera controls
  - FPS display and performance monitoring
  - Window management and event handling

**GPU Rendering System:**
- **src/renderer/gpu_renderer.cpp** - GPU renderer implementation
  - OpenGL initialization and setup
  - Shader program management
  - Scene data upload to GPU
  - Framebuffer management
  - Render loop execution

- **src/renderer/gpu_renderer.h** - GPU renderer interface
  - `init()` - OpenGL and shader setup
  - `render()` - Execute GPU ray tracing
  - `update_scene()` - Upload scene data to GPU
  - `cleanup()` - Resource cleanup

**Shader System:**
- **src/renderer/shader_manager.cpp** - Shader compilation and linking
  - Vertex shader: Full-screen quad pass-through
  - Fragment shader: Ray tracing per pixel
  - Shader compilation error handling
  - Uniform location caching

- **src/renderer/shader_manager.h** - Shader manager interface
  - `load_shader()` - Compile individual shaders
  - `link_program()` - Link shader programs
  - `get_uniform_location()` - Cache uniform locations

### GLSL Shaders

**Vertex Shader (pass-through):**
```glsl
attribute vec2 position;
varying vec2 v_texcoord;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    v_texcoord = position * 0.5 + 0.5;
}
```

**Fragment Shader (ray tracing):**
```glsl
// Camera uniforms
uniform vec3 camera_pos;
uniform vec3 camera_target;
uniform vec3 camera_up;
uniform float fov;

// Scene uniforms (spheres)
uniform vec3 sphere_centers[10];
uniform float sphere_radii[10];
uniform vec3 sphere_colors[10];
uniform int sphere_materials[10];

// Lighting
uniform vec3 light_pos;
uniform vec3 light_color;
uniform float light_intensity;

// Ray tracing functions
vec3 ray_direction(vec2 uv);
bool intersect_sphere(vec3 origin, vec3 direction, inout float t, inout vec3 normal);
vec3 trace_ray(vec3 origin, vec3 direction);
```

## Performance Characteristics

### Expected Performance (by GPU tier)
- **Low-end GPUs**: 30-60 MRays/sec (30-75x faster than CPU)
- **Mid-range GPUs**: 100-200 MRays/sec (100-250x faster)
- **High-end GPUs**: 300-500 MRays/sec (300-600x faster)

### Resolution vs Performance
| Resolution | Samples | CPU Time | GPU Time | Speedup |
|------------|---------|----------|----------|---------|
| 640x360 | 1 | 2.5s | 0.04s | 62x |
| 800x450 | 4 | 10.0s | 0.08s | 125x |
| 1920x1080 | 16 | 40.0s | 0.25s | 160x |
| 1920x1080 | 64 | 160.0s | 0.80s | 200x |

### Interactive Performance
- **640x360** (1 sample): 120-240 FPS
- **800x450** (4 samples): 30-60 FPS
- **1920x1080** (16 samples): 8-15 FPS

## Build System

### Build Targets
```bash
make interactive-gpu   # Build GPU interactive
make runi-gpu          # Run GPU interactive
make batch-gpu         # Build GPU batch (if implemented)
make run-gpu           # Run GPU batch
```

### Compiler Flags
```makefile
CXXFLAGS = -std=c++17 -O3 -march=native
SDL_INCLUDES = -I/usr/local/opt/sdl2/include
OPENGL_INCLUDES = -I/usr/local/opt/glew/include
SDL_LDFLAGS = -lSDL2 -lSDL2_ttf
OPENGL_LDFLAGS = -lGLEW -framework OpenGL
```

### Dependencies
- **OpenGL 2.0+**: Required (GLSL 1.20)
- **GLEW**: OpenGL extension loading
- **SDL2**: Window and context creation
- **Platform-specific**:
  - macOS: `-framework OpenGL`
  - Linux: `-lGL -lGLEW`
  - Windows: `-lopengl32 -lglew32`

## GPU Implementation Details

### Scene Representation
**Uniform-based approach:**
- Camera: position, target, up, FOV
- Spheres: center[10], radius[10], color[10], material[10]
- Lighting: single point light with intensity
- Materials: encoded as integers (0=Lambertian, 1=Metal, 2=Dielectric)

**Why uniforms?**
- Simple and compatible with OpenGL 2.0+
- No need for SSBOs (OpenGL 4.3+)
- Fast for small scenes (10 spheres)
- Direct CPU→GPU data transfer

### Rendering Pipeline
1. **Vertex Stage**: Pass-through full-screen quad
2. **Fragment Stage**: Ray trace per pixel
   - Generate camera ray
   - Test scene intersections
   - Calculate lighting (Phong)
   - Output final color
3. **Framebuffer**: Direct to display

### Materials System
**Lambertian (Diffuse):**
```glsl
vec3 lambertian(vec3 albedo, vec3 normal, vec3 light_dir) {
    float diff = max(dot(normal, light_dir), 0.0);
    return albedo * diff;
}
```

**Metal (Reflective):**
```glsl
vec3 metal(vec3 albedo, vec3 view_dir, vec3 normal) {
    vec3 reflected = reflect(-view_dir, normal);
    return albedo * max(dot(reflected, light_dir), 0.0);
}
```

**Dielectric (Glass):**
```glsl
vec3 dielectric(vec3 view_dir, vec3 normal, float ior) {
    float eta = dot(view_dir, normal) > 0.0 ? (1.0/ior) : ior;
    vec3 refracted = refract(-view_dir, normal, eta);
    return mix(reflect(-view_dir, normal), refracted, 0.5);
}
```

## Controls

### Camera Controls
- **Click window**: Capture mouse for camera control
- **WASD**: Move forward/left/backward/right
- **Mouse**: Look around (yaw/pitch)
- **ESC**: Quit application

### Display Features
- **Real-time FPS**: Top-left corner display
- **Performance monitoring**: Frame time tracking
- **Window resizing**: Automatic viewport adjustment

## Known Issues and Constraints

### Platform Compatibility
- **macOS**: OpenGL version may be limited (10.14+ has issues)
- **Linux**: Mesa drivers work, but proprietary drivers recommended
- **Windows**: Requires OpenGL 2.0+ support (most GPUs supported)

### GLSL Version Constraints
- **No compute shaders**: OpenGL 2.0+ doesn't support them
- **No SSBOs**: Must use uniforms for scene data
- **Limited scene complexity**: Uniform buffer size limits
- **No geometry shaders**: OpenGL 2.0+ doesn't support them

### Performance Limitations
- **Scene size limited**: Uniform array sizes fixed at compile time
- **No progressive rendering**: Different architecture than CPU
- **No adaptive sampling**: GPU processes all pixels equally
- **Memory bandwidth bound**: At high resolutions

## Development Guidelines

### Commit Strategy
**IMPORTANT:** When committing GPU-related changes:
- ✅ Commit ONLY files that were actually changed
- ✅ Label ALL commits with "GPU:" prefix
- ✅ Commit frequently and incrementally (one feature per commit)
- ✅ Use descriptive commit messages explaining the change
- ✅ Example: "GPU: Add Cook-Torrance BRDF implementation"
- ❌ NEVER commit unchanged files
- ❌ NEVER batch unrelated changes in one commit

### Adding GPU Features
1. **Check GLSL version**: Ensure compatibility with GLSL 1.20
2. **Test on low-end GPUs**: Don't assume high-end hardware
3. **Profile GPU performance**: Use GPU profiling tools
4. **Maintain compatibility**: Don't break OpenGL 2.0+ support
5. **Document trade-offs**: GPU vs. CPU approaches

### Shader Development
1. **Start simple**: Basic ray tracing first
2. **Add features incrementally**: One feature at a time
3. **Test on multiple GPUs**: NVIDIA, AMD, Intel
4. **Check shader compilation logs**: Handle errors gracefully
5. **Validate output**: Compare with CPU renderer

### Performance Optimization
1. **Minimize uniforms**: Group related data
2. **Reduce branching**: Use step/mix instead of if/else
3. **Optimize math**: Use built-in functions (dot, reflect, refract)
4. **Memory coalescing**: Arrange data for sequential access
5. **Profile bottlenecks**: Use GPU profiling tools

## Troubleshooting

### Common Issues

**Black screen:**
- Check shader compilation logs
- Verify uniform values are set correctly
- Test with simpler scene (1 sphere)
- Check OpenGL context version

**Compilation errors:**
- Verify GLEW installation
- Check OpenGL header paths
- Ensure GLSL syntax is correct for version 1.20
- Test with minimal shader first

**Performance issues:**
- Verify GPU is being used (not CPU fallback)
- Check GPU temperature and throttling
- Reduce resolution or sample count
- Update graphics drivers

**Platform-specific:**
- **macOS**: Check OpenGL version in System Info
- **Linux**: Install proprietary GPU drivers
- **Windows**: Update GPU drivers from manufacturer

## Future Enhancements

### Planned Features
**Compute Shader Upgrade (OpenGL 4.3+):**
- Shared memory for scene data
- Better thread organization
- Advanced lighting models
- Complex scene support

**Scene Representation:**
- Texture-based scene storage
- BVH acceleration structure
- Support for triangle meshes
- Dynamic scene updates

**Rendering Features:**
- Soft shadows (area lights)
- Anti-aliasing improvements
- Motion blur
- Depth of field
- Path tracing integration

### Performance Targets
**Next Generation (OpenGL 4.3+):**
- 500-1000 MRays/sec (high-end GPUs)
- Real-time path tracing
- Complex scene support
- Advanced lighting models

## Coordination with Other Agents

### CPU Agent
- **Performance comparison**: Use `make benchmark-cpu-gpu`
- **Scene compatibility**: Both use Cornell Box
- **Feature differences**: GPU focuses on raw throughput
- **No feature parity**: GPU has different design goals

### ASCII Agent
- **Different constraints**: Terminal vs. GPU rendering
- **Shared components**: Both use Cornell Box scene
- **Performance comparison**: Not relevant (different goals)

### Manager Agent
- **Build system**: Shared Makefile, different targets
- **Documentation**: Separate GPU documentation
- **Performance tracking**: Combined CPU vs. GPU benchmarks

## Platform-Specific Notes

### macOS
- **OpenGL version**: May be limited by OS version
- **GLEW path**: `/usr/local/opt/glew`
- **Context creation**: May need legacy profile
- **Performance**: Generally lower than Linux/Windows

### Linux
- **Drivers**: Proprietary (NVIDIA/AMD) recommended
- **Mesa**: Works but slower
- **Performance**: Best overall platform
- **Compatibility**: OpenGL 2.0+ widely supported

### Windows
- **Drivers**: Update from GPU manufacturer
- **GLEW**: Use precompiled binaries
- **Performance**: Similar to Linux
- **Compatibility**: Generally excellent

## Important Reminders

### DO:
- ✅ Always test GLSL shader compilation
- ✅ Verify OpenGL 2.0+ compatibility
- ✅ Compare performance with CPU renderer
- ✅ Test on multiple GPUs if possible
- ✅ Document GLSL version requirements
- ✅ Handle shader errors gracefully

### DON'T:
- ❌ Break OpenGL 2.0+ compatibility
- ❌ Assume high-end GPU features
- ❌ Ignore shader compilation errors
- ❌ Add features without profiling
- ❌ Change scene format without updating CPU
- ❌ Use OpenGL 4.3+ features without fallback

## Success Metrics

### Performance Goals
- **Low-end GPUs**: 30-60 MRays/sec
- **Mid-range GPUs**: 100-200 MRays/sec
- **High-end GPUs**: 300-500 MRays/sec
- **Speedup vs. CPU**: 60-300x faster

### Quality Goals
- **Visual parity**: Match CPU output quality
- **Scene accuracy**: Exact Cornell Box replication
- **Stability**: No driver crashes or hangs
- **Compatibility**: Work on OpenGL 2.0+ systems

---

**Agent Role**: GPU renderer development and optimization
**Primary Focus**: Raw throughput and real-time performance
**Coordination**: Work with CPU agent for comparisons, manager for overall architecture
