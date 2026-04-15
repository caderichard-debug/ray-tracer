#ifndef RENDERER_H
#define RENDERER_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../scene/scene.h"
#include "../camera/camera.h"
#include "../material/material.h"

class Renderer {
public:
    int max_depth; // Maximum recursion depth for reflections
    bool enable_shadows; // Enable/disable shadow rays
    bool enable_reflections; // Enable/disable reflection rays

    Renderer() : max_depth(5), enable_shadows(true), enable_reflections(true) {}
    Renderer(int depth) : max_depth(depth), enable_shadows(true), enable_reflections(true) {}

    // Main ray tracing function
    Color ray_color(const Ray& r, const Scene& scene, int depth) const;

    // Calculate Phong shading at a hit point
    Color compute_phong_shading(const HitRecord& rec, const Scene& scene) const;
};

#endif // RENDERER_H
