#ifndef GPU_DEMO_H
#define GPU_DEMO_H

#include "scene.h"
#include "primitives/sphere.h"
#include "material/material.h"
#include "texture/texture.h"
#include "math/vec3.h"

// GPU Demo Scene - Designed to showcase specific PBR improvements
// Each section demonstrates a particular feature of the new lighting system

inline void setup_gpu_demo_scene(Scene& scene) {
    // ========================================================================
    // ENVIRONMENT
    // ========================================================================

    // Ground (matte gray for neutral reflection)
    auto ground_mat = std::make_shared<Lambertian>(Color(0.4, 0.4, 0.45));
    scene.add_object(std::make_shared<Sphere>(Point3(0, -100.5, 0), 100, ground_mat));

    // Back wall (dark gradient for depth)
    auto back_wall_mat = std::make_shared<Lambertian>(Color(0.2, 0.22, 0.25));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -106), 100, back_wall_mat));

    // ========================================================================
    // ROUGHNESS COMPARISON (Bottom Row)
    // Demonstrates: PBR roughness parameter (0.0 = mirror, 1.0 = matte)
    // ========================================================================

    // Mirror finish - shows perfect reflections
    auto mirror_mat = std::make_shared<Metal>(Color(0.98, 0.98, 0.98), 0.0);
    scene.add_object(std::make_shared<Sphere>(Point3(-6, 0, -2), 0.5, mirror_mat));

    // Polished metal - very sharp reflections
    auto polished_mat = std::make_shared<Metal>(Color(0.92, 0.88, 0.75), 0.05);
    scene.add_object(std::make_shared<Sphere>(Point3(-4, 0, -2), 0.5, polished_mat));

    // Glossy - good balance of sharp and soft
    auto glossy_mat = std::make_shared<Metal>(Color(0.87, 0.78, 0.55), 0.2);
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 0, -2), 0.5, glossy_mat));

    // Semi-matte - diffuse with some specularity
    auto semimate_mat = std::make_shared<Metal>(Color(0.82, 0.68, 0.35), 0.35);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -2), 0.5, semimate_mat));

    // Matte - very soft reflections
    auto matte_metal_mat = std::make_shared<Metal>(Color(0.77, 0.58, 0.15), 0.5);
    scene.add_object(std::make_shared<Sphere>(Point3(2, 0, -2), 0.5, matte_metal_mat));

    // Rough - nearly diffuse
    auto rough_mat = std::make_shared<Metal>(Color(0.72, 0.48, 0.0), 0.65);
    scene.add_object(std::make_shared<Sphere>(Point3(4, 0, -2), 0.5, rough_mat));

    // ========================================================================
    // METALLIC SWEEP (Middle Row)
    // Demonstrates: Metallic parameter (0.0 = dielectric, 1.0 = metal)
    // ========================================================================

    // Plastic red (dielectric)
    auto red_plastic_mat = std::make_shared<Lambertian>(Color(0.85, 0.15, 0.15));
    scene.add_object(std::make_shared<Sphere>(Point3(-4, 2, -1), 0.5, red_plastic_mat));

    // Orange ceramic (dielectric with some shine)
    auto orange_mat = std::make_shared<Metal>(Color(0.9, 0.6, 0.2), 0.3);
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 2, -1), 0.5, orange_mat));

    // Gold (classic metal)
    auto gold_mat = std::make_shared<Metal>(Color(1.0, 0.82, 0.35), 0.15);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 2, -1), 0.5, gold_mat));

    // Copper (metal)
    auto copper_mat = std::make_shared<Metal>(Color(0.88, 0.55, 0.35), 0.2);
    scene.add_object(std::make_shared<Sphere>(Point3(2, 2, -1), 0.5, copper_mat));

    // Chrome (highly reflective metal)
    auto chrome_mat = std::make_shared<Metal>(Color(0.95, 0.95, 0.98), 0.05);
    scene.add_object(std::make_shared<Sphere>(Point3(4, 2, -1), 0.5, chrome_mat));

    // ========================================================================
    // HERO OBJECTS (Top Row)
    // Demonstrates: Real-world materials with PBR accuracy
    // ========================================================================

    // Pearl (complex layered material - approximated)
    auto pearl_mat = std::make_shared<Metal>(Color(1.0, 0.95, 0.9), 0.12);
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 4.5, 0), 0.7, pearl_mat));

    // Emerald (dielectric with color)
    auto emerald_mat = std::make_shared<Lambertian>(Color(0.15, 0.6, 0.25));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 4.5, 0), 0.7, emerald_mat));

    // Ruby (dielectric with red color)
    auto ruby_mat = std::make_shared<Lambertian>(Color(0.7, 0.15, 0.15));
    scene.add_object(std::make_shared<Sphere>(Point3(2, 4.5, 0), 0.7, ruby_mat));

    // ========================================================================
    // FRESNEL DEMONSTRATION
    // Demonstrates: Grazing angle reflections (Fresnel effect)
    // ========================================================================

    // Glass sphere (shows refraction and reflection)
    auto glass_mat = std::make_shared<Dielectric>(1.5);
    scene.add_object(std::make_shared<Sphere>(Point3(-5, 3.5, -3), 0.6, glass_mat));

    // Water-like material (lower IOR)
    auto water_mat = std::make_shared<Dielectric>(1.33);
    scene.add_object(std::make_shared<Sphere>(Point3(-3.5, 3.5, -3), 0.6, water_mat));

    // Diamond (high IOR for strong refraction)
    auto diamond_mat = std::make_shared<Dielectric>(2.42);
    scene.add_object(std::make_shared<Sphere>(Point3(-2, 3.5, -3), 0.6, diamond_mat));

    // ========================================================================
    // COMPARISON OBJECTS
    // Demonstrates: Phong vs PBR material quality
    // ========================================================================

    // Red sphere (shows Fresnel effect in PBR mode)
    auto demo_red_mat = std::make_shared<Lambertian>(Color(0.8, 0.1, 0.1));
    scene.add_object(std::make_shared<Sphere>(Point3(4, 3.5, -3), 0.6, demo_red_mat));

    // Blue sphere (shows energy conservation in PBR mode)
    auto demo_blue_mat = std::make_shared<Lambertian>(Color(0.1, 0.3, 0.8));
    scene.add_object(std::make_shared<Sphere>(Point3(5.5, 3.5, -3), 0.6, demo_blue_mat));

    // ========================================================================
    // LIGHTING SETUP
    // Demonstrates: Multiple light configurations
    // ========================================================================

    // Main light (overhead, slightly warm)
    scene.add_light(Light(
        Point3(0, 15, 0),
        Color(1.0, 0.98, 0.95) * 1.0
    ));

    // Fill light (cool, from the side)
    scene.add_light(Light(
        Point3(-12, 10, 8),
        Color(0.85, 0.9, 1.0) * 0.35
    ));

    // Rim light (warm, from behind)
    scene.add_light(Light(
        Point3(12, 8, -8),
        Color(1.0, 0.92, 0.85) * 0.4
    ));

    // Accent light (for hero objects)
    scene.add_light(Light(
        Point3(0, 12, -5),
        Color(1.0, 1.0, 1.0) * 0.25
    ));
}

#endif // GPU_DEMO_H
