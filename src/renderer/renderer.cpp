#include "renderer.h"
#include "../math/vec3_avx2.h"
#include "../math/pcg_random.h"
#include "../math/morton.h"
#include "../utils/simd_utils.h"
#include "../acceleration/bvh.h"
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
    Color color = AVX2::mul_color_avx2(scene.ambient_light, rec.mat->albedo);

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

        // Shadow ray (if enabled)
        bool in_shadow = false;
        if (enable_shadows) {
            Ray shadow_ray(rec.p, light_dir_normalized);
            in_shadow = scene.is_shadowed(shadow_ray, light_distance);
        }

        if (!in_shadow) {
            // Diffuse component (Lambertian) - use SIMD color operations
            float diffuse_intensity = dot_product;
            Color diffuse_light = AVX2::mul_color_avx2(light.intensity, rec.mat->albedo);
            Color diffuse = AVX2::scale_avx2(diffuse_light, diffuse_intensity);

            // Specular component (Phong) - use SIMD operations
            Vec3 view_dir = AVX2::normalize_avx2(-light_dir_normalized);
            Vec3 reflect_dir = AVX2::reflect_avx2(-light_dir_normalized, rec.normal);
            float view_dot_reflect = AVX2::dot_product_avx2(view_dir, reflect_dir);
            float spec = std::pow(std::max(0.0f, view_dot_reflect), rec.mat->shininess);
            Color specular = AVX2::scale_avx2(light.intensity, rec.mat->specular_intensity * spec);

            // Accumulate using SIMD
            color = AVX2::add_avx2(color, diffuse);
            color = AVX2::add_avx2(color, specular);
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
                        // Use PCG random number generator (thread-safe, fast)
                        float u0 = (i + random_float_pcg()) / (width - 1);
                        float v0 = (j + random_float_pcg()) / (height - 1);
                        Ray r0 = cam.get_ray(u0, v0);
                        pixel_color = pixel_color + ray_color(r0, scene, max_depth);

                        float u1 = (i + random_float_pcg()) / (width - 1);
                        float v1 = (j + random_float_pcg()) / (height - 1);
                        Ray r1 = cam.get_ray(u1, v1);
                        pixel_color = pixel_color + ray_color(r1, scene, max_depth);

                        float u2 = (i + random_float_pcg()) / (width - 1);
                        float v2 = (j + random_float_pcg()) / (height - 1);
                        Ray r2 = cam.get_ray(u2, v2);
                        pixel_color = pixel_color + ray_color(r2, scene, max_depth);

                        float u3 = (i + random_float_pcg()) / (width - 1);
                        float v3 = (j + random_float_pcg()) / (height - 1);
                        Ray r3 = cam.get_ray(u3, v3);
                        pixel_color = pixel_color + ray_color(r3, scene, max_depth);
                    }
                    // Handle remaining samples
                    for (; s < samples; ++s) {
                        float u = (i + random_float_pcg()) / (width - 1);
                        float v = (j + random_float_pcg()) / (height - 1);
                        Ray r = cam.get_ray(u, v);
                        pixel_color = pixel_color + ray_color(r, scene, max_depth);
                    }
#else
                    for (int s = 0; s < samples; ++s) {
                        // Use PCG random number generator (thread-safe, fast)
                        float u = (i + random_float_pcg()) / (width - 1);
                        float v = (j + random_float_pcg()) / (height - 1);

                        Ray r = cam.get_ray(u, v);
                        pixel_color = pixel_color + ray_color(r, scene, max_depth);
                    }
#endif

                    float scale = 1.0f / samples;
                    pixel_color = AVX2::scale_avx2(pixel_color, scale);

                    // Gamma correction using SIMD
                    pixel_color = AVX2::sqrt_avx2(pixel_color);

                    framebuffer[height - 1 - j][i] = pixel_color;
                }
            }
        }
    }
}

