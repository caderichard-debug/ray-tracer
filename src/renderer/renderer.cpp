#include "renderer.h"
#include "../math/vec3_avx2.h"
#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>

// Large number constant for ray tracing (instead of infinity to avoid -ffast-math issues)
const float infinity = 1.0e30f;

// Progressive Rendering Implementation
void Renderer::initialize_progressive(int width, int height) {
    accumulation_buffer.resize(height, std::vector<Color>(width, Color(0, 0, 0)));
    sample_count.resize(height, std::vector<int>(width, 0));
    current_pass = 0;
}

void Renderer::reset_progressive() {
    int height = accumulation_buffer.size();
    int width = height > 0 ? accumulation_buffer[0].size() : 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            accumulation_buffer[y][x] = Color(0, 0, 0);
            sample_count[y][x] = 0;
        }
    }
    current_pass = 0;
}

bool Renderer::is_progressive_complete() const {
    return current_pass >= max_passes;
}

Color Renderer::get_progressive_color(int x, int y) const {
    if (accumulation_buffer.empty() || sample_count.empty()) {
        return Color(0, 0, 0);
    }

    int samples = sample_count[y][x];
    if (samples == 0) {
        return Color(0, 0, 0); // Black for unrendered pixels
    }

    return accumulation_buffer[y][x] / samples;
}

// Adaptive Sampling Implementation
bool Renderer::should_continue_sampling(int x, int y, const std::vector<Color>& samples) const {
    (void)x;  // Currently unused, reserved for future spatial analysis
    (void)y;  // Currently unused, reserved for future spatial analysis

    if (static_cast<int>(samples.size()) < min_samples) {
        return true; // Need at least minimum samples
    }

    if (static_cast<int>(samples.size()) >= max_samples) {
        return false; // Reached maximum samples
    }

    // Compute variance to decide if we need more samples
    float variance = compute_variance(samples);
    return variance > variance_threshold;
}

float Renderer::compute_variance(const std::vector<Color>& samples) const {
    if (samples.size() < 2) {
        return 0.0f;
    }

    // Compute mean color
    Color mean(0, 0, 0);
    for (const auto& sample : samples) {
        mean = mean + sample;
    }
    mean = mean / samples.size();

    // Compute variance (average squared deviation from mean)
    float variance = 0.0f;
    for (const auto& sample : samples) {
        float dr = sample.x - mean.x;
        float dg = sample.y - mean.y;
        float db = sample.z - mean.z;
        variance += (dr * dr + dg * dg + db * db) / 3.0f;
    }
    variance /= samples.size();

    return variance;
}

