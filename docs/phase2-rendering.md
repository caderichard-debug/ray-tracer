# Phase 2: Basic Rendering

**Status:** ✅ COMPLETE
**Build Target:** `make phase2`
**Dependencies:** [Phase 1](phase1-foundation.md)
**New Files:** [src/scene/](../src/scene/), [src/renderer/](../src/renderer/)

## Overview

Phase 2 transforms the mathematical foundation into a working ray tracer with proper lighting, shadows, and reflections. This phase implements the complete rendering pipeline.

## Key Components

### 1. Scene Graph ([scene.h](../src/scene/scene.h))

Manages all objects and lights in the scene.

```cpp
class Scene {
    std::vector<std::shared_ptr<Primitive>> objects;
    std::vector<Light> lights;
    Color ambient_light;

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const;
    bool is_shadowed(const Ray& shadow_ray, float t_max) const;
};
```

**Features:**
- Container for multiple primitives and lights
- Finds closest intersection among all objects
- Efficient shadow ray testing
- Ambient lighting control

**Scene Traversal:**
```cpp
// Test all objects, return closest hit
for (const auto& object : objects) {
    if (object->hit(r, t_min, closest_so_far, temp_rec)) {
        hit_anything = true;
        closest_so_far = temp_rec.t;
        rec = temp_rec;
    }
}
```

### 2. Point Lights ([light.h](../src/scene/light.h))

Simple point light sources for illumination.

```cpp
class Light {
    Point3 position;
    Color intensity;  // RGB intensity
};
```

**Usage:**
- Located at specific point in space
- Emits light equally in all directions
- Multiple lights supported
- Intensity can be colored

### 3. Phong Shading Model

Implementation of classic Phong shading with ambient, diffuse, and specular components.

```cpp
Color compute_phong_shading(const HitRecord& rec, const Scene& scene) {
    // 1. Ambient component
    Color color = ambient_light * material.albedo;

    for (each light) {
        // 2. Shadow ray
        if (!is_shadowed(shadow_ray)) {
            // 3. Diffuse component (Lambertian)
            float diffuse_intensity = max(0, normal·light_dir);
            Color diffuse = diffuse_intensity * light.intensity * albedo;

            // 4. Specular component (Phong)
            Vec3 reflect_dir = reflect(-light_dir, normal);
            float spec = pow(max(0, view·reflect_dir), shininess);
            Color specular = spec * light.intensity;

            color += diffuse + specular;
        }
    }
    return color;
}
```

**Phong Components:**
1. **Ambient:** Constant illumination, prevents total blackness
2. **Diffuse:** Lambertian scattering, depends on light angle
3. **Specular:** Shiny highlights, depends on view direction

### 4. Shadow Rays

Shadow rays determine if a light is visible from the hit point.

```cpp
bool is_shadowed(const Ray& shadow_ray, float light_distance) const {
    // Cast ray toward light
    // If any object blocks it, return true
    for (const auto& object : objects) {
        if (object->hit(shadow_ray, 0.001f, light_distance, shadow_rec)) {
            return true;  // Light is blocked
        }
    }
    return false;  // Light is visible
}
```

**Implementation Details:**
- `t_min = 0.001f` prevents self-shadowing (epsilon offset)
- `t_max = light_distance` stops at light source
- Hard shadows (binary: blocked or not)

### 5. Recursive Reflections

Metallic materials recursively trace reflected rays.

```cpp
Color ray_color(const Ray& r, const Scene& scene, int depth) {
    if (!scene.hit(r, t_min, t_max, rec)) {
        return background_color;  // Sky gradient
    }

    if (depth <= 0) {
        return Color(0, 0, 0);  // Terminated
    }

    // Calculate shading
    Color color = compute_phong_shading(rec, scene);

    // Handle reflections
    if (material->scatter(r, rec, attenuation, scattered)) {
        Color reflected = ray_color(scattered, scene, depth - 1);
        color += attenuation * reflected;
    }

    return color;
}
```

**Reflection Physics:**
```cpp
Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * dot(v, n) * n;
}
```

### 6. Gamma Correction

Linear to gamma-encoded color space conversion.

```cpp
// Gamma 2 correction
pixel_color.r = sqrt(pixel_color.r);
pixel_color.g = sqrt(pixel_color.g);
pixel_color.b = sqrt(pixel_color.b);
```

