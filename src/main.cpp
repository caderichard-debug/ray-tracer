#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image_write.h"

#include <iostream>
#include <vector>
#include <limits>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <omp.h>
#include "math/vec3.h"
#include "math/ray.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "primitives/primitive.h"
#include "material/material.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "scene/light.h"
#include "renderer/renderer.h"
#include "renderer/performance.h"

// Infinity constant for ray tracing
const float infinity = std::numeric_limits<float>::infinity();

// Fast XOR-shift random number generator with atomic counter
inline float random_float() {
    // Atomic counter for seed progression (thread-safe)
    static std::atomic<uint32_t> counter{0};
    uint32_t seed = counter.fetch_add(1, std::memory_order_relaxed);

    // XOR-shift algorithm (fast and good quality)
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    // Return float in [0, 1)
    return (seed & 0xFFFFFF) / 16777216.0f;
}

// Generate unique output filename with timestamp
std::string generate_output_filename(const std::string& prefix = "output", const std::string& ext = "png") {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << "renders/" << prefix << "_"
       << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S")
       << "_" << ms.count() << "." << ext;
    return ss.str();
}

int main(int argc, char* argv[]) {
    // Default settings
    const float aspect_ratio = 16.0f / 9.0f;
    int image_width = 800;
    int samples_per_pixel = 16; // Anti-aliasing (16 samples per pixel)
    int max_depth = 5; // Max reflection bounces
    std::string output_prefix = "cornell_box";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--width" || arg == "-w") {
            if (i + 1 < argc) {
                image_width = std::atoi(argv[++i]);
                if (image_width < 100) image_width = 100;
                if (image_width > 3840) image_width = 3840;
            }
        } else if (arg == "--samples" || arg == "-s") {
            if (i + 1 < argc) {
                samples_per_pixel = std::atoi(argv[++i]);
                if (samples_per_pixel < 1) samples_per_pixel = 1;
                if (samples_per_pixel > 256) samples_per_pixel = 256;
            }
        } else if (arg == "--depth" || arg == "-d") {
            if (i + 1 < argc) {
                max_depth = std::atoi(argv[++i]);
                if (max_depth < 1) max_depth = 1;
                if (max_depth > 10) max_depth = 10;
            }
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                output_prefix = argv[++i];
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -w, --width <pixels>     Image width (100-3840, default: 800)\n"
                      << "  -s, --samples <count>     Samples per pixel (1-256, default: 16)\n"
                      << "  -d, --depth <bounces>     Max reflection depth (1-10, default: 5)\n"
                      << "  -o, --output <prefix>     Output filename prefix (default: cornell_box)\n"
                      << "  -h, --help                Show this help message\n"
                      << "\nExample:\n"
                      << "  " << argv[0] << " -w 1920 -s 32 -d 8 -o my_scene\n";
            return 0;
        }
    }

    const int image_height = static_cast<int>(image_width / aspect_ratio);

    // Performance tracking
    PerformanceTracker perf("Phase 2++: Multi-threaded Renderer (AA + PNG + OpenMP)", image_width, image_height,
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

    // Objects in scene - diverse geometry and materials
    // Center sphere (gold - reflective)
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, 0), 0.5, material_gold));

    // Orbiting spheres
    scene.add_object(std::make_shared<Sphere>(Point3(-1.2, 0.2, -0.5), 0.35, material_metal_fuzz));
    scene.add_object(std::make_shared<Sphere>(Point3(1.2, -0.1, -0.8), 0.4, material_blue));
    scene.add_object(std::make_shared<Sphere>(Point3(0, -0.5, 0.5), 0.25, material_red));
    scene.add_object(std::make_shared<Sphere>(Point3(-0.6, -0.4, 0.8), 0.2, material_yellow));

    // Glass sphere (demonstrates refraction)
    scene.add_object(std::make_shared<Sphere>(Point3(0.7, 0.0, 0.3), 0.3, material_glass));

    // Triangles - forming a pyramid
    Point3 pyramid_top(0.0f, 0.9f, -1.8f);
    Point3 pyramid_base1(-0.6f, -0.3f, -2.3f);
    Point3 pyramid_base2(0.6f, -0.3f, -2.3f);
    Point3 pyramid_base3(0.0f, -0.3f, -1.3f);

    // 4 triangles forming a pyramid
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base1, pyramid_base2, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base2, pyramid_base3, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base3, pyramid_base1, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_base1, pyramid_base3, pyramid_base2, material_gray));

    // Small spheres in the back
    scene.add_object(std::make_shared<Sphere>(Point3(-0.3, -0.45, -1.8), 0.15, material_metal));
    scene.add_object(std::make_shared<Sphere>(Point3(0.3, -0.45, -1.8), 0.15, material_metal));

    // Lighting
    scene.add_light(Light(Point3(0, 4.9, 0), Color(1.0f, 1.0f, 1.0f)));

    // Create renders directory and generate unique output filename
    system("mkdir -p renders");

    // Generate unique output filenames with timestamp
    std::string png_filename = generate_output_filename(output_prefix, "png");
    std::string ppm_filename = generate_output_filename(output_prefix, "ppm");
    std::cerr << "Output PNG: " << png_filename << std::endl;
    std::cerr << "Output PPM: " << ppm_filename << std::endl;

    // Allocate framebuffer for PNG output (RGB, top-to-bottom)
    std::vector<unsigned char> framebuffer(image_width * image_height * 3);

    // Display OpenMP thread count
    #ifdef _OPENMP
    std::cerr << "OpenMP threads: " << omp_get_max_threads() << std::endl;
    #endif

    // Render pixels, top to bottom, left to right (parallelized with OpenMP)
    #pragma omp parallel for schedule(dynamic, 4)
    for (int j = image_height - 1; j >= 0; --j) {
        if (j % 10 == 0) {
            #pragma omp critical
            std::cerr << "\rScanlines remaining: " << j << ' ' << std::flush;
        }
        for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);

            // Sample pixels for anti-aliasing (16 samples)
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

            // Write to framebuffer (convert to 0-255 range)
            int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
            framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
            framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
            framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
        }
    }

    std::cerr << "\n";

    // Write PNG output using stb_image_write
    stbi_write_png(png_filename.c_str(), image_width, image_height, 3,
                   framebuffer.data(), image_width * 3);

    // Also write PPM for compatibility
    FILE* ppm_file = fopen(ppm_filename.c_str(), "w");
    if (ppm_file) {
        fprintf(ppm_file, "P6\n%d %d\n255\n", image_width, image_height);
        fwrite(framebuffer.data(), 1, framebuffer.size(), ppm_file);
        fclose(ppm_file);
    }

    std::cerr << "✓ Done. Images saved to:\n";
    std::cerr << "  PNG: " << png_filename << "\n";
    std::cerr << "  PPM: " << ppm_filename << "\n";

    // Calculate and print performance statistics
    perf.calculate_ray_count();
    perf.print_report();

    return 0;
}