Color Renderer::compute_phong_shading(const HitRecord& rec, const Scene& scene) const {
    // Start with ambient component
    Color color = scene.ambient_light * rec.mat->albedo;

    // Add contribution from each light
    for (const auto& light : scene.lights) {
        // Vector from hit point to light
        Vec3 light_dir = light.position - rec.p;
        float light_distance = light_dir.length();
        Vec3 light_dir_normalized = light_dir.normalized();  // Use regular normalize for precision

        // Early culling: if surface faces away from light, skip shadow ray
        float dot_product = dot(rec.normal, light_dir_normalized);  // Use regular dot for precision
        if (dot_product <= 0.0f) {
            // Surface faces away from light - no contribution, just ambient
            continue;
        }

        // Shadow ray (if enabled)
        bool in_shadow = false;
        if (enable_shadows) {
            Ray shadow_ray(rec.p, light_dir_normalized);
            in_shadow = scene.is_shadowed(shadow_ray, light_distance);
        }

        if (!in_shadow) {
            // Diffuse component (Lambertian)
            float diffuse_intensity = dot_product; // Use precomputed dot
            Color diffuse = diffuse_intensity * light.intensity * rec.mat->albedo;

            // Specular component (Phong) - use material properties
            Vec3 view_dir = (-light_dir_normalized).normalized();
            Vec3 reflect_dir = AVX2::reflect_avx2(-light_dir_normalized, rec.normal);
            float spec = std::pow(std::max(0.0f, dot(view_dir, reflect_dir)), rec.mat->shininess);
            Color specular = rec.mat->specular_intensity * spec * light.intensity;

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
        Vec3 unit_direction = unit_vector(r.direction());
        float t = 0.5f * (unit_direction.y + 1.0f);
        return (1.0f - t) * Color(1.0f, 1.0f, 1.0f) + t * Color(0.5f, 0.7f, 1.0f);
    }

    // If we've exceeded the ray bounce limit, no more light is gathered
    if (depth <= 0) {
        return Color(0, 0, 0);
    }

    // Calculate shading at hit point
    Color color = compute_phong_shading(rec, scene);

    // Handle reflections for metallic materials (if enabled)
    Ray scattered;
    Color attenuation;
    if (enable_reflections && rec.mat->scatter(r, rec, attenuation, scattered)) {
        // Recursively trace reflected ray
        Color reflected_color = ray_color(scattered, scene, depth - 1);
        color = color + attenuation * reflected_color;
    } else if (!enable_reflections && rec.mat->scatter(r, rec, attenuation, scattered)) {
        // Even when reflections are disabled, still attenuate for metallic appearance
        color = color + 0.3f * attenuation * rec.mat->albedo;
    }

    return color;
}

// Wavefront Rendering Implementation
void Renderer::render_wavefront(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                                int width, int height, int samples) {
    // Simple wavefront implementation - process rays in batches for better cache coherence
    // In a full implementation, this would sort rays by direction/material and process them in waves

    // For now, we'll use the standard rendering but with tiling for better cache utilization
    const int tile_size = 64; // Process 64x64 pixel tiles

    #pragma omp parallel for schedule(dynamic, 1)
    for (int tile_y = 0; tile_y < height; tile_y += tile_size) {
        for (int tile_x = 0; tile_x < width; tile_x += tile_size) {
            // Process a tile of pixels
            int tile_end_x = std::min(tile_x + tile_size, width);
            int tile_end_y = std::min(tile_y + tile_size, height);

            for (int j = tile_y; j < tile_end_y; ++j) {
                for (int i = tile_x; i < tile_end_x; ++i) {
                    Color pixel_color(0, 0, 0);

#ifdef ENABLE_LOOP_UNROLL
                    // Loop unrolling by 4 for better performance
                    int s = 0;
                    for (; s + 4 <= samples; s += 4) {
                        // Helper function for random float
                        auto random_float = []() {
                            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        };

                        // Unroll 4 samples
                        float u0 = (i + random_float()) / (width - 1);
                        float v0 = (j + random_float()) / (height - 1);
                        Ray r0 = cam.get_ray(u0, v0);
                        pixel_color = pixel_color + ray_color(r0, scene, max_depth);

                        float u1 = (i + random_float()) / (width - 1);
                        float v1 = (j + random_float()) / (height - 1);
                        Ray r1 = cam.get_ray(u1, v1);
                        pixel_color = pixel_color + ray_color(r1, scene, max_depth);

                        float u2 = (i + random_float()) / (width - 1);
                        float v2 = (j + random_float()) / (height - 1);
                        Ray r2 = cam.get_ray(u2, v2);
                        pixel_color = pixel_color + ray_color(r2, scene, max_depth);

                        float u3 = (i + random_float()) / (width - 1);
                        float v3 = (j + random_float()) / (height - 1);
                        Ray r3 = cam.get_ray(u3, v3);
                        pixel_color = pixel_color + ray_color(r3, scene, max_depth);
                    }
                    // Handle remaining samples
                    for (; s < samples; ++s) {
                        auto random_float = []() {
                            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        };
                        float u = (i + random_float()) / (width - 1);
                        float v = (j + random_float()) / (height - 1);
                        Ray r = cam.get_ray(u, v);
                        pixel_color = pixel_color + ray_color(r, scene, max_depth);
                    }
#else
                    for (int s = 0; s < samples; ++s) {
                        // Helper function for random float
                        auto random_float = []() {
                            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        };

                        float u = (i + random_float()) / (width - 1);
                        float v = (j + random_float()) / (height - 1);

                        Ray r = cam.get_ray(u, v);
                        pixel_color = pixel_color + ray_color(r, scene, max_depth);
                    }
#endif

                    float scale = 1.0f / samples;
                    pixel_color = pixel_color * scale;

                    // Gamma correction
                    pixel_color.x = std::sqrt(pixel_color.x);
                    pixel_color.y = std::sqrt(pixel_color.y);
                    pixel_color.z = std::sqrt(pixel_color.z);

                    framebuffer[height - 1 - j][i] = pixel_color;
                }
            }
        }
    }
}
