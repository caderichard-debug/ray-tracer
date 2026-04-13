#include <iostream>
#include <vector>
#include <limits>
#include <memory>
#include "math/vec3.h"
#include "math/ray.h"
#include "primitives/sphere.h"
#include "primitives/primitive.h"
#include "material/material.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "scene/light.h"
#include "renderer/renderer.h"
#include "renderer/performance.h"

// Infinity constant for ray tracing
const float infinity = std::numeric_limits<float>::infinity();

// Helper function for random float in [0, 1)
inline float random_float() {
    return static_cast<float>(rand()) / (RAND_MAX + 1.0f);
}

int main() {
    // Image dimensions
    const float aspect_ratio = 16.0f / 9.0f;
    const int image_width = 800;
    const int image_height = static_cast<int>(image_width / aspect_ratio);
    const int samples_per_pixel = 1; // No anti-aliasing yet
    const int max_depth = 5; // Max reflection bounces

    // Performance tracking
    PerformanceTracker perf("Phase 2: Scalar Ray Tracer", image_width, image_height,
                           samples_per_pixel, max_depth);

    // Camera setup
    Point3 lookfrom(0, 1, 3);
    Point3 lookat(0, 0, -1);
    Vec3 vup(0, 1, 0);
    float vfov = 60;
    float dist_to_focus = (lookfrom - lookat).length();
    float aperture = 0.0; // Pinhole camera for now
    Camera cam(lookfrom, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);

    // Renderer setup
    Renderer renderer(max_depth);

    // Scene setup - Cornell box style
    Scene scene;
    scene.ambient_light = Color(0.1f, 0.1f, 0.1f);

    // Materials
    auto material_red = std::make_shared<Lambertian>(Color(0.65f, 0.05f, 0.05f));
    auto material_green = std::make_shared<Lambertian>(Color(0.12f, 0.45f, 0.15f));
    auto material_gray = std::make_shared<Lambertian>(Color(0.73f, 0.73f, 0.73f));
    auto material_light = std::make_shared<Lambertian>(Color(15.0f, 15.0f, 15.0f));
    auto material_metal = std::make_shared<Metal>(Color(0.8f, 0.8f, 0.8f), 0.0); // Perfect mirror

    // Cornell box walls (using large spheres as approximation)
    // Back wall (green)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -5.5), 5.0, material_green));

    // Floor (gray)
    scene.add_object(std::make_shared<Sphere>(Point3(0, -5.5, 0), 5.0, material_gray));

    // Ceiling (gray)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 5.5, 0), 5.0, material_gray));

    // Left wall (red)
    scene.add_object(std::make_shared<Sphere>(Point3(-5.5, 0, 0), 5.0, material_red));

    // Right wall (green)
    scene.add_object(std::make_shared<Sphere>(Point3(5.5, 0, 0), 5.0, material_green));

    // Objects in scene
    // Center sphere (metal - reflective)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, 0), 0.5, material_metal));

    // Smaller sphere (red)
    scene.add_object(std::make_shared<Sphere>(Point3(-0.8, -0.3, -0.5), 0.3, material_red));

    // Smaller sphere (green)
    scene.add_object(std::make_shared<Sphere>(Point3(0.8, -0.3, -0.5), 0.3, material_green));

    // Lighting
    scene.add_light(Light(Point3(0, 4.9, 0), Color(1.0f, 1.0f, 1.0f)));

    // Render header (PPM format for simplicity)
    std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

    // Render pixels, top to bottom, left to right
    for (int j = image_height - 1; j >= 0; --j) {
        std::cerr << "\rScanlines remaining: " << j << ' ' << std::flush;
        for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);

            // Sample pixels for anti-aliasing (currently 1 sample = no AA)
            for (int s = 0; s < samples_per_pixel; ++s) {
                float u = (i + random_float()) / (image_width - 1);
                float v = (j + random_float()) / (image_height - 1);

                Ray r = cam.get_ray(u, v);
                pixel_color = pixel_color + renderer.ray_color(r, scene, renderer.max_depth);
            }

            // Average the samples
            float scale = 1.0f / samples_per_pixel;
            pixel_color = pixel_color * scale;

            // Gamma correction (gamma 2)
            pixel_color.x = std::sqrt(pixel_color.x);
            pixel_color.y = std::sqrt(pixel_color.y);
            pixel_color.z = std::sqrt(pixel_color.z);

            // Write color to output (clamp to [0, 1])
            std::cout << static_cast<int>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f)) << ' '
                      << static_cast<int>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f)) << ' '
                      << static_cast<int>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f)) << '\n';
        }
    }

    std::cerr << "\nDone.\n";

    // Calculate and print performance statistics
    perf.calculate_ray_count();
    perf.print_report();

    return 0;
}
