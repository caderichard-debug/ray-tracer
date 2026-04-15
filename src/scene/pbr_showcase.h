#ifndef PBR_SHOWCASE_H
#define PBR_SHOWCASE_H

#include "scene.h"
#include "primitives/sphere.h"
#include "material/material.h"
#include "texture/texture.h"
#include "math/vec3.h"

// PBR Showcase Scene - Demonstrates material quality differences
// This scene is designed to show off the Cook-Torrance BRDF capabilities

inline void setup_pbr_showcase_scene(Scene& scene) {
    // Ground plane (matte gray)
    auto ground_mat = std::make_shared<Lambertian>(Color3(0.5, 0.5, 0.5));
    ground_mat->name = "Ground";
    scene.add(std::make_shared<Sphere>(Point3(0, -100.5, 0), 100, ground_mat));

    // Background wall (matte white)
    auto wall_mat = std::make_shared<Lambertian>(Color3(0.8, 0.8, 0.8));
    wall_mat->name = "Wall";
    scene.add(std::make_shared<Sphere>(Point3(0, 0, -105), 100, wall_mat));

    // === ROUGHNESS SWEEP (Left to Right) ===
    // Shows roughness from 0.0 (mirror) to 1.0 (matte)

    // Mirror (roughness = 0.0, metallic = 1.0)
    auto mirror_mat = std::make_shared<Metal>(Color3(0.95, 0.95, 0.95), 0.0);
    mirror_mat->name = "Mirror_R0.0";
    scene.add(std::make_shared<Sphere>(Point3(-4.5, 0, 0), 0.5, mirror_mat));

    // Polished metal (roughness = 0.1, metallic = 1.0)
    auto polished_metal_mat = std::make_shared<Metal>(Color3(0.9, 0.9, 0.9), 0.05);
    polished_metal_mat->name = "Polished_R0.1";
    scene.add(std::make_shared<Sphere>(Point3(-3.5, 0, 0), 0.5, polished_metal_mat));

    // Glossy metal (roughness = 0.3, metallic = 1.0)
    auto glossy_metal_mat = std::make_shared<Metal>(Color3(0.85, 0.85, 0.85), 0.15);
    glossy_metal_mat->name = "Glossy_R0.3";
    scene.add(std::make_shared<Sphere>(Point3(-2.5, 0, 0), 0.5, glossy_metal_mat));

    // Medium metal (roughness = 0.5, metallic = 1.0)
    auto medium_metal_mat = std::make_shared<Metal>(Color3(0.8, 0.8, 0.8), 0.25);
    medium_metal_mat->name = "Medium_R0.5";
    scene.add(std::make_shared<Sphere>(Point3(-1.5, 0, 0), 0.5, medium_metal_mat));

    // Rough metal (roughness = 0.7, metallic = 1.0)
    auto rough_metal_mat = std::make_shared<Metal>(Color3(0.75, 0.75, 0.75), 0.35);
    rough_metal_mat->name = "Rough_R0.7";
    scene.add(std::make_shared<Sphere>(Point3(-0.5, 0, 0), 0.5, rough_metal_mat));

    // Matte metal (roughness = 0.9, metallic = 1.0)
    auto matte_metal_mat = std::make_shared<Metal>(Color3(0.7, 0.7, 0.7), 0.5);
    matte_metal_mat->name = "Matte_R0.9";
    scene.add(std::make_shared<Sphere>(Point3(0.5, 0, 0), 0.5, matte_metal_mat));

    // === METALLIC SWEEP (Left to Right) ===
    // Shows metallic from 0.0 (dielectric) to 1.0 (metal)

    // Plastic (roughness = 0.3, metallic = 0.0)
    auto plastic_mat = std::make_shared<Lambertian>(Color3(0.8, 0.2, 0.2));
    plastic_mat->name = "Plastic_M0.0";
    scene.add(std::make_shared<Sphere>(Point3(2.0, 0, 2), 0.5, plastic_mat));

    // Rusty metal (roughness = 0.6, metallic = 0.5)
    // Approximated with fuzzy metal
    auto rusty_mat = std::make_shared<Metal>(Color3(0.7, 0.5, 0.3), 0.3);
    rusty_mat->name = "Rusty_M0.5";
    scene.add(std::make_shared<Sphere>(Point3(3.0, 0, 2), 0.5, rusty_mat));

    // Pure metal (roughness = 0.2, metallic = 1.0)
    auto pure_metal_mat = std::make_shared<Metal>(Color3(0.95, 0.9, 0.7), 0.1);
    pure_metal_mat->name = "Pure_M1.0";
    scene.add(std::make_shared<Sphere>(Point3(4.0, 0, 2), 0.5, pure_metal_mat));

    // === DEMONSTRATION SPHERES ===

    // Gold sphere (classic metal test)
    auto gold_mat = std::make_shared<Metal>(Color3(1.0, 0.85, 0.3), 0.1);
    gold_mat->name = "Gold";
    scene.add(std::make_shared<Sphere>(Point3(0, 1.5, -1), 0.7, gold_mat));

    // Silver sphere
    auto silver_mat = std::make_shared<Metal>(Color3(0.95, 0.95, 0.95), 0.05);
    silver_mat->name = "Silver";
    scene.add(std::make_shared<Sphere>(Point3(1.5, 1.2, -1), 0.6, silver_mat));

    // Copper sphere
    auto copper_mat = std::make_shared<Metal>(Color3(0.9, 0.6, 0.4), 0.15);
    copper_mat->name = "Copper";
    scene.add(std::make_shared<Sphere>(Point3(-1.5, 1.2, -1), 0.6, copper_mat));

    // Glass sphere (dielectric test)
    auto glass_mat = std::make_shared<Dielectric>(1.5);
    glass_mat->name = "Glass";
    scene.add(std::make_shared<Sphere>(Point3(0, 0.5, -3), 0.5, glass_mat));

    // Red plastic sphere (dielectric)
    auto red_plastic_mat = std::make_shared<Lambertian>(Color3(0.8, 0.1, 0.1));
    red_plastic_mat->name = "Red_Plastic";
    scene.add(std::make_shared<Sphere>(Point3(3, 0.5, -3), 0.5, red_plastic_mat));

    // Blue plastic sphere (dielectric)
    auto blue_plastic_mat = std::make_shared<Lambertian>(Color3(0.1, 0.3, 0.8));
    blue_plastic_mat->name = "Blue_Plastic";
    scene.add(std::make_shared<Sphere>(Point3(-3, 0.5, -3), 0.5, blue_plastic_mat));

    // Add a light source
    scene.add_light(std::make_shared<PointLight>(Point3(0, 15, 0), Color3(1, 1, 1), 1.0));

    // Optional: Add fill light
    scene.add_light(std::make_shared<PointLight>(Point3(-10, 10, 10), Color3(0.8, 0.9, 1.0), 0.3));

    // Optional: Add rim light
    scene.add_light(std::make_shared<PointLight>(Point3(15, 5, -5), Color3(1.0, 0.9, 0.8), 0.5));
}

#endif // PBR_SHOWCASE_H
