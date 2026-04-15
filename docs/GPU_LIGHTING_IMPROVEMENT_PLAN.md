# GPU Ray Tracer - Lighting Improvement Plan

## Executive Summary

This plan transforms the GPU raytracer from basic Phong shading to state-of-the-art physically based rendering while maintaining GLSL 1.20 (OpenGL 2.0+) compatibility. The implementation is divided into three phases, each delivering significant visual improvements.

**Current State:** Basic Phong shading (ambient + diffuse + specular), single point light, hard shadows
**Target State:** PBR lighting with soft shadows, ambient occlusion, global illumination approximation
**Expected Visual Impact:** 3-5x more realistic lighting, film-quality renders

---

## Phase 1: Physically Based Lighting (GLSL 1.20 Compatible)

### Goal: Replace Phong with Cook-Torrance BRDF

**Timeline:** 1-2 weeks
**Visual Impact:** ★★★★★ (Massive - objects look like real materials)
**Performance Impact:** +15-25% rendering time
**Compatibility:** 100% (GLSL 1.20)

### 1.1 Cook-Torrance BRDF Implementation

**What it solves:**
- Phong is ad-hoc (invented for graphics, not based on physics)
- Cook-Torrance is grounded in microfacet theory (real surface physics)
- Energy conservation (materials don't reflect more light than they receive)
- Accurate Fresnel effects (grazing angles become mirror-like)

**Implementation:**

```glsl
// Microfacet distribution function (GGX/Trowbridge-Reitz)
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return nom / denom;
}

// Geometry function (Smith)
float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);

    return ggx1 * ggx2;
}

// Fresnel (Schlick approximation)
vec3 F_Schlick(float cosTheta, vec3 F0) {
    float pow5 = pow(1.0 - cosTheta, 5.0);
    return F0 + (1.0 - F0) * pow5;
}

// Full Cook-Torrance BRDF
vec3 cook_torrance(vec3 N, vec3 V, vec3 L, vec3 H, 
                   vec3 albedo, float roughness, float metallic) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // Diffuse (Lambertian)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F_Schlick(HdotV, F0);

    vec3 kS = F;  // Specular reflection
    vec3 kD = vec3(1.0) - kS;  // Diffuse reflection
    kD *= 1.0 - metallic;  // Metals have no diffuse

    vec3 diffuse = kD * albedo / PI;

    // Specular (Cook-Torrance)
    float D = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3 specular = (D * G * F) / (4.0 * NdotL * NdotV + 0.0001);

    return diffuse + specular;
}
```

**Material System Changes:**
- Add `roughness` parameter (0.0 = mirror, 1.0 = matte)
- Add `metallic` parameter (0.0 = dielectric, 1.0 = metal)
- Convert existing materials to PBR workflow

**Roughness Values:**
- Mirror: 0.0
- Polished metal: 0.1-0.2
- Glossy plastic: 0.3-0.4
- Rough plastic: 0.5-0.6
- Chalk/drywall: 0.8-1.0

**Metallic Values:**
- Plastic/wood/dielectric: 0.0
- Gold/aluminum/copper: 1.0
- Rusty metal: 0.5

### 1.2 Image-Based Lighting (IBL) Approximation

**What it solves:**
- Current: Single point light creates harsh lighting
- IBL: Environment lighting from all directions
- Results in beautiful soft lighting and reflections

**Implementation (GLSL 1.20 compatible):**

```glsl
// Approximate IBL using environment gradient
vec3 ibl_lighting(vec3 N, vec3 V, vec3 albedo, float roughness, float metallic) {
    vec3 R = reflect(-V, N);
    
    // Sample environment gradient (sky approximation)
    float env_dot = max(R.y, 0.0);
    vec3 env_color = mix(vec3(0.1), vec3(0.5, 0.7, 1.0), env_dot);
    
    // Roughness-based blur approximation
    float roughness2 = roughness * roughness;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F_Schlick(max(dot(N, V), 0.0), F0);
    
    // Specular IBL
    vec3 specular_ibl = env_color * (roughness2 * F);
    
    // Diffuse IBL (irradiance approximation)
    float irradiance = max(N.y, 0.0) * 0.5 + 0.5;
    vec3 diffuse_ibl = albedo * irradiance * 0.3;
    
    return diffuse_ibl + specular_ibl;
}
```

**Visual Impact:** Removes harsh shadows, adds natural fill light

### 1.3 Multiple Light Sources

**What it solves:**
- Single light creates unnatural contrast
- Multiple lights create realistic studio lighting

**Implementation:**
```glsl
#define MAX_LIGHTS 4

uniform vec3 light_positions[MAX_LIGHTS];
uniform vec3 light_colors[MAX_LIGHTS];
uniform float light_intensities[MAX_LIGHTS];
uniform int num_lights;

vec3 calculate_lighting(vec3 N, vec3 V, vec3 albedo, float roughness, float metallic) {
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < num_lights; i++) {
        vec3 L = normalize(light_positions[i] - hit_point);
        vec3 H = normalize(V + L);
        
        // Shadow check
        float shadow = calculate_shadow(hit_point, light_positions[i]);
        
        // Cook-Torrance BRDF
        vec3 brdf = cook_torrance(N, V, L, H, albedo, roughness, metallic);
        
        // Light attenuation
        float distance = length(light_positions[i] - hit_point);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        
        Lo += brdf * light_colors[i] * light_intensities[i] * attenuation * shadow;
    }
    
    // Add IBL
    vec3 ibl = ibl_lighting(N, V, albedo, roughness, metallic);
    
    return Lo + ibl;
}
```

**Light Setup for Cornell Box:**
- Main light: (0, 18, 0) - overhead (existing)
- Fill light: (-10, 10, 10) - soft fill
- Rim light: (15, 5, -5) - edge highlight
- Ambient: Very low intensity everywhere

**Performance Impact:** +10% per additional light (3 lights total = +30%)

---

## Phase 2: Advanced Shadow and Lighting Techniques

### Goal: Soft shadows and ambient occlusion

**Timeline:** 2-3 weeks
**Visual Impact:** ★★★★☆ (Major - removes "CG look")
**Performance Impact:** +30-50% rendering time
**Compatibility:** 100% (GLSL 1.20)

### 2.1 Soft Shadows (Area Light Approximation)

**What it solves:**
- Hard shadows look unnatural (sharp edges)
- Real lights have size, creating penumbra (soft edges)

**Implementation (Stratified Sampling):**

```glsl
uniform int soft_shadow_samples;  // 1 = hard, 4+ = soft

float calculate_soft_shadow(vec3 hit_point, vec3 light_pos, vec3 light_normal, float light_radius) {
    float shadow = 0.0;
    
    // Create orthonormal basis around light normal
    vec3 light_tangent = normalize(cross(light_normal, vec3(0, 1, 0)));
    vec3 light_bitangent = cross(light_normal, light_tangent);
    
    for (int i = 0; i < soft_shadow_samples; i++) {
        for (int j = 0; j < soft_shadow_samples; j++) {
            // Stratified samples on light surface
            float u = (float(i) + hash(hit_point + float(i))) / float(soft_shadow_samples);
            float v = (float(j) + hash(hit_point + float(j))) / float(soft_shadow_samples);
            u = u * 2.0 - 1.0;
            v = v * 2.0 - 1.0;
            
            // Point on area light
            vec3 sample_point = light_pos + 
                light_tangent * u * light_radius +
                light_bitangent * v * light_radius;
            
            // Shadow ray
            shadow += trace_shadow_ray(hit_point, sample_point);
        }
    }
    
    return shadow / float(soft_shadow_samples * soft_shadow_samples);
}
```

**Visual Impact:** Shadows go from sharp-edged to beautifully soft

**Performance Impact:** 4x samples = 4x shadow computation cost (but only for shadows)

### 2.2 Screen-Space Ambient Occlusion (SSAO)

**What it solves:**
- Current: Flat ambient lighting everywhere
- SSAO: Darkens crevices and corners (realistic)

**Implementation (GLSL 1.20 compatible):**

```glsl
#define SSAO_SAMPLES 16
#define SSAO_RADIUS 0.5
#define SSAO_BIAS 0.05

float calculate_ssao(vec2 uv, vec3 normal, vec3 position) {
    float ao = 0.0;
    
    for (int i = 0; i < SSAO_SAMPLES; i++) {
        // Hemisphere sample direction
        vec3 sample_dir = hemisphere_sample(i, SSAO_SAMPLES, normal);
        vec3 sample_pos = position + sample_dir * SSAO_RADIUS;
        
        // Project sample to screen space
        vec2 sample_uv = world_to_screen(sample_pos);
        
        // Depth at sample location
        float sample_depth = read_depth(sample_uv);
        
        // Check if sample is occluded
        float range_check = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(position.z - sample_depth));
        ao += step(sample_depth, sample_pos.z + SSAO_BIAS) * range_check;
    }
    
    return 1.0 - (ao / SSAO_SAMPLES);
}
```

**Simplified Version (Ray Tracing):**

```glsl
// AO via random hemisphere rays
float calculate_ao_raytraced(vec3 hit_point, vec3 normal) {
    float ao = 0.0;
    int ao_samples = 8;  // Balance quality vs performance
    
    for (int i = 0; i < ao_samples; i++) {
        // Random direction in hemisphere around normal
        vec3 sample_dir = random_hemisphere_direction(normal, hit_point, i);
        vec3 sample_origin = hit_point + normal * 0.001;
        
        // Check for nearby occluders
        float t_max = 0.5;  // AO radius
        if (trace_any_ray(sample_origin, sample_dir, t_max)) {
            ao += 1.0;
        }
    }
    
    return 1.0 - (ao / ao_samples);
}
```

**Visual Impact:** Crevices become dark, exposed surfaces bright - huge realism boost

**Performance Impact:** +20-30% (can be tuned with sample count)

### 2.3 Tone Mapping and Gamma Correction

**What it solves:**
- Current: Linear output looks washed out
- Tone mapping: Film-like contrast and dynamic range

**Implementation:**

```glsl
// ACES Filmic Tone Mapping
vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard tone mapping
vec3 reinhard(vec3 x) {
    return x / (1.0 + x);
}

// Full color pipeline
vec3 color_pipeline(vec3 color) {
    // Exposure
    float exposure = 1.0;
    color *= exposure;
    
    // Tone map
    color = aces(color);
    
    // Gamma correction (always do last)
    color = pow(color, vec3(1.0 / 2.2));
    
    return color;
}
```

**Visual Impact:** Cinematic contrast, no blown-out highlights

### 2.4 Volumetric Light Scattering (God Rays)

**What it solves:**
- Current: No light participating media
- God rays: Light beams through dusty air

**Simplified Implementation:**

```glsl
uniform sampler2D depth_buffer;

vec3 volumetric_light(vec2 uv, vec3 light_pos) {
    vec3 light_screen = world_to_screen(light_pos);
    vec2 light_dir = light_screen.xy - uv;
    float light_dist = length(light_dir);
    light_dir /= light_dist;
    
    vec3 scattering = vec3(0.0);
    int samples = 32;
    
    for (int i = 0; i < samples; i++) {
        vec2 sample_uv = uv + light_dir * (float(i) / samples) * light_dist;
        float sample_depth = texture2D(depth_buffer, sample_uv).r;
        
        // Accumulate light if not occluded
        scattering += light_color * (1.0 / samples);
    }
    
    return scattering * 0.1;  // Scale intensity
}
```

**Note:** Requires depth buffer pass (deferred rendering approach)

---

## Phase 3: Global Illumination Approximation

### Goal: Approximate indirect lighting

**Timeline:** 3-4 weeks
**Visual Impact:** ★★★★★ (Massive - color bleeding, realistic interiors)
**Performance Impact:** +50-100% rendering time
**Compatibility:** 100% (GLSL 1.20)

### 3.1 Path Tracing (Monte Carlo Integration)

**What it solves:**
- Current: Only direct light (no light bounces)
- Path tracing: Physically accurate indirect light

**Implementation:**

```glsl
uniform int path_samples;  // Samples per pixel (1-64)
uniform int max_bounces;   // Ray depth (2-5)

vec3 path_trace(vec3 origin, vec3 direction) {
    vec3 color = vec3(0.0);
    vec3 throughput = vec3(1.0);
    
    vec3 current_origin = origin;
    vec3 current_dir = direction;
    
    for (int bounce = 0; bounce < max_bounces; bounce++) {
        // Find closest intersection
        HitInfo hit = scene_intersect(current_origin, current_dir);
        
        if (!hit.hit) {
            // Hit environment - sample sky
            color += throughput * sample_environment(current_dir);
            break;
        }
        
        // Direct lighting from light sources
        vec3 direct_light = sample_direct_light(hit);
        color += throughput * direct_light;
        
        // Indirect lighting (random bounce)
        vec3 bounce_dir = cosine_weighted_hemisphere(hit.normal, hit.position, bounce);
        float pdf = max(dot(bounce_dir, hit.normal), 0.0) / PI;
        
        throughput *= hit.albedo * max(dot(bounce_dir, hit.normal), 0.0) / pdf;
        
        // Russian roulette termination
        float max_color = max(max(throughput.r, throughput.g), throughput.b);
        if (hash(hit.position + bounce) > max_color) {
            break;  // Terminate low-contribution paths
        }
        throughput /= max_color;
        
        // Next bounce
        current_origin = hit.position + hit.normal * 0.001;
        current_dir = bounce_dir;
    }
    
    return color;
}

// Main rendering with Monte Carlo integration
vec3 render_path_traced(vec2 uv) {
    vec3 color = vec3(0.0);
    
    for (int i = 0; i < path_samples; i++) {
        vec2 jitter = vec2(
            hash(vec3(uv, float(i))),
            hash(vec3(uv + float(i), float(i)))
        );
        
        vec3 ray_dir = generate_camera_ray(uv + jitter / resolution);
        color += path_trace(camera_pos, ray_dir);
    }
    
    return color / path_samples;
}
```

**Visual Impact:** 
- Color bleeding (red wall makes floor reddish)
- Soft realistic shadows
- Light fills corners naturally

**Performance Impact:** 
- 1 sample: +20%
- 16 samples: +300% (but 16x better quality)

**Optimization:** Progressive rendering (show 1 sample first, refine over time)

### 3.2 Photon Mapping (Hybrid Approach)

**What it solves:**
- Path tracing is slow for caustics (focused light patterns)
- Photon mapping excels at caustics

**Implementation Overview:**
1. **Photon Pass:** Shoot photons from lights, store on surfaces
2. **Rendering Pass:** Use photon density for indirect light

**Simplified Version:**

```glsl
// Photon map as 3D texture (pre-pass)
uniform sampler3D photon_map;
uniform int photon_count;

vec3 estimate_radiance_photons(vec3 position, vec3 normal, float radius) {
    // Sample photons in radius
    vec3 radiance = vec3(0.0);
    int photons_found = 0;
    
    for (int i = 0; i < photon_count; i++) {
        vec3 photon_pos = texelFetch3D(photon_map, ivec3(i, 0, 0)).xyz;
        vec3 photon_power = texelFetch3D(photon_map, ivec3(i, 0, 1)).xyz;
        
        float dist = length(photon_pos - position);
        if (dist < radius && dot(photon_pos - position, normal) > 0) {
            radiance += photon_power;
            photons_found++;
        }
    }
    
    if (photons_found > 0) {
        float area = PI * radius * radius;
        return radiance / area;
    }
    
    return vec3(0.0);
}
```

**Note:** Complex to implement in GLSL 1.20, consider for OpenGL 4.3+ version

### 3.3 Light Probes (Irradiance Caching)

**What it solves:**
- Path tracing every bounce is expensive
- Pre-compute indirect light at sparse points

**Implementation:**
```glsl
uniform sampler3D irradiance_volume;  // Pre-computed GI

vec3 sample_irradiance_probe(vec3 position) {
    // Trilinear interpolation from 3D texture
    return texture3D(irradiance_volume, position * 0.1).rgb;
}

vec3 calculate_gi_fast(vec3 position, vec3 normal) {
    vec3 irradiance = sample_irradiance_probe(position);
    return irradiance * albedo / PI;
}
```

**Note:** Requires pre-computation pass (CPU-side)

### 3.4 Ambient Occlusion Pass (RTAO)

**What it solves:**
- More accurate than SSAO
- Ray traced AO is physically correct

**Implementation:**
```glsl
uniform int ao_samples;

float calculate_rtao(vec3 position, vec3 normal) {
    float occlusion = 0.0;
    
    for (int i = 0; i < ao_samples; i++) {
        vec3 sample_dir = cosine_weighted_hemisphere(normal, position, i);
        
        if (trace_any_ray(position + normal * 0.001, sample_dir, 1.0)) {
            occlusion += 1.0;
        }
    }
    
    return 1.0 - (occlusion / ao_samples);
}
```

**Visual Impact:** Crisper than SSAO, more accurate

---

## Optional: OpenGL 4.3+ Upgrade Path

### Goal: Modern GPU ray tracing features

**Timeline:** 4-6 weeks
**Visual Impact:** Same as above, but faster and cleaner
**Performance Impact:** -20 to +50% (depends on optimization)
**Compatibility:** Requires OpenGL 4.3+ (breaks compatibility)

### 4.1 Compute Shaders

**Benefits:**
- Better thread organization
- Shared memory for scene data
- No fragment shader limitations
- Direct memory access

**Implementation Structure:**
```glsl
#version 430

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba32f) uniform image2D output_image;
layout(std140, binding = 0) uniform SceneData {
    vec3 camera_pos;
    vec3 camera_dir;
    // ... more scene data
};

// Shared memory for BVH nodes
shared BVHNode shared_bvh[256];

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pixel) / image_size;
    
    // Ray trace
    vec3 color = path_trace(camera_pos, generate_ray(uv));
    
    imageStore(output_image, pixel, vec4(color, 1.0));
}
```

### 4.2 Shader Storage Buffer Objects (SSBOs)

**Benefits:**
- Large scene data (millions of triangles)
- Complex scene graphs
- BVH acceleration structures

**Implementation:**
```glsl
struct BVHNode {
    vec3 min_bound;
    int left_child;
    vec3 max_bound;
    int right_child;
    int first_primitive;
    int primitive_count;
};

layout(std430, binding = 1) buffer BVHBuffer {
    BVHNode nodes[];
};

layout(std430, binding = 2) buffer TriangleBuffer {
    Triangle triangles[];
};
```

### 4.3 Hardware Ray Tracing (NVIDIA RTX / Vulkan)

**Benefits:**
- 10-100x faster than software ray tracing
- Real-time path tracing at 1080p
- Hardware-accelerated BVH traversal

**Note:** Requires Vulkan or DX12 (not OpenGL)

---

## Implementation Priority

### Week 1-2: Foundation (Must Have)
1. ✅ Cook-Torrance BRDF (PBR lighting)
2. ✅ Multiple light sources (3-point lighting)
3. ✅ Tone mapping and gamma correction
4. ✅ Update material system with roughness/metallic

**Expected Visual Improvement:** 3x more realistic materials

### Week 3-4: Shadows and AO (High Impact)
1. ✅ Soft shadows (area light sampling)
2. ✅ Ray-traced ambient occlusion
3. ✅ Image-based lighting approximation

**Expected Visual Improvement:** 2x better shadows and ambient light

### Week 5-6: Global Illumination (Major Impact)
1. ✅ Path tracing with Monte Carlo integration
2. ✅ Progressive rendering (refine over time)
3. ✅ Russian roulette termination

**Expected Visual Improvement:** 3x more realistic lighting (color bleeding)

### Week 7-8: Polish and Optimization
1. ✅ Temporal anti-aliasing (TAA)
2. ✅ Performance profiling and optimization
3. ✅ UI controls for lighting parameters

**Expected Result:** Production-quality renderer

---

## Performance Budget

### Target Performance (1920x1080)

| Configuration | Current | Phase 1 | Phase 2 | Phase 3 |
|--------------|---------|---------|---------|---------|
| **1 sample** | 15 FPS | 12 FPS | 8 FPS | 6 FPS |
| **4 samples** | 4 FPS | 3 FPS | 2 FPS | 1.5 FPS |
| **16 samples** | 1 FPS | 0.8 FPS | 0.5 FPS | 0.3 FPS |

### Performance Optimization Strategies

1. **Progressive Rendering:**
   - Show 1 sample immediately
   - Refine to 4, then 16 samples over time
   - User can move camera during refinement

2. **Adaptive Sampling:**
   - More samples where needed (edges, shadows)
   - Fewer samples in flat areas

3. **Quality Presets:**
   - **Low:** 1 sample, hard shadows, no GI
   - **Medium:** 4 samples, soft shadows, RTAO
   - **High:** 16 samples, soft shadows, path tracing

4. **Resolution Scaling:**
   - Render at lower resolution, upscale
   - Maintain 60 FPS for interactive use

---

## Testing and Validation

### Visual Tests
1. **Material Test:** Render roughness/metallic sweep
2. **Lighting Test:** Cornell Box with reference images
3. **Shadow Test:** Soft shadow penumbra quality
4. **GI Test:** Color bleeding verification

### Performance Tests
1. **Baseline:** Current Phong shader
2. **PBR Only:** Cook-Torrance without extra features
3. **Full Stack:** All features enabled
4. **Regression:** Ensure no performance loss in existing code

### Comparison Reference
- [Disney BRDF Explorer](https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf)
- [Mitsuba Renderer](https://www.mitsuba-renderer.org/) (ground truth)
- [PBRT](https://www.pbrt.org/) (physically based reference)

---

## Documentation and Deliverables

### Code Changes
- Updated fragment shader with PBR lighting
- New uniform variables for lights and materials
- Modified scene setup for roughness/metallic
- Added progressive rendering support

### Documentation
- **GPU_LIGHTING_IMPLEMENTATION.md**: Technical details
- **MATERIAL_SYSTEM_GUIDE.md**: How to create PBR materials
- **LIGHTING_PRESETS.md**: Pre-configured lighting setups
- **PERFORMANCE_GUIDE.md**: Optimization strategies

### Demo Scenes
1. **Material Balls:** Roughness/metallic reference
2. **Cornell Box PBR:** Classic scene with modern lighting
3. **Spheres Studio:** Multi-light product shot
4. **GI Test Scene:** Color bleeding verification

---

## Success Metrics

### Visual Quality
- [ ] Materials look physically plausible (not plastic)
- [ ] Lighting matches reference images (within 10%)
- [ ] Shadows have soft penumbra (not sharp edges)
- [ ] Color bleeding visible in GI test scene
- [ ] No blown-out highlights (tone mapping working)

### Performance
- [ ] Interactive performance at 640x360 (30+ FPS)
- [ ] Progressive refinement works (1 sample → 16 samples)
- [ ] No performance regression in existing features
- [ ] Memory usage within budget (500MB limit)

### Compatibility
- [ ] Works on OpenGL 2.0+ (GLSL 1.20)
- [ ] Tested on NVIDIA, AMD, Intel GPUs
- [ ] Falls back gracefully on older hardware
- [ ] Cross-platform (macOS, Linux, Windows)

---

## Risk Assessment

### Technical Risks

**Risk:** PBR lighting too slow for interactive use
**Mitigation:** Quality presets, progressive rendering, resolution scaling

**Risk:** GLSL 1.20 limitations (no modern features)
**Mitigation:** Creative workarounds, document upgrade path to OpenGL 4.3+

**Risk:** Complexity explosion (too many features)
**Mitigation:** Phased implementation, each phase independently valuable

### Compatibility Risks

**Risk:** Breaks on older macOS OpenGL versions
**Mitigation:** Test on 10.14+, provide software fallback

**Risk:** Intel GPU compatibility issues
**Mitigation:** Reduce sample count on integrated GPUs

---

## Conclusion

This plan transforms the GPU raytracer from basic Phong shading to state-of-the-art physically based rendering while maintaining compatibility with GLSL 1.20. The phased approach allows us to:

1. **Phase 1:** Achieve 90% of visual quality with PBR lighting (2 weeks)
2. **Phase 2:** Add polish with soft shadows and AO (2-3 weeks)
3. **Phase 3:** Complete the picture with GI (3-4 weeks)

**Total Timeline:** 7-9 weeks for full implementation
**Visual Impact:** 5-10x more realistic rendering
**Performance Impact:** 2-4x slower (acceptable for quality gain)

The final result will be a GPU raytracer that produces film-quality renders rivaling production path tracers, while still running in real-time at lower resolutions.

---

**Next Steps:**
1. Review and approve this plan
2. Begin Phase 1 implementation (Cook-Torrance BRDF)
3. Create test scenes for validation
4. Set up performance benchmarks
5. Document progress with screenshots

**Questions to Resolve:**
- Should we prioritize interactive performance or final render quality?
- Do we want to maintain backward compatibility or target OpenGL 4.3+?
- What's the target hardware spec (min GPU requirement)?

---

**Document Version:** 1.0
**Last Updated:** 2024-01-15
**Status:** Ready for Implementation
