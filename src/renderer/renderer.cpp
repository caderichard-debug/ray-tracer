#include "renderer.h"
#include "../math/vec3_avx2.h"
#include <iostream>
#include <limits>

// Large number constant for ray tracing (instead of infinity to avoid -ffast-math issues)
const float infinity = 1.0e30f;

Color Renderer::compute_phong_shading(const HitRecord& rec, const Scene& scene) const {
    // Start with ambient component
    Color color = scene.ambient_light * rec.mat->albedo;

    // Add contribution from each light
    for (const auto& light : scene.lights) {
        // Vector from hit point to light
        Vec3 light_dir = light.position - rec.p;
        float light_distance = light_dir.length();
        Vec3 light_dir_normalized = AVX2::normalize_avx2(light_dir);

        // Early culling: if surface faces away from light, skip shadow ray
        float dot_product = AVX2::dot_product_avx2(rec.normal, light_dir_normalized);
        if (dot_product <= 0.0f) {
            // Surface faces away from light - no contribution, just ambient
            continue;
        }

        // Shadow ray
        Ray shadow_ray(rec.p, light_dir_normalized);
        bool in_shadow = scene.is_shadowed(shadow_ray, light_distance);

        if (!in_shadow) {
            // Diffuse component (Lambertian)
            float diffuse_intensity = dot_product; // Use precomputed dot
            Color diffuse = diffuse_intensity * light.intensity * rec.mat->albedo;

            // Specular component (Phong)
            Vec3 view_dir = AVX2::normalize_avx2(-light_dir_normalized);
            Vec3 reflect_dir = AVX2::reflect_avx2(-light_dir_normalized, rec.normal);
            float spec = std::pow(std::max(0.0f, AVX2::dot_product_avx2(view_dir, reflect_dir)), 32);
            Color specular = spec * light.intensity;

            color = color + diffuse + specular;
        }
    }

    return color;
}

Color Renderer::ray_color(const Ray& r, const Scene& scene, int depth) const {
    HitRecord rec;

    // Check if we hit anything
    if (!scene.hit(r, 0.001f, infinity, rec)) {
        // Background gradient (sky)
        Vec3 unit_direction = AVX2::normalize_avx2(r.direction());
        float t = 0.5f * (unit_direction.y + 1.0f);
        return (1.0f - t) * Color(1.0f, 1.0f, 1.0f) + t * Color(0.5f, 0.7f, 1.0f);
    }

    // If we've exceeded the ray bounce limit, no more light is gathered
    if (depth <= 0) {
        return Color(0, 0, 0);
    }

    // Calculate shading at hit point
    Color color = compute_phong_shading(rec, scene);

    // Handle reflections for metallic materials
    Ray scattered;
    Color attenuation;
    if (rec.mat->scatter(r, rec, attenuation, scattered)) {
        // Recursively trace reflected ray
        Color reflected_color = ray_color(scattered, scene, depth - 1);
        color = color + attenuation * reflected_color;
    }

    return color;
}
