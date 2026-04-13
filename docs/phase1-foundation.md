# Phase 1: Foundation

**Status:** ✅ COMPLETE
**Build Target:** `make phase1`
**Files:** [src/math/](../src/math/), [src/primitives/](../src/primitives/), [src/material/](../src/material/), [src/camera/](../src/camera/)

## Overview

Phase 1 establishes the mathematical and geometric foundation for the ray tracer. This phase implements all core data structures and algorithms needed for basic ray casting, without any rendering features yet.

## Key Components

### 1. Vec3 Class ([vec3.h](../src/math/vec3.h))

The fundamental 3D vector class used throughout the ray tracer.

```cpp
class Vec3 {
    float x, y, z;

    // Vector operations
    Vec3 operator+(const Vec3& v) const;
    Vec3 operator-(const Vec3& v) const;
    Vec3 operator*(float t) const;
    float dot(const Vec3& v) const;
    Vec3 cross(const Vec3& v) const;
    float length() const;
    Vec3 normalized() const;
};
```

**Features:**
- Complete vector arithmetic (add, subtract, multiply, divide)
- Dot product and cross product
- Length calculation and normalization
- Random vector generation for Monte Carlo techniques

**Design Decisions:**
- Inline operations for performance
- Const-correctness for safety
- Operator overloading for natural syntax

### 2. Ray Class ([ray.h](../src/math/ray.h))

Represents a ray with origin and direction.

```cpp
class Ray {
    Point3 orig;    // Origin point
    Vec3 dir;       // Direction vector (normalized)
    float tm;       // Time (for motion blur, unused)

    Point3 at(float t) const;  // Point at distance t
};
```

**Key Method:**
- `at(t)`: Returns point `P(t) = origin + t * direction`

### 3. Sphere Primitive ([sphere.h](../src/primitives/sphere.h))

First geometric primitive with analytic ray-sphere intersection.

**Intersection Algorithm:**
```cpp
// Ray: P(t) = origin + t*direction
// Sphere: |point - center|² = radius²

// Substitute ray into sphere equation:
// |origin + t*direction - center|² = radius²

// This gives us a quadratic equation: at² + bt + c = 0
// Where:
//   a = direction·direction (should be 1 if normalized)
//   b = 2*(origin-center)·direction
//   c = |origin-center|² - radius²

// Solve: t = (-b ± √(b²-4ac)) / 2a
```

**Implementation Details:**
- Uses quadratic formula with discriminant check
- Returns closest intersection in valid range [t_min, t_max]
- Properly sets hit record with normal and material
- Handles front/back face detection

### 4. Material System ([material.h](../src/material/material.h))

Abstract base class for material properties.

```cpp
class Material {
public:
    Color albedo;  // Base color

    virtual bool scatter(const Ray& r_in, const HitRecord& rec,
                        Color& attenuation, Ray& scattered) const = 0;
};
```

**Implemented Materials:**

#### Lambertian (Diffuse)
- Random scattering in hemisphere around normal
- Perfectly matte surface
- Albedo determines color

#### Metal (Reflective)
- Perfect or fuzzy reflection
- Adjustable roughness (fuzz parameter)
- Specular highlights

### 5. Camera Class ([camera.h](../src/camera/camera.h))

Pinhole camera model for perspective projection.

**Parameters:**
- `lookfrom`: Camera position
- `lookat`: Point camera is looking at
- `vup`: Up direction (typically [0,1,0])
- `vfov`: Vertical field of view (degrees)
- `aspect_ratio`: Width / height

**Ray Generation:**
```cpp
Ray get_ray(float u, float v) const;
// u, v in [0, 1] representing normalized pixel coordinates
```

## Testing

**Test Scene:**
- Single red sphere at (0, 0, -1)
- Radius 0.5
- White background with gradient sky

**Build and Run:**
```bash
make phase1
./raytracer > output.ppm
```

**Expected Output:**
- 800x450 PPM image
- Red sphere on gradient background
- Should render in <1 second

## Files Created

| File | Lines | Purpose |
|------|-------|---------|
| [src/math/vec3.h](../src/math/vec3.h) | 150 | 3D vector math |
| [src/math/ray.h](../src/math/ray.h) | 30 | Ray representation |
| [src/primitives/primitive.h](../src/primitives/primitive.h) | 35 | Primitive base class |
| [src/primitives/sphere.h](../src/primitives/sphere.h) | 60 | Sphere implementation |
| [src/material/material.h](../src/material/material.h) | 70 | Material classes |
| [src/camera/camera.h](../src/camera/camera.h) | 80 | Camera model |

## Next Steps

[→ Phase 2: Basic Rendering](phase2-rendering.md)

Adds lighting, shadows, and reflections to create proper rendered images.

## Performance Notes

- Scalar implementation (no SIMD yet)
- Single-threaded
- ~1-2 MRays/sec on i7
- Baseline for future optimizations

## Lessons Learned

1. **Analytic intersections** are much faster than numerical methods
2. **Hit record structure** should include all data needed for shading
3. **Face normals** must be flipped for rays hitting from inside
4. **Material system** design is critical for extensibility
