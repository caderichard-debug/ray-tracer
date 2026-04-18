// Headless benchmark: scalar vs SIMD packets vs wavefront (Cornell box).
// Build: make bench-cpu-modes
// Run:  ./build/bench_cpu_render_modes [-w 800] [-s 4] [-r 3] [--warmup 1]

#include "math/pcg_random.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "scene/cornell_box.h"
#include "renderer/renderer.h"
#include "renderer/render_analysis.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

using clock_hr = std::chrono::high_resolution_clock;

static void render_scalar_path(const Camera& cam, Scene& scene, Renderer& ray_renderer,
                               RenderAnalysis& analysis, int image_width, int image_height,
                               int samples, bool enable_shadows, std::vector<unsigned char>& framebuffer) {
    (void)ray_renderer.enable_adaptive;
    ray_renderer.sync_frustum(cam);
    #pragma omp parallel for schedule(dynamic, 16)
    for (int j = image_height - 1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);
            float total_depth = 0.0f;
            Color pixel_normal(0, 0, 0);
            Color pixel_albedo(0, 0, 0);
            int shadow_rays = 0;
            int total_rays = 0;

            const int actual_samples = samples;

            for (int s = 0; s < actual_samples; ++s) {
                float u = (i + random_float_pcg()) / (image_width - 1);
                float v = (j + random_float_pcg()) / (image_height - 1);

                Ray r = cam.get_ray(u, v);
                Color sample_color = ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                pixel_color = pixel_color + sample_color;
                total_rays += ray_renderer.max_depth;

                HitRecord rec;
                if (scene.hit(r, 0.001f, 1000.0f, rec)) {
                    total_depth += rec.t;
                    pixel_normal = pixel_normal + Color(rec.normal.x, rec.normal.y, rec.normal.z);
                    if (rec.mat) {
                        pixel_albedo = pixel_albedo + rec.mat->albedo;
                    }

                    if (enable_shadows) {
                        for (const auto& light : scene.lights) {
                            Vec3 light_dir = light.position - rec.p;
                            Ray shadow_ray(rec.p, light_dir.normalized());
                            if (!scene.is_shadowed(shadow_ray, light_dir.length())) {
                                shadow_rays++;
                            }
                        }
                    }
                }
            }

            const float scale = 1.0f / static_cast<float>(actual_samples);
            pixel_color = pixel_color * scale;
            pixel_normal = pixel_normal * scale;
            pixel_albedo = pixel_albedo * scale;
            total_depth *= scale;
            analysis.record_pixel(i, image_height - 1 - j, shadow_rays, total_rays, total_depth, pixel_normal, pixel_albedo);

            Color final_color = analysis.get_analysis_color(i, image_height - 1 - j, pixel_color);
            final_color.x = std::sqrt(final_color.x);
            final_color.y = std::sqrt(final_color.y);
            final_color.z = std::sqrt(final_color.z);

            const int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
            framebuffer[static_cast<size_t>(pixel_index) + 0] =
                static_cast<unsigned char>(256 * std::clamp(final_color.x, 0.0f, 0.999f));
            framebuffer[static_cast<size_t>(pixel_index) + 1] =
                static_cast<unsigned char>(256 * std::clamp(final_color.y, 0.0f, 0.999f));
            framebuffer[static_cast<size_t>(pixel_index) + 2] =
                static_cast<unsigned char>(256 * std::clamp(final_color.z, 0.0f, 0.999f));
        }
    }
}

static void tonemap_to_rgb24(const std::vector<std::vector<Color>>& src, int image_width, int image_height,
                             std::vector<unsigned char>& framebuffer) {
    #pragma omp parallel for schedule(dynamic, 16)
    for (int j = image_height - 1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            Color pixel_color = src[static_cast<size_t>(j)][static_cast<size_t>(i)];
            const int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
            framebuffer[static_cast<size_t>(pixel_index) + 0] =
                static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
            framebuffer[static_cast<size_t>(pixel_index) + 1] =
                static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
            framebuffer[static_cast<size_t>(pixel_index) + 2] =
                static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
        }
    }
}

