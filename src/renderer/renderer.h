#ifndef RENDERER_H
#define RENDERER_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../scene/scene.h"
#include "../camera/camera.h"
#include "../material/material.h"
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

    Renderer() : max_depth(5), enable_shadows(true), enable_reflections(true),
                 enable_progressive(false), current_pass(0), max_passes(10),
                 enable_adaptive(false), variance_threshold(0.01f), min_samples(4), max_samples(64),
                 enable_wavefront(false), wavefront_size(1024), enable_morton(false) {}

    Renderer(int depth) : max_depth(depth), enable_shadows(true), enable_reflections(true),
                          enable_progressive(false), current_pass(0), max_passes(10),
                          enable_adaptive(false), variance_threshold(0.01f), min_samples(4), max_samples(64),
                          enable_wavefront(false), wavefront_size(1024), enable_morton(false) {}

    // Main ray tracing function
    Color ray_color(const Ray& r, const Scene& scene, int depth) const;

    // Calculate Phong shading at a hit point
    Color compute_phong_shading(const HitRecord& rec, const Scene& scene) const;

    // Progressive rendering functions
    void initialize_progressive(int width, int height);
    void reset_progressive();
    bool is_progressive_complete() const;
    Color get_progressive_color(int x, int y) const;

    // Adaptive sampling functions
    bool should_continue_sampling(int x, int y, const std::vector<Color>& samples) const;
    float compute_variance(const std::vector<Color>& samples) const;

    // Wavefront rendering functions
    void render_wavefront(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                         int width, int height, int samples);

    // Cache-friendly rendering functions
    void render_morton(const Camera& cam, const Scene& scene, std::vector<std::vector<Color>>& framebuffer,
                       int width, int height, int samples);
};

#endif // RENDERER_H
