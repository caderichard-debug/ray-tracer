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
        float sines = std::sin(scale * p.x) * std::sin(scale * p.y) * std::sin(scale * p.z);
        if (sines < 0) {
            return odd->value(u, v, p);
        } else {
            return even->value(u, v, p);
        }
    }
};

// Gradient texture (linear interpolation)
class GradientTexture : public Texture {
public:
    Color color1;
    Color color2;
    Vec3 direction;  // Gradient direction

    GradientTexture(Color c1, Color c2, Vec3 dir = Vec3(0, 1, 0))
        : color1(c1), color2(c2), direction(dir.normalized()) {}

    Color value(float u, float v, const Point3& p) const override {
        float t = 0.5f * (dot(p.normalized(), direction) + 1.0f);
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

    // Simple pseudo-random noise function
    float noise(float x, float y, float z) const {
        // Simple hash-based noise
        float hash = std::sin(x * 12.9898f + y * 78.233f + z * 37.719f) * 43758.5453f;
        return hash - std::floor(hash);
    }

    // Fractal Brownian Motion
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

        return total / max_value;
    }

    Color value(float u, float v, const Point3& p) const override {
        float n = fbm(p.x * scale, p.y * scale, p.z * scale);
        // Remap from [-1, 1] to [0, 1]
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
        // Rotate point for angled stripes
        float x = p.x * std::cos(angle) - p.y * std::sin(angle);
        float stripe = std::sin(x * scale);
        float t = 0.5f * (stripe + 1.0f);
        t = std::clamp(t, 0.0f, 1.0f);
        return (1.0f - t) * color1 + t * color2;
    }
};

#endif // TEXTURE_H