// Cache-Friendly Morton Z-Curve Rendering
void Renderer::render_morton(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                             int width, int height, int samples) {
    // Generate Morton-ordered pixel list for cache-friendly traversal
    std::vector<Morton::MortalPixel> morton_pixels = Morton::generate_morton_pixels(width, height);

    // Process pixels in Morton order (Z-curve) for better cache locality
    #pragma omp parallel for schedule(dynamic, 4)
    for (size_t idx = 0; idx < morton_pixels.size(); ++idx) {
        const auto& pixel = morton_pixels[idx];
        int i = pixel.x;
        int j = pixel.y;

        Color pixel_color(0, 0, 0);

#ifdef ENABLE_LOOP_UNROLL
        // Loop unrolling by 4 for better performance
        int s = 0;
        for (; s + 4 <= samples; s += 4) {
            // Use PCG random number generator (thread-safe, fast)
            float u0 = (i + random_float_pcg()) / (width - 1);
            float v0 = (j + random_float_pcg()) / (height - 1);
            Ray r0 = cam.get_ray(u0, v0);
            pixel_color = pixel_color + ray_color(r0, scene, max_depth);

            float u1 = (i + random_float_pcg()) / (width - 1);
            float v1 = (j + random_float_pcg()) / (height - 1);
            Ray r1 = cam.get_ray(u1, v1);
            pixel_color = pixel_color + ray_color(r1, scene, max_depth);

            float u2 = (i + random_float_pcg()) / (width - 1);
            float v2 = (j + random_float_pcg()) / (height - 1);
            Ray r2 = cam.get_ray(u2, v2);
            pixel_color = pixel_color + ray_color(r2, scene, max_depth);

            float u3 = (i + random_float_pcg()) / (width - 1);
            float v3 = (j + random_float_pcg()) / (height - 1);
            Ray r3 = cam.get_ray(u3, v3);
            pixel_color = pixel_color + ray_color(r3, scene, max_depth);
        }
        // Handle remaining samples
        for (; s < samples; ++s) {
            float u = (i + random_float_pcg()) / (width - 1);
            float v = (j + random_float_pcg()) / (height - 1);
            Ray r = cam.get_ray(u, v);
            pixel_color = pixel_color + ray_color(r, scene, max_depth);
        }
#else
        for (int s = 0; s < samples; ++s) {
            // Use PCG random number generator (thread-safe, fast)
            float u = (i + random_float_pcg()) / (width - 1);
            float v = (j + random_float_pcg()) / (height - 1);

            Ray r = cam.get_ray(u, v);
            pixel_color = pixel_color + ray_color(r, scene, max_depth);
        }
#endif

        float scale = 1.0f / samples;
        pixel_color = AVX2::scale_avx2(pixel_color, scale);

        // Gamma correction using SIMD
        pixel_color = AVX2::sqrt_avx2(pixel_color);

        framebuffer[height - 1 - j][i] = pixel_color;
    }
}

// Phase 3: SIMD Packet Tracing Implementation
void Renderer::render_simd_packets(const Camera& cam, const Scene& scene,
                                   std::vector<std::vector<Color>>& framebuffer,
                                   int width, int height, int samples) {
    Camera camera = cam;

    // Process pixels in 4x2 blocks (8 rays = AVX2 width)
    for (int j = 0; j < height; j += 2) {
        for (int i = 0; i < width; i += 4) {
            Color pixel_colors[8] = {Color(0,0,0), Color(0,0,0), Color(0,0,0), Color(0,0,0),
                                    Color(0,0,0), Color(0,0,0), Color(0,0,0), Color(0,0,0)};

            // Accumulate samples for each pixel in the 4x2 block
            for (int s = 0; s < samples; ++s) {
                Ray rays[8];
                int ray_idx = 0;

                // Generate 8 camera rays for this block
                for (int dy = 0; dy < 2 && j + dy < height; dy++) {
                    for (int dx = 0; dx < 4 && i + dx < width; dx++) {
                        float u = (i + dx + random_float_pcg()) / (width - 1);
                        float v = (j + dy + random_float_pcg()) / (height - 1);
                        rays[ray_idx++] = camera.get_ray(u, v);
                    }
                }

                // Pad to 8 rays if at edge/bottom
                while (ray_idx < 8) {
                    rays[ray_idx++] = rays[0];  // Duplicate first ray
                }

                // Trace each ray individually (scalar shading, but coherent rays)
                // Note: We could use SIMD for intersection, but scalar for shading is simpler
                for (int r = 0; r < 8; r++) {
                    pixel_colors[r] = pixel_colors[r] + ray_color(rays[r], scene, max_depth);
                }
            }

            // Average samples and write to framebuffer
            float scale = 1.0f / samples;
            int out_idx = 0;

            for (int dy = 0; dy < 2 && j + dy < height; dy++) {
                for (int dx = 0; dx < 4 && i + dx < width; dx++) {
                    Color final_color = AVX2::scale_avx2(pixel_colors[out_idx], scale);

                    // Gamma correction
                    final_color = AVX2::sqrt_avx2(final_color);

                    // Write to framebuffer (flip Y to match standard rendering)
                    int y_pos = height - 1 - j - dy;
                    framebuffer[y_pos][i + dx] = final_color;
                    out_idx++;
                }
            }
        }
    }
}

// Phase 3: BVH Acceleration Implementation
void Renderer::build_bvh(const Scene& scene) {
    // Extract spheres from scene for BVH
    bvh_spheres.clear();
    for (const auto& obj : scene.objects) {
        // Try to cast to Sphere (most objects in Cornell Box are spheres)
        auto sphere = std::dynamic_pointer_cast<Sphere>(obj);
        if (sphere) {
            bvh_spheres.push_back(sphere);
        }
    }

    std::cout << "BVH: Found " << bvh_spheres.size() << " spheres" << std::endl;

    if (bvh_spheres.empty()) {
        std::cout << "BVH: No spheres found, BVH not built" << std::endl;
        bvh_built = false;
        return;
    }

    // Build BVH from spheres
    BVH bvh;
    bvh.build(bvh_spheres);
    bvh_built = true;

    std::cout << "BVH: Build complete" << std::endl;
}

bool Renderer::hit_bvh(const Ray& r, float t_min, float t_max, HitRecord& rec, const Scene& scene) const {
    if (!bvh_built || bvh_spheres.empty()) {
        // Fallback to linear traversal
        return scene.hit(r, t_min, t_max, rec);
    }

    // Use BVH for spheres only
    BVH bvh;
    return bvh.hit(r, t_min, t_max, rec, bvh_spheres);
}
