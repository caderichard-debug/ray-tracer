#ifndef CORNELL_BOX_H
#define CORNELL_BOX_H

#include "scene.h"
#include "material/material.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "light.h"
#include <memory>

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
    auto material_light = std::make_shared<Lambertian>(Color(15.0f, 15.0f, 15.0f));
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

    // Left wall (red)
    scene.add_object(std::make_shared<Sphere>(Point3(-20.0, 0, 0), 16.0, material_red));

    // Right wall (green)
    scene.add_object(std::make_shared<Sphere>(Point3(20.0, 0, 0), 16.0, material_green));

    // Objects in scene - positioned in 16x16x16 room
    // Center sphere (gold - reflective)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, 0), 0.5, material_gold));

    // Orbiting spheres (distributed in the room)
    scene.add_object(std::make_shared<Sphere>(Point3(-3.0, 1.0, 2.0), 0.8, material_metal_fuzz));
    scene.add_object(std::make_shared<Sphere>(Point3(3.0, 0.8, 2.0), 0.9, material_blue));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, 1.5), 0.6, material_red));
    scene.add_object(std::make_shared<Sphere>(Point3(-1.5, 0.3, 2.0), 0.5, material_yellow));

    // Glass sphere (demonstrates refraction) - clearly visible
    scene.add_object(std::make_shared<Sphere>(Point3(4.0, 1.5, 0.0), 0.8, material_glass));

    // Triangles - forming a pyramid (positioned in larger room)
    Point3 pyramid_top(0.0f, 2.5f, -4.0f);
    Point3 pyramid_base1(-1.5f, 0.5f, -5.0f);
    Point3 pyramid_base2(1.5f, 0.5f, -5.0f);
    Point3 pyramid_base3(0.0f, 0.5f, -3.0f);

    // 4 triangles forming a pyramid
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base1, pyramid_base2, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base2, pyramid_base3, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base3, pyramid_base1, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_base1, pyramid_base3, pyramid_base2, material_gray));

    // Small spheres in the back (positioned in larger room)
    scene.add_object(std::make_shared<Sphere>(Point3(-1.0, 0.5, -4.0), 0.4, material_metal));
    scene.add_object(std::make_shared<Sphere>(Point3(1.0, 0.5, -4.0), 0.4, material_metal));

    // Lighting
    scene.add_light(Light(Point3(0, 18.0, 0), Color(1.0f, 1.0f, 1.0f)));
}

#endif // CORNELL_BOX_H