**Why Gamma Correction?**
- Rendering calculations are linear
- Monitors expect gamma-encoded values (~2.2)
- Without it, images look too dark

## Test Scene: Cornell Box

Classic test scene with colored walls and multiple objects.

```
Scene Composition:
- Green right wall, red left wall
- Gray floor and ceiling
- Green back wall
- Center: Metallic sphere (perfect mirror)
- Left: Red diffuse sphere
- Right: Green diffuse sphere
- Overhead: Point light source
```

**Build and Run:**
```bash
make phase2
./raytracer > cornell.ppm
```

**Expected Results:**
- 800x450 PPM image
- Colored walls visible
- Shadows under spheres
- Reflections in metal sphere
- Realistic lighting
- Render time: ~2-3 seconds

## Rendering Pipeline

```
For each pixel (i, j):
    Initialize color = (0, 0, 0)

    For each sample s:
        Generate ray through pixel
        color += ray_color(ray, scene, max_depth)

    Average color samples
    Apply gamma correction
    Write to PPM file

ray_color(ray, scene, depth):
    Find closest intersection
    if no hit:
        return sky gradient
    if depth == 0:
        return black

    Calculate Phong shading
    if material is reflective:
        Reflect ray and recurse
        Add reflected color

    return final color
```

## Files Created/Modified

| File | Lines | Purpose |
|------|-------|---------|
| [src/scene/scene.h](../src/scene/scene.h) | 50 | Scene graph |
| [src/scene/light.h](../src/scene/light.h) | 15 | Light sources |
| [src/renderer/renderer.h](../src/renderer/renderer.h) | 20 | Renderer interface |
| [src/renderer/renderer.cpp](../src/renderer/renderer.cpp) | 70 | Rendering logic |
| [src/main.cpp](../src/main.cpp) | 120 | Test scene setup |

## Performance

**Current Metrics:**
- Resolution: 800x450 (360K pixels)
- Max depth: 5 bounces
- Rays per pixel: ~6 (primary + reflections + shadows)
- Total rays: ~2 million
- Render time: ~2-3 seconds
- **Performance: ~0.7-1.5 MRays/sec**

**Bottlenecks:**
- Scalar ray traversal
- No spatial acceleration
- Single-threaded
- Shadow rays test all objects

## Next Steps

[→ Phase 3: SIMD Vectorization](phase3-simd.md)

Adds AVX2 SIMD to process 8 rays simultaneously for 4-6x speedup.

## Visual Results

**Cornell Box Scene Features:**
✅ Colored walls (red/green/gray)
✅ Hard shadows under all spheres
✅ Specular highlights on metal sphere
✅ Reflections of other spheres in metal sphere
✅ Smooth gradients on curved surfaces
✅ Proper falloff with distance

## Implementation Notes

### Phong vs. Physically Based Rendering

This implementation uses classic Phong shading, not physically based rendering (PBR). Differences:
- **Phong:** Ad-hoc model, looks plausible
- **PBR:** Energy-conserving, more realistic

For simplicity, we use Phong here.

### Shadow Quality

Current: **Hard shadows** (binary)
- Either fully lit or fully shadowed
- Sharp shadow edges
- No penumbras

Future: **Soft shadows**
- Multiple samples per light
- Area light sources
- Smooth penumbra regions

### Reflection Limit

Max depth of 5 means:
- Primary ray
- Up to 5 bounces
- Prevents infinite recursion
- Typically sufficient for simple scenes

## Troubleshooting

**Issue:** Image looks too dark
**Solution:** Check gamma correction is applied

**Issue:** No shadows visible
**Solution:** Verify shadow ray epsilon (0.001f) prevents self-shadowing

**Issue:** Reflections look wrong
**Solution:** Ensure reflect direction formula: `v - 2*dot(v,n)*n`

**Issue:** Colors blown out
**Solution:** Reduce light intensity or material albedo

## Lessons Learned

1. **Shadow rays** must have epsilon offset to avoid self-intersection
2. **Gamma correction** is essential for correct appearance
3. **Recursion depth** trades quality for performance
4. **Phong shading** provides good visual results despite being non-physical
5. **Scene graph** design impacts both code clarity and performance
