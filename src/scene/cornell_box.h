#ifndef CORNELL_BOX_H
#define CORNELL_BOX_H

#include "scene.h"
#include "material/material.h"
#include "texture/texture.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "light.h"
#include <memory>
#include <algorithm>

// Shared Cornell Box scene setup
// Used by both batch renderer and interactive viewer
inline void setup_cornell_box_scene(Scene& scene) {
    scene.ambient_light = Color(0.1f, 0.1f, 0.1f);

    // Materials - diverse palette
    auto material_red = std::make_shared<Lambertian>(Color(0.65f, 0.05f, 0.05f));
    auto material_green = std::make_shared<Lambertian>(Color(0.12f, 0.45f, 0.15f));
    auto material_gray = std::make_shared<Lambertian>(Color(0.73f, 0.73f, 0.73f));
    auto material_blue = std::make_shared<Lambertian>(Color(0.1f, 0.2f, 0.7f));
    auto material_yellow = std::make_shared<Lambertian>(Color(0.8f, 0.7f, 0.1f));
    auto material_metal = std::make_shared<Metal>(Color(0.8f, 0.8f, 0.8f), 0.0); // Perfect mirror
    auto material_metal_fuzz = std::make_shared<Metal>(Color(0.7f, 0.6f, 0.5f), 0.3); // Fuzzy metal
    auto material_gold = std::make_shared<Metal>(Color(1.0f, 0.77f, 0.35f), 0.1); // Gold-like
    auto material_glass = std::make_shared<Dielectric>(1.5f); // Glass (IOR 1.5)

    // Cornell box walls (using large spheres as approximation)
    // Walls at distance ±20 with radius 12 creates a ~16x16x16 room
    // Back wall (green)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -20.0), 16.0, material_green));

    // Floor (gray)
    scene.add_object(std::make_shared<Sphere>(Point3(0, -20.0, 0), 16.0, material_gray));

    // Ceiling (gray)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 20.0, 0), 16.0, material_gray));

    // Emissive ceiling lamp (area light for GPU path tracing)
    auto ceiling_emit = std::make_shared<Emissive>(Color(3.5f, 3.5f, 3.2f));
    scene.add_object(std::make_shared<Sphere>(Point3(0.0f, 16.2f, 0.0f), 1.4f, ceiling_emit));

    // Left wall (red)
    scene.add_object(std::make_shared<Sphere>(Point3(-20.0, 0, 0), 16.0, material_red));

    // Right wall (green)
    scene.add_object(std::make_shared<Sphere>(Point3(20.0, 0, 0), 16.0, material_green));

    // Objects in scene - positioned in 16x16x16 room
    // Center sphere (gold - reflective, BIG)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, 0), 2.0, material_gold));

    // Orbiting spheres (distributed in the room)
    scene.add_object(std::make_shared<Sphere>(Point3(-3.0, 1.0, 2.0), 0.8, material_metal_fuzz));
    scene.add_object(std::make_shared<Sphere>(Point3(3.0, 0.8, 2.0), 0.9, material_blue));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 1.5), 0.6, material_red));
    scene.add_object(std::make_shared<Sphere>(Point3(-1.5, 0.3, 2.0), 0.5, material_yellow));

    // Glass sphere (demonstrates refraction) - near center, below golden ball, toward player
    scene.add_object(std::make_shared<Sphere>(Point3(1.0, -1.5, 2.5), 0.8, material_glass));
    // Sphere behind glass
    scene.add_object(std::make_shared<Sphere>(Point3(0.5, -2.0, 1.5), 0.5, material_red));

    // Triangles - forming a pyramid (moved closer to camera)
    Point3 pyramid_top(-2.0f, 4.0f, 0.0f);  // Much closer to camera
    Point3 pyramid_base1(-1.0f, 2.0f, -1.0f);
    Point3 pyramid_base2(-3.0f, 2.0f, -1.0f);
    Point3 pyramid_base3(-2.0f, 2.0f, 1.0f);

    // Give pyramid a checkerboard texture (black and white)
    auto checker_pyramid = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.1f, 0.1f, 0.1f)),  // Black
        std::make_shared<SolidColor>(Color(0.9f, 0.9f, 0.9f)),  // White
        6.0f  // Scale (checker size)
    );
    auto material_pyramid = std::make_shared<Lambertian>(checker_pyramid);

    // 4 triangles forming a pyramid
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base1, pyramid_base2, material_pyramid));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base2, pyramid_base3, material_pyramid));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base3, pyramid_base1, material_pyramid));
    scene.add_object(std::make_shared<Triangle>(pyramid_base1, pyramid_base3, pyramid_base2, material_pyramid));

    // Small spheres in the back (moved closer to camera near pyramid)
    scene.add_object(std::make_shared<Sphere>(Point3(-0.5f, 2.5f, 0.0f), 0.2, material_metal));
    scene.add_object(std::make_shared<Sphere>(Point3(-3.5f, 2.8f, 0.0f), 0.2, material_metal));

    // === PROCEDURAL TEXTURES ===

    // Checkerboard texture sphere (alternating red and blue)
    auto checker_red_blue = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.8f, 0.2f, 0.2f)),  // Red
        std::make_shared<SolidColor>(Color(0.2f, 0.2f, 0.8f)),  // Blue
        8.0f  // Scale
    );
    auto material_checker = std::make_shared<Lambertian>(checker_red_blue);
    scene.add_object(std::make_shared<Sphere>(Point3(-3.0, 1.0, -2.0), 0.8, material_checker));

    // Noise texture sphere (high contrast marble)
    auto noise_marble = std::make_shared<NoiseTexture>(
        Color(1.0f, 1.0f, 1.0f),     // Pure white
        Color(0.0f, 0.0f, 0.0f),     // Pure black
        5.0f,  // Scale (even more detailed)
        6,     // Octaves (more detail layers)
        0.7f   // Persistence (higher contrast)
    );
    auto material_noise = std::make_shared<Lambertian>(noise_marble);
    scene.add_object(std::make_shared<Sphere>(Point3(-3.5, -2.0, 2.0), 0.8, material_noise));

    // Gradient texture sphere (purple to yellow vertical gradient) - auto-calibrated
    // Sphere at (-1.0, -1.5, 2.0) with radius 0.8
    Point3 sphere_center(-1.0, -1.5, 2.0);
    float sphere_radius = 0.8;
    Point3 sphere_bounds_min(sphere_center.x - sphere_radius, sphere_center.y - sphere_radius, sphere_center.z - sphere_radius);
    Point3 sphere_bounds_max(sphere_center.x + sphere_radius, sphere_center.y + sphere_radius, sphere_center.z + sphere_radius);

    auto gradient_purple_yellow = std::make_shared<GradientTexture>(
        Color(0.6f, 0.2f, 0.8f),  // Purple
        Color(0.9f, 0.9f, 0.2f),  // Yellow
        Vec3(0, 1, 0),            // Vertical direction
        sphere_bounds_min,        // Auto-calibrate from sphere bounds
        sphere_bounds_max
    );
    auto material_gradient = std::make_shared<Lambertian>(gradient_purple_yellow);
    scene.add_object(std::make_shared<Sphere>(sphere_center, sphere_radius, material_gradient));

    // Stripe texture sphere (orange and white horizontal stripes)
    auto stripe_orange_white = std::make_shared<StripeTexture>(
        Color(0.8f, 0.5f, 0.2f),  // Orange
        Color(0.9f, 0.9f, 0.9f),  // White
        8.0f,  // Scale (more stripes, closer together)
        0.0f   // Angle
    );
    auto material_stripe = std::make_shared<Lambertian>(stripe_orange_white);
    scene.add_object(std::make_shared<Sphere>(Point3(0.5, 1.5, 2.0), 0.8, material_stripe));

    // Gradient quad (on right wall) - horizontal gradient from red to blue - auto-calibrated
    Point3 quad_top_left(4.0f, 3.0f, 1.0f);
    Point3 quad_top_right(4.0f, 3.0f, 5.0f);
    Point3 quad_bottom_left(4.0f, -1.0f, 1.0f);
    Point3 quad_bottom_right(4.0f, -1.0f, 5.0f);

    // Calculate quad bounding box
    Point3 quad_bounds_min(
        std::min({quad_top_left.x, quad_top_right.x, quad_bottom_left.x, quad_bottom_right.x}),
        std::min({quad_top_left.y, quad_top_right.y, quad_bottom_left.y, quad_bottom_right.y}),
        std::min({quad_top_left.z, quad_top_right.z, quad_bottom_left.z, quad_bottom_right.z})
    );
    Point3 quad_bounds_max(
        std::max({quad_top_left.x, quad_top_right.x, quad_bottom_left.x, quad_bottom_right.x}),
        std::max({quad_top_left.y, quad_top_right.y, quad_bottom_left.y, quad_bottom_right.y}),
        std::max({quad_top_left.z, quad_top_right.z, quad_bottom_left.z, quad_bottom_right.z})
    );

    auto gradient_red_blue = std::make_shared<GradientTexture>(
        Color(1.0f, 0.0f, 0.0f),  // Pure red
        Color(0.0f, 0.0f, 1.0f),  // Pure blue
        Vec3(0, 0, 1),            // Horizontal gradient (along Z axis)
        quad_bounds_min,          // Auto-calibrate from quad bounds
        quad_bounds_max
    );
    auto material_quad_gradient = std::make_shared<Lambertian>(gradient_red_blue);

    // Quad made of 2 triangles on the right side (vertices declared above for bounds calculation)
    scene.add_object(std::make_shared<Triangle>(quad_top_left, quad_top_right, quad_bottom_right, material_quad_gradient));
    scene.add_object(std::make_shared<Triangle>(quad_top_left, quad_bottom_right, quad_bottom_left, material_quad_gradient));

    // Lighting
    scene.add_light(Light(Point3(0, 18.0, 0), Color(1.0f, 1.0f, 1.0f)));
}

#endif // CORNELL_BOX_H
