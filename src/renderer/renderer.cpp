#include "renderer.h"
#include "wavefront_types.h"
#include "../math/vec3_avx2.h"
#include "../math/pcg_random.h"
#include "../math/morton.h"
#include "../math/ray_packet.h"
#include "../utils/simd_utils.h"
#include "../acceleration/bvh.h"
#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>

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
    Color color = AVX2::mul_color_avx2(scene.ambient_light, rec.mat->albedo);

    for (const auto& light : scene.lights) {
        // Vector from hit point to light
        Vec3 light_dir = light.position - rec.p;
        float light_distance = light_dir.length();
        Vec3 light_dir_normalized = AVX2::normalize_avx2(light_dir);

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

            color = AVX2::add_avx2(color, diffuse);
            color = AVX2::add_avx2(color, specular);
        }
    }

    return color;
}

Color Renderer::shade_hit(const HitRecord& rec, const Ray& r, const Scene& scene, int depth) const {
    if (depth <= 0) return Color(0, 0, 0);

    // First bounce uses the pre-found hit record (avoids re-intersecting the primary ray).
    // Remaining bounces reuse the iterative ray_color path.
    Color result = compute_phong_shading(rec, scene);
    Ray scattered;
    Color attenuation;
    if (enable_reflections) {
        if (rec.mat->scatter(r, rec, attenuation, scattered)) {
            result = result + attenuation * ray_color(scattered, scene, depth - 1);
        }
    } else {
        if (rec.mat->scatter(r, rec, attenuation, scattered)) {
            result = result + 0.3f * attenuation * rec.mat->albedo;
        }
    }
    return result;
}

Color Renderer::ray_color(const Ray& r, const Scene& scene, int depth) const {
    Color result(0, 0, 0);
    Color throughput(1, 1, 1);  // accumulated attenuation across bounces
    Ray current = r;

    for (int bounce = 0; bounce < depth; ++bounce) {
        HitRecord rec;
        const Frustum::Frustum* vf =
            (bounce == 0 && enable_frustum && frustum_valid) ? &view_frustum : nullptr;
        if (!trace_closest(current, 0.001f, infinity, rec, scene, vf)) {
            Vec3 unit_dir = unit_vector(current.direction());
            float t = 0.5f * (unit_dir.y + 1.0f);
            Color bg = (1.0f - t) * Color(1.0f, 1.0f, 1.0f) + t * Color(0.5f, 0.7f, 1.0f);
            result = result + throughput * bg;
            break;
        }

        result = result + throughput * compute_phong_shading(rec, scene);

        Ray scattered;
        Color attenuation;
        if (enable_reflections) {
            if (!rec.mat->scatter(current, rec, attenuation, scattered)) break;
            throughput = throughput * attenuation;
            current = scattered;
        } else {
            if (rec.mat->scatter(current, rec, attenuation, scattered)) {
                result = result + throughput * (0.3f * attenuation * rec.mat->albedo);
            }
            break;  // no recursive bounces when reflections are off
        }
    }

    return result;
}

