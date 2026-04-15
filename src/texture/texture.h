#ifndef TEXTURE_H
#define TEXTURE_H

#include "../math/vec3.h"
#include <cmath>

// Base texture class
class Texture {
public:
    virtual ~Texture() = default;
    virtual Color value(float u, float v, const Point3& p) const = 0;
};

// Solid color texture
class SolidColor : public Texture {
public:
    Color color_value;

    SolidColor() {}
    SolidColor(Color c) : color_value(c) {}

    Color value(float u, float v, const Point3& p) const override {
        (void)u; (void)v; (void)p;
        return color_value;
    }
};

// Checkerboard pattern
class CheckerTexture : public Texture {
public:
    std::shared_ptr<Texture> odd;
    std::shared_ptr<Texture> even;
    float scale;

    CheckerTexture() {}
    CheckerTexture(std::shared_ptr<Texture> t1, std::shared_ptr<Texture> t2, float s = 10.0f)
        : odd(t1), even(t2), scale(s) {}

    Color value(float u, float v, const Point3& p) const override {
        // Optimized: Use integer-based pattern instead of expensive sin() calls
        int ix = static_cast<int>(p.x * scale);
        int iy = static_cast<int>(p.y * scale);
        int iz = static_cast<int>(p.z * scale);
        int sum = ix + iy + iz;
        if (sum % 2 == 0) {
            return even->value(u, v, p);
        } else {
            return odd->value(u, v, p);
        }
    }
};

// Gradient texture (linear interpolation)
class GradientTexture : public Texture {
public:
    Color color1;
    Color color2;
    Vec3 direction;  // Gradient direction (normalized in constructor)
    float scale;     // Gradient scale factor
    float offset;    // Offset for positioning the gradient

    // Auto-calibrating constructor - takes bounding box
    GradientTexture(Color c1, Color c2, Vec3 dir, Point3 bounds_min, Point3 bounds_max)
        : color1(c1), color2(c2), direction(dir.normalized()) {
        // Project bounds onto direction to find min/max values along gradient
        float min_val = dot(bounds_min, direction);
        float max_val = dot(bounds_max, direction);
        float range = max_val - min_val;

        // Calculate scale and offset to map [min_val, max_val] to [0, 1]
        scale = (range > 0.0001f) ? (1.0f / range) : 1.0f;
        offset = -min_val * scale;
    }

    // Manual constructor - for backward compatibility or custom gradients
    GradientTexture(Color c1, Color c2, Vec3 dir = Vec3(0, 1, 0), float s = 0.25f, float o = 0.0f)
        : color1(c1), color2(c2), direction(dir.normalized()), scale(s), offset(o) {}

    Color value(float u, float v, const Point3& p) const override {
        (void)u; (void)v;
        // Simple linear gradient based on position in direction
        float value = dot(p, direction);
        // Map approximately to [0, 1] range using scale factor and offset
        float t = std::clamp(value * scale + offset, 0.0f, 1.0f);
        return (1.0f - t) * color1 + t * color2;
    }
};

// Noise texture (simple pseudo-random)
class NoiseTexture : public Texture {
public:
    Color color1;
    Color color2;
    float scale;
    int octaves;
    float persistence;

    NoiseTexture(Color c1, Color c2, float s = 5.0f, int oct = 4, float pers = 0.5f)
        : color1(c1), color2(c2), scale(s), octaves(oct), persistence(pers) {}

    // Fast integer-based hash function (much cheaper than sin())
    float noise(float x, float y, float z) const {
        // Convert to integer coordinates for hashing
        int ix = static_cast<int>(x);
        int iy = static_cast<int>(y);
        int iz = static_cast<int>(z);

        // Simple hash function
        int hash = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791);
        // Use bit manipulation for better distribution
        hash = hash * 1103515245 + 12345;
        hash = (hash ^ (hash >> 16)) * 1103515245 + 12345;

        // Convert to float in [-1, 1] range
        float n = (hash % 1000) / 500.0f - 1.0f;
        return n;
    }

    // Optimized Fractal Brownian Motion
    float fbm(float x, float y, float z) const {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float max_value = 0.0f;

        for (int i = 0; i < octaves; i++) {
            total += noise(x * frequency, y * frequency, z * frequency) * amplitude;
            max_value += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }

        return total / max_value;  // Returns range approximately [-1, 1]
    }

    Color value(float u, float v, const Point3& p) const override {
        (void)u; (void)v;
        // Optimized: Direct position usage without scaling multiplication
        float n = fbm(p.x * scale, p.y * scale, p.z * scale);
        // Remap from [-1, 1] to [0, 1] with more contrast
        float t = 0.5f * (n + 1.0f);
        t = std::clamp(t, 0.0f, 1.0f);
        return (1.0f - t) * color1 + t * color2;
    }
};

// Stripe pattern
class StripeTexture : public Texture {
public:
    Color color1;
    Color color2;
    float scale;
    float angle;

    StripeTexture(Color c1, Color c2, float s = 5.0f, float a = 0.0f)
        : color1(c1), color2(c2), scale(s), angle(a) {}

    Color value(float u, float v, const Point3& p) const override {
        (void)u; (void)v;
        // Optimized: Simple stripes without trigonometric rotation
        // Just use x-coordinate with scale for stripes
        float stripe = std::sin(p.x * scale);
        float t = (stripe > 0.0f) ? 1.0f : 0.0f;  // Hard edges
        return t * color2 + (1.0f - t) * color1;
    }
};

#endif // TEXTURE_H