static double median_of(std::vector<double>& v) {
    if (v.empty()) {
        return 0.0;
    }
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    if (n % 2 == 1) {
        return v[n / 2];
    }
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

int main(int argc, char** argv) {
    int image_width = 800;
    int samples = 4;
    int reps = 3;
    int warmup = 1;

    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "-w") == 0 || std::strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            image_width = std::atoi(argv[++i]);
        } else if ((std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--samples") == 0) && i + 1 < argc) {
            samples = std::atoi(argv[++i]);
        } else if ((std::strcmp(argv[i], "-r") == 0 || std::strcmp(argv[i], "--reps") == 0) && i + 1 < argc) {
            reps = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            warmup = std::atoi(argv[++i]);
        }
    }

    if (image_width < 160 || image_width > 4096) {
        image_width = 800;
    }
    if (samples < 1) {
        samples = 1;
    }
    if (reps < 1) {
        reps = 1;
    }
    if (warmup < 0) {
        warmup = 0;
    }

    const int image_height = static_cast<int>(image_width / (16.0f / 9.0f));
    const size_t nbytes = static_cast<size_t>(image_width) * static_cast<size_t>(image_height) * 3u;

    Scene scene;
    setup_cornell_box_scene(scene);
    scene.optimize_spatial_layout();

    const float aspect_ratio = 16.0f / 9.0f;
    Point3 lookfrom(0, 2, 15);
    Point3 lookat(0, 2, 0);
    Vec3 vup(0, 1, 0);
    const float vfov = 60;
    const float dist_to_focus = (lookfrom - lookat).length();
    const float aperture = 0.0f;
    Camera cam(lookfrom, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);

    RenderAnalysis analysis;
    analysis.resize(image_width, image_height);

    std::cout << "bench_cpu_render_modes  Cornell box  " << image_width << "x" << image_height
              << "  samples=" << samples << "  reps=" << reps << "  warmup=" << warmup << "\n";
#ifdef _OPENMP
    std::cout << "OpenMP threads: " << omp_get_max_threads() << "\n";
#endif
    std::cout << std::fixed << std::setprecision(4);

    auto bench = [&](const char* label, auto&& fn) {
        std::vector<double> times;
        times.reserve(static_cast<size_t>(reps));
        for (int w = 0; w < warmup; ++w) {
            fn();
        }
        for (int r = 0; r < reps; ++r) {
            const auto t0 = clock_hr::now();
            fn();
            const auto t1 = clock_hr::now();
            times.push_back(std::chrono::duration<double>(t1 - t0).count());
        }
        const double med = median_of(times);
        const double mrays = (static_cast<double>(image_width) * image_height * samples) / (1e6 * med);
        std::cout << label << "  median_s=" << med << "  MRays/s_est=" << mrays << "\n";
    };

    {
        Renderer ray_renderer(5);
        ray_renderer.enable_shadows = true;
        ray_renderer.enable_reflections = true;
        ray_renderer.enable_wavefront = false;
        ray_renderer.enable_simd_packets = false;
        std::vector<unsigned char> framebuffer(nbytes);
        bench("scalar_openmp   ", [&]() {
            std::fill(framebuffer.begin(), framebuffer.end(), 0);
            render_scalar_path(cam, scene, ray_renderer, analysis, image_width, image_height, samples, true, framebuffer);
        });
    }

    {
        Renderer ray_renderer(5);
        ray_renderer.enable_shadows = true;
        ray_renderer.enable_reflections = true;
        ray_renderer.enable_wavefront = false;
        ray_renderer.enable_simd_packets = true;
        std::vector<std::vector<Color>> simd_fb(static_cast<size_t>(image_height),
                                               std::vector<Color>(static_cast<size_t>(image_width)));
        std::vector<unsigned char> framebuffer(nbytes);
        bench("simd_packets    ", [&]() {
            for (auto& row : simd_fb) {
                std::fill(row.begin(), row.end(), Color(0, 0, 0));
            }
            ray_renderer.render_simd_packets(cam, scene, simd_fb, image_width, image_height, samples);
            tonemap_to_rgb24(simd_fb, image_width, image_height, framebuffer);
        });
    }

    {
        Renderer ray_renderer(5);
        ray_renderer.enable_shadows = true;
        ray_renderer.enable_reflections = true;
        ray_renderer.enable_wavefront = true;
        ray_renderer.enable_simd_packets = false;
        std::vector<std::vector<Color>> wf_fb(static_cast<size_t>(image_height),
                                             std::vector<Color>(static_cast<size_t>(image_width)));
        std::vector<unsigned char> framebuffer(nbytes);
        bench("wavefront       ", [&]() {
            for (auto& row : wf_fb) {
                std::fill(row.begin(), row.end(), Color(0, 0, 0));
            }
            ray_renderer.render_wavefront(cam, scene, wf_fb, image_width, image_height, samples);
            tonemap_to_rgb24(wf_fb, image_width, image_height, framebuffer);
        });
    }

    return 0;
}