// Wavefront Rendering Implementation
// Processes all rays in a tile simultaneously across bounce levels, using SIMD intersection
// at each bounce. This amortizes scene traversal overhead across the whole wave of rays.
void Renderer::render_wavefront(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                                int width, int height, int samples, bool accumulate_linear_radiance) {
    scene.build_simd_cache();

    const int tile_size = 32;

    #pragma omp parallel for schedule(dynamic, 1)
    for (int tile_y = 0; tile_y < height; tile_y += tile_size) {
        for (int tile_x = 0; tile_x < width; tile_x += tile_size) {
            int tx_end = std::min(tile_x + tile_size, width);
            int ty_end = std::min(tile_y + tile_size, height);
            int tile_w = tx_end - tile_x;
            int tile_h = ty_end - tile_y;

            // Per-pixel accumulation buffer for this tile
            std::vector<Color> accum(tile_w * tile_h, Color(0, 0, 0));

            // Generate all primary rays for this tile (pixels * samples)
            std::vector<WavefrontRay> current_wave, next_wave;
            current_wave.reserve(tile_w * tile_h * samples);

            for (int j = tile_y; j < ty_end; j++) {
                for (int i = tile_x; i < tx_end; i++) {
                    for (int s = 0; s < samples; s++) {
                        float u = (i + random_float_pcg()) / (width - 1);
                        float v = (j + random_float_pcg()) / (height - 1);
                        WavefrontRay wr;
                        wr.ray = cam.get_ray(u, v);
                        wr.throughput = Color(1, 1, 1);
                        wr.radiance = Color(0, 0, 0);
                        wr.pixel_x = i;
                        wr.pixel_y = j;
                        current_wave.push_back(wr);
                    }
                }
            }

            // Bounce loop: each iteration processes the entire wave via SIMD, then
            // rays that scatter continue into next_wave for the next bounce.
            for (int bounce = 0; bounce < max_depth && !current_wave.empty(); bounce++) {
                next_wave.clear();
                next_wave.reserve(current_wave.size());

                int n = (int)current_wave.size();
                for (int chunk_start = 0; chunk_start < n; chunk_start += 8) {
                    int chunk_count = std::min(8, n - chunk_start);

                    Ray rays[8];
                    float dir_lens[8];
                    for (int k = 0; k < 8; k++) {
                        int idx = (k < chunk_count) ? (chunk_start + k) : chunk_start;
                        rays[k] = current_wave[idx].ray;
                        dir_lens[k] = rays[k].direction().length();
                    }

                    RayPacket packet;
                    packet.load_rays(rays);
                    std::vector<HitRecord> hit_records(8);
                    scene.hit_packet(packet, 0.001f, infinity, hit_records);

                    for (int k = 0; k < chunk_count; k++) {
                        WavefrontRay& wr = current_wave[chunk_start + k];
                        HitRecord& hr = hit_records[k];

                        int px = wr.pixel_x - tile_x;
                        int py = wr.pixel_y - tile_y;
                        int pidx = py * tile_w + px;

                        bool hit_something = false;
                        HitRecord final_rec;
                        Ray final_ray = wr.ray;

                        if (hr.t < infinity && hr.mat != nullptr) {
                            // SIMD found a sphere. Convert t to original-ray units and check
                            // whether a non-sphere (triangle/quad) is actually closer.
                            float t_orig = hr.t / dir_lens[k];
                            HitRecord ns_rec;
                            if (scene.hit_non_spheres(wr.ray, 0.001f, t_orig, ns_rec)) {
                                final_rec = ns_rec;
                                hit_something = true;
                            } else {
                                Vec3 unit_dir = wr.ray.direction() / dir_lens[k];
                                final_ray = Ray(wr.ray.origin(), unit_dir);
                                final_rec = hr;
                                hit_something = true;
                            }
                        } else {
                            // No sphere hit — check non-spheres only
                            HitRecord ns_rec;
                            if (scene.hit_non_spheres(wr.ray, 0.001f, infinity, ns_rec)) {
                                final_rec = ns_rec;
                                hit_something = true;
                            }
                        }

                        if (hit_something) {
                            wr.radiance = wr.radiance + wr.throughput * compute_phong_shading(final_rec, scene);
                            Ray scattered;
                            Color attenuation;
                            if (enable_reflections && final_rec.mat->scatter(final_ray, final_rec, attenuation, scattered)) {
                                WavefrontRay next_wr = wr;
                                next_wr.ray = scattered;
                                next_wr.throughput = wr.throughput * attenuation;
                                next_wave.push_back(next_wr);
                            } else {
                                if (!enable_reflections && final_rec.mat->scatter(final_ray, final_rec, attenuation, scattered)) {
                                    wr.radiance = wr.radiance + wr.throughput * (0.3f * attenuation * final_rec.mat->albedo);
                                }
                                accum[pidx] = accum[pidx] + wr.radiance;
                            }
                        } else {
                            // Miss — add sky background and terminate
                            Vec3 unit_dir = unit_vector(wr.ray.direction());
                            float t = 0.5f * (unit_dir.y + 1.0f);
                            Color bg = (1.0f - t) * Color(1, 1, 1) + t * Color(0.5f, 0.7f, 1.0f);
                            accum[pidx] = accum[pidx] + wr.radiance + wr.throughput * bg;
                        }
                    }
                }

                current_wave = std::move(next_wave);
            }

            // Rays still active at max_depth get background
            for (auto& wr : current_wave) {
                Vec3 unit_dir = unit_vector(wr.ray.direction());
                float t = 0.5f * (unit_dir.y + 1.0f);
                Color bg = (1.0f - t) * Color(1, 1, 1) + t * Color(0.5f, 0.7f, 1.0f);
                int px = wr.pixel_x - tile_x;
                int py = wr.pixel_y - tile_y;
                int pidx = py * tile_w + px;
                accum[pidx] = accum[pidx] + wr.radiance + wr.throughput * bg;
            }

            // Write tile: row j matches SIMD / get_ray image row (consumer reads framebuffer[j][i])
            float scale = 1.0f / samples;
            for (int j = tile_y; j < ty_end; j++) {
                for (int i = tile_x; i < tx_end; i++) {
                    int pidx = (j - tile_y) * tile_w + (i - tile_x);
                    Color linear = AVX2::scale_avx2(accum[pidx], scale);
                    if (accumulate_linear_radiance) {
                        framebuffer[j][i] = framebuffer[j][i] + linear;
                    } else {
                        framebuffer[j][i] = AVX2::sqrt_avx2(linear);
                    }
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

        // Row j matches SIMD / get_ray convention (see render_simd_packets and interactive blit).
        framebuffer[j][i] = pixel_color;
    }
}

// Phase 3: SIMD Packet Tracing Implementation
void Renderer::render_simd_packets(const Camera& cam, const Scene& scene,
                                   std::vector<std::vector<Color>>& framebuffer,
                                   int width, int height, int samples, bool accumulate_linear_radiance) {
    Camera camera = cam;

    // Build SIMD sphere cache once (single-threaded) before the parallel loop.
    // hit_packet reads this cache; parallel threads never write it.
    scene.build_simd_cache();

    // Process pixels in 4x2 blocks (8 rays = AVX2 width)
    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = height - 2; j >= 0; j -= 2) {
        for (int i = 0; i < width; i += 4) {
            if (j + 1 >= height) continue;
            Color pixel_colors[8] = {};

            for (int s = 0; s < samples; ++s) {
                Ray rays[8];
                float dir_lens[8];  // original (unnormalized) direction magnitudes
                int ray_idx = 0;

                for (int dy = 0; dy < 2 && j + dy < height; dy++) {
                    for (int dx = 0; dx < 4 && i + dx < width; dx++) {
                        float u = (i + dx + random_float_pcg()) / (width - 1);
                        float v = (j + dy + random_float_pcg()) / (height - 1);
                        rays[ray_idx] = camera.get_ray(u, v);
                        dir_lens[ray_idx] = rays[ray_idx].direction().length();
                        ray_idx++;
                    }
                }
                while (ray_idx < 8) {
                    rays[ray_idx] = rays[0];
                    dir_lens[ray_idx] = dir_lens[0];
                    ray_idx++;
                }

                RayPacket packet;
                packet.load_rays(rays);  // normalizes directions internally

                std::vector<HitRecord> hit_records(8);
                scene.hit_packet(packet, 0.001f, infinity, hit_records);

                for (int r = 0; r < 8; r++) {
                    Color sample_color;
                    if (hit_records[r].t < infinity && hit_records[r].mat != nullptr) {
                        // SIMD found a sphere hit at t_simd (normalized-ray units).
                        // Convert to original-ray t-space to compare with non-sphere hits:
                        //   t_original = t_simd / dir_len  (since unit_dir = dir / dir_len)
                        float t_sphere_orig = hit_records[r].t / dir_lens[r];

                        // Check non-sphere objects (triangles, quads) for a closer hit.
                        HitRecord nonsphere_rec;
                        if (scene.hit_non_spheres(rays[r], 0.001f, t_sphere_orig, nonsphere_rec)) {
                            sample_color = shade_hit(nonsphere_rec, rays[r], scene, max_depth);
                        } else {
                            // Sphere is the closest hit — shade using unit-direction ray
                            // (scatter functions normalize r_in themselves, so this is fine)
                            Vec3 unit_dir = rays[r].direction() / dir_lens[r];
                            Ray unit_ray(rays[r].origin(), unit_dir);
                            sample_color = shade_hit(hit_records[r], unit_ray, scene, max_depth);
                        }
                    } else {
                        // No sphere hit — full scalar path handles triangles, quads, background
                        sample_color = ray_color(rays[r], scene, max_depth);
                    }
                    pixel_colors[r] = pixel_colors[r] + sample_color;
                }
            }

            float scale = 1.0f / samples;
            int out_idx = 0;
            for (int dy = 0; dy < 2 && j + dy < height; dy++) {
                for (int dx = 0; dx < 4 && i + dx < width; dx++) {
                    Color linear = AVX2::scale_avx2(pixel_colors[out_idx], scale);
                    if (accumulate_linear_radiance) {
                        framebuffer[j + dy][i + dx] = framebuffer[j + dy][i + dx] + linear;
                    } else {
                        framebuffer[j + dy][i + dx] = AVX2::sqrt_avx2(linear);
                    }
                    out_idx++;
                }
            }
        }
    }
}

// Phase 3: BVH + closest-hit (spheres via BVH, quads/meshes linear via simd non-sphere cache)
void Renderer::release_bvh() {
    bvh_accel.reset();
    bvh_spheres.clear();
    bvh_built = false;
}

void Renderer::sync_frustum(const Camera& cam) {
    view_frustum = cam.make_view_frustum(0.01f, 1.0e4f);
    frustum_valid = true;
}

bool Renderer::trace_closest(const Ray& r, float t_min, float t_max, HitRecord& rec, const Scene& scene,
                             const Frustum::Frustum* view_frustum) const {
    if (!enable_bvh || !bvh_built || !bvh_accel) {
        return scene.hit(r, t_min, t_max, rec, view_frustum);
    }

    bool any = false;
    float closest = t_max;
    HitRecord best;
    HitRecord tmp;

    if (scene.hit_non_spheres(r, t_min, closest, tmp)) {
        best = tmp;
        closest = tmp.t;
        any = true;
    }
    if (bvh_accel->hit(r, t_min, closest, tmp, bvh_spheres, view_frustum)) {
        if (!any || tmp.t < best.t) {
            best = tmp;
        }
        any = true;
    }
    if (any) {
        rec = best;
    }
    return any;
}

void Renderer::build_bvh(const Scene& scene) {
    bvh_accel.reset();
    bvh_spheres.clear();
    for (const auto& obj : scene.objects) {
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

    bvh_accel = std::make_unique<BVH>();
    bvh_accel->build(bvh_spheres);
    bvh_built = true;

    std::cout << "BVH: Build complete" << std::endl;
}
