#ifndef RENDERER_H
#define RENDERER_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../scene/scene.h"
#include "../camera/camera.h"
#include "../material/material.h"
#include "../primitives/sphere.h"
#include "../acceleration/bvh.h"
#include "../math/frustum.h"
#include <vector>
#include <memory>

class Renderer {
public:
    int max_depth; // Maximum recursion depth for reflections
    bool enable_shadows; // Enable/disable shadow rays
    bool enable_reflections; // Enable/disable reflection rays

    // Progressive rendering state
    bool enable_progressive; // Enable progressive rendering
    int current_pass; // Current progressive pass
    int max_passes; // Maximum number of progressive passes
    std::vector<std::vector<Color>> accumulation_buffer; // Accumulates samples across passes
    std::vector<std::vector<int>> sample_count; // Tracks samples per pixel

    // Adaptive sampling state
    bool enable_adaptive; // Enable adaptive sampling
    float variance_threshold; // Variance threshold for adaptive sampling
    int min_samples; // Minimum samples per pixel
    int max_samples; // Maximum samples per pixel

    // Wavefront rendering state
    bool enable_wavefront; // Enable wavefront rendering
    int wavefront_size; // Number of rays to process in each wave

    // Cache-friendly rendering state
    bool enable_morton; // Enable Morton Z-curve ordering
    bool enable_frustum; // Enable frustum culling (primary rays; see sync_frustum)
    bool enable_stratified; // Jittered grid samples per pixel (outer loop, not RNG white noise)

    // Phase 3: SIMD packet tracing
    bool enable_simd_packets; // Enable AVX2 ray packet tracing

    // Phase 3: BVH acceleration (spheres in tree + linear non-spheres; requires scene.build_simd_cache())
    bool enable_bvh;
    std::vector<std::shared_ptr<Sphere>> bvh_spheres;
    bool bvh_built;
    std::unique_ptr<BVH> bvh_accel;

    Frustum::Frustum view_frustum;
    bool frustum_valid;

    Renderer() : max_depth(5), enable_shadows(true), enable_reflections(true),
                 enable_progressive(false), current_pass(0), max_passes(10),
                 enable_adaptive(false), variance_threshold(0.01f), min_samples(4), max_samples(64),
                 enable_wavefront(false), wavefront_size(1024), enable_morton(false), enable_frustum(false),
                 enable_stratified(false), enable_simd_packets(false), enable_bvh(false), bvh_built(false),
                 frustum_valid(false) {}

    Renderer(int depth) : max_depth(depth), enable_shadows(true), enable_reflections(true),
                          enable_progressive(false), current_pass(0), max_passes(10),
                          enable_adaptive(false), variance_threshold(0.01f), min_samples(4), max_samples(64),
                          enable_wavefront(false), wavefront_size(1024), enable_morton(false), enable_frustum(false),
                          enable_stratified(false), enable_simd_packets(false), enable_bvh(false), bvh_built(false),
                          frustum_valid(false) {}

    // Main ray tracing function
    Color ray_color(const Ray& r, const Scene& scene, int depth) const;

    // Calculate Phong shading at a hit point
    Color compute_phong_shading(const HitRecord& rec, const Scene& scene) const;

    // Shade a pre-found hit with scatter/reflection (matches ray_color logic, avoids re-intersection)
    Color shade_hit(const HitRecord& rec, const Ray& r, const Scene& scene, int depth) const;

    // Progressive rendering functions
    void initialize_progressive(int width, int height);
    void reset_progressive();
    bool is_progressive_complete() const;
    Color get_progressive_color(int x, int y) const;

    // Adaptive sampling functions
    bool should_continue_sampling(int x, int y, const std::vector<Color>& samples) const;
    float compute_variance(const std::vector<Color>& samples) const;

    // Wavefront rendering functions
    // When accumulate_linear_radiance is true, adds linear (pre-gamma) radiance per pixel into framebuffer
    // (typically samples=1 per frame); framebuffer must be zeroed or managed by the caller.
    void render_wavefront(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                         int width, int height, int samples, bool accumulate_linear_radiance = false);

    // Cache-friendly rendering functions
    void render_morton(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                       int width, int height, int samples);

    // Phase 3: SIMD packet tracing
    // When accumulate_linear_radiance is true, adds linear (pre-gamma) radiance per pixel (samples is usually 1).
    void render_simd_packets(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                            int width, int height, int samples, bool accumulate_linear_radiance = false);

    // Phase 3: BVH + view frustum
    void build_bvh(const Scene& scene);
    void release_bvh();
    void sync_frustum(const Camera& cam);
    bool trace_closest(const Ray& r, float t_min, float t_max, HitRecord& rec, const Scene& scene,
                       const Frustum::Frustum* view_frustum) const;
    // Primary-ray hit (applies view frustum when enable_frustum + sync_frustum were used).
    bool trace_primary(const Ray& r, float t_min, float t_max, HitRecord& rec, const Scene& scene) const {
        const Frustum::Frustum* vf = (enable_frustum && frustum_valid) ? &view_frustum : nullptr;
        return trace_closest(r, t_min, t_max, rec, scene, vf);
    }
};

#endif // RENDERER_H
