#ifndef PBR_SHOWCASE_H
#define PBR_SHOWCASE_H

#include "scene.h"
#include "material/material.h"
#include "texture/texture.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "light.h"
#include <memory>
#include <algorithm>

// PBR Showcase Scene - Demonstrates material quality differences
// This scene is designed to show off the PBR capabilities

inline void setup_pbr_showcase_scene(Scene& scene) {
    scene.ambient_light = Color(0.1f, 0.1f, 0.1f);

    // Ground plane (checkerboard texture - classic marble floor look)
    auto checker_floor = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.8f, 0.8f, 0.8f)),  // White marble
        std::make_shared<SolidColor>(Color(0.2f, 0.2f, 0.2f)),  // Dark marble
        8.0f  // Checker size
    );
    auto ground_mat = std::make_shared<Lambertian>(checker_floor);
    scene.add_object(std::make_shared<Sphere>(Point3(0, -100.5, 0), 100, ground_mat));

    // Background wall (gradient texture - sunset effect)
    auto gradient_wall = std::make_shared<GradientTexture>(
        Color(0.1f, 0.1f, 0.3f),  // Deep blue at top
        Color(0.8f, 0.5f, 0.3f),  // Orange at bottom
        Vec3(0, 1, 0),            // Vertical gradient
        0.1f                      // Fine scale
    );
    auto wall_mat = std::make_shared<Lambertian>(gradient_wall);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -105), 100, wall_mat));

    // === ROUGHNESS SWEEP (Left to Right) ===
    // Shows roughness from 0.0 (mirror) to 1.0 (matte)

    // Mirror (roughness = 0.0, metallic = 1.0)
    auto mirror_mat = std::make_shared<Metal>(Color(0.95f, 0.95f, 0.95f), 0.0);
    scene.add_object(std::make_shared<Sphere>(Point3(-4.5, 0.5, 0), 0.5, mirror_mat));

    // Polished metal (roughness = 0.1, metallic = 1.0)
    auto polished_metal_mat = std::make_shared<Metal>(Color(0.9f, 0.9f, 0.9f), 0.05);
    scene.add_object(std::make_shared<Sphere>(Point3(-3.5, 0.5, 0), 0.5, polished_metal_mat));

    // Glossy metal (roughness = 0.3, metallic = 1.0)
    auto glossy_metal_mat = std::make_shared<Metal>(Color(0.85f, 0.85f, 0.85f), 0.15);
    scene.add_object(std::make_shared<Sphere>(Point3(-2.5, 0.5, 0), 0.5, glossy_metal_mat));

    // Medium metal (roughness = 0.5, metallic = 1.0)
    auto medium_metal_mat = std::make_shared<Metal>(Color(0.8f, 0.8f, 0.8f), 0.25);
    scene.add_object(std::make_shared<Sphere>(Point3(-1.5, 0.5, 0), 0.5, medium_metal_mat));

    // Rough metal (roughness = 0.7, metallic = 1.0)
    auto rough_metal_mat = std::make_shared<Metal>(Color(0.75f, 0.75f, 0.75f), 0.35);
    scene.add_object(std::make_shared<Sphere>(Point3(-0.5, 0.5, 0), 0.5, rough_metal_mat));

    // Matte metal (roughness = 0.9, metallic = 1.0)
    auto matte_metal_mat = std::make_shared<Metal>(Color(0.7f, 0.7f, 0.7f), 0.5);
    scene.add_object(std::make_shared<Sphere>(Point3(0.5, 0.5, 0), 0.5, matte_metal_mat));

    // === METALLIC SWEEP (Left to Right) ===
    // Shows metallic from 0.0 (dielectric) to 1.0 (metal)

    // Plastic with checkerboard texture
    auto checker_red_yellow = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.8f, 0.2f, 0.2f)),  // Red
        std::make_shared<SolidColor>(Color(0.8f, 0.8f, 0.2f)),  // Yellow
        4.0f  // Fine checker pattern
    );
    auto plastic_mat = std::make_shared<Lambertian>(checker_red_yellow);
    scene.add_object(std::make_shared<Sphere>(Point3(2.0, 0.5, 2), 0.5, plastic_mat));

    // Rusty metal (roughness = 0.6, metallic = 0.5)
    // Approximated with fuzzy metal
    auto rusty_mat = std::make_shared<Metal>(Color(0.7f, 0.5f, 0.3f), 0.3);
    scene.add_object(std::make_shared<Sphere>(Point3(3.0, 0.5, 2), 0.5, rusty_mat));

    // Pure metal with gradient (silver to gold)
    auto gradient_gold = std::make_shared<GradientTexture>(
        Color(0.95f, 0.9f, 0.7f),  // Silver
        Color(1.0f, 0.85f, 0.3f),  // Gold
        Vec3(1, 0, 0),             // Horizontal gradient
        0.15f                     // Scale
    );
    auto pure_metal_mat = std::make_shared<Lambertian>(gradient_gold);  // Lambertian for texture support
    scene.add_object(std::make_shared<Sphere>(Point3(4.0, 0.5, 2), 0.5, pure_metal_mat));

    // === DEMONSTRATION SPHERES (Center Stage) ===

    // Gold sphere (classic metal test) - BIG center piece
    auto gold_mat = std::make_shared<Metal>(Color(1.0f, 0.85f, 0.3f), 0.1);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 1.5, -1), 0.7, gold_mat));

    // Silver sphere with noise texture (hammered metal look)
    auto noise_silver = std::make_shared<NoiseTexture>(
        Color(0.95f, 0.95f, 0.95f),  // Base silver
        Color(0.7f, 0.7f, 0.7f),     // Darker silver
        8.0f                         // Fine noise scale
    );
    auto silver_mat = std::make_shared<Lambertian>(noise_silver);  // Lambertian for texture support
    scene.add_object(std::make_shared<Sphere>(Point3(1.5, 1.2, -1), 0.6, silver_mat));

    // Copper sphere with stripe texture (looks like metallic bands)
    auto stripe_copper = std::make_shared<StripeTexture>(
        Color(0.9f, 0.6f, 0.4f),     // Copper
        Color(0.7f, 0.4f, 0.2f),     // Dark copper
        3.0f                         // Stripe width
    );
    auto copper_mat = std::make_shared<Lambertian>(stripe_copper);  // Lambertian for texture support
    scene.add_object(std::make_shared<Sphere>(Point3(-1.5, 1.2, -1), 0.6, copper_mat));

    // Glass sphere (dielectric test) - clear glass
    auto glass_mat = std::make_shared<Dielectric>(1.5f);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0.5, -3), 0.5, glass_mat));

    // === TEXTURE SHOWCASE SPHERES (Background Row) ===

    // Red/Blue checkerboard sphere
    auto checker_red_blue = std::make_shared<CheckerTexture>(
        std::make_shared<SolidColor>(Color(0.8f, 0.1f, 0.1f)),  // Red
        std::make_shared<SolidColor>(Color(0.1f, 0.1f, 0.8f)),  // Blue
        6.0f  // Medium checker pattern
    );
    auto red_plastic_mat = std::make_shared<Lambertian>(checker_red_blue);
    scene.add_object(std::make_shared<Sphere>(Point3(3, 0.5, -3), 0.5, red_plastic_mat));

    // Noise marble sphere (white with gray veins)
    auto noise_marble = std::make_shared<NoiseTexture>(
        Color(1.0f, 1.0f, 1.0f),     // Pure white
        Color(0.6f, 0.6f, 0.6f),     // Gray
        12.0f                        // Fine marble veins
    );
    auto blue_plastic_mat = std::make_shared<Lambertian>(noise_marble);
    scene.add_object(std::make_shared<Sphere>(Point3(-3, 0.5, -3), 0.5, blue_plastic_mat));

    // === ADDITIONAL COOL TEXTURES ===

    // Rainbow gradient sphere (diagonal gradient)
    auto gradient_rainbow = std::make_shared<GradientTexture>(
        Color(1.0f, 0.0f, 0.0f),     // Red
        Color(0.0f, 0.0f, 1.0f),     // Blue
        Vec3(1, 1, 0).normalized(),  // Diagonal gradient
        0.15f                       // Scale
    );
    auto rainbow_mat = std::make_shared<Lambertian>(gradient_rainbow);
    scene.add_object(std::make_shared<Sphere>(Point3(-5, 0.8, -2), 0.4, rainbow_mat));

    // Stripe texture sphere (zebra pattern)
    auto stripe_zebra = std::make_shared<StripeTexture>(
        Color(0.1f, 0.1f, 0.1f),     // Black
        Color(1.0f, 1.0f, 1.0f),     // White
        4.0f                         // Narrow stripes
    );
    auto zebra_mat = std::make_shared<Lambertian>(stripe_zebra);
    scene.add_object(std::make_shared<Sphere>(Point3(5, 0.8, -2), 0.4, zebra_mat));

    // Earth-like texture sphere (noise simulating terrain)
    auto noise_terrain = std::make_shared<NoiseTexture>(
        Color(0.2f, 0.5f, 0.2f),     // Green (land)
        Color(0.1f, 0.3f, 0.8f),     // Blue (water)
        6.0f                         // Continent scale
    );
    auto earth_mat = std::make_shared<Lambertian>(noise_terrain);
    scene.add_object(std::make_shared<Sphere>(Point3(0, 2.5, -4), 0.8, earth_mat));

    // Fire texture sphere (noise with orange/red)
    auto noise_fire = std::make_shared<NoiseTexture>(
        Color(1.0f, 0.8f, 0.0f),     // Yellow
        Color(1.0f, 0.2f, 0.0f),     // Red
        10.0f                        // Fine fire scale
    );
    auto fire_mat = std::make_shared<Lambertian>(noise_fire);
    scene.add_object(std::make_shared<Sphere>(Point3(-6, 1.0, 0), 0.3, fire_mat));

    // Wood texture sphere (noise with brown tones)
    auto noise_wood = std::make_shared<NoiseTexture>(
        Color(0.6f, 0.4f, 0.2f),     // Light brown
        Color(0.3f, 0.2f, 0.1f),     // Dark brown
        15.0f                        // Fine wood grain
    );
    auto wood_mat = std::make_shared<Lambertian>(noise_wood);
    scene.add_object(std::make_shared<Sphere>(Point3(6, 1.0, 0), 0.3, wood_mat));

    // Add a main light source
    scene.add_light(Light(Point3(0, 18.0, 0), Color(1.0f, 1.0f, 1.0f)));

    // Optional: Add fill light
    scene.add_light(Light(Point3(-10, 10, 10), Color(0.8f, 0.9f, 1.0f)));

    // Optional: Add rim light
    scene.add_light(Light(Point3(15, 5, -5), Color(1.0f, 0.9f, 0.8f)));
}

#endif // PBR_SHOWCASE_H
