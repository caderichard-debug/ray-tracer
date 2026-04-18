// Headless feature matrix: time each toggle / render path vs scalar baseline (Cornell box).
// Build: make bench-feature-deltas
// Run:   ./build/bench_feature_deltas ... [--markdown-out f] [--png-out dir]
//        All preview PNGs for one run go under feature_deltas_latest_run/ (cleared each run). Optional --png-out sets that directory.
//        Scaling tables (resolution, SPP, max_depth, threads) are ON by default; pass --no-sweep to skip.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image_write.h"

#include "math/pcg_random.h"
#include "math/stratified.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "scene/cornell_box.h"
#include "renderer/renderer.h"
#include "renderer/render_analysis.h"
#include "primitives/sphere.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

using clock_hr = std::chrono::high_resolution_clock;

// Matches interactive scalar branch (non-progressive): adaptive halves spp; analysis skips when adaptive.
static void render_scalar_interactive_like(const Camera& cam, Scene& scene, Renderer& ray_renderer,
                                           RenderAnalysis& analysis, int image_width, int image_height, int samples,
                                           std::vector<unsigned char>& framebuffer) {
    if (!scene.simd_scene_cache_ready()) {
        scene.build_simd_cache();
    }
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

            const int actual_samples =
                ray_renderer.enable_adaptive ? std::max(1, samples / 2) : samples;

            std::vector<std::pair<float, float>> stratified_samples;
            if (ray_renderer.enable_stratified) {
                stratified_samples = Stratified::generate_stratified_samples(actual_samples);
            }

            for (int s = 0; s < actual_samples; ++s) {
                float u;
                float v;
                if (ray_renderer.enable_stratified) {
                    u = (i + stratified_samples[static_cast<size_t>(s)].first) / (image_width - 1);
                    v = (j + stratified_samples[static_cast<size_t>(s)].second) / (image_height - 1);
                } else {
                    u = (i + random_float_pcg()) / (image_width - 1);
                    v = (j + random_float_pcg()) / (image_height - 1);
                }

                Ray r = cam.get_ray(u, v);
                Color sample_color = ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                pixel_color = pixel_color + sample_color;
                total_rays += ray_renderer.max_depth;

                if (!ray_renderer.enable_adaptive) {
                    HitRecord rec;
                    if (ray_renderer.trace_primary(r, 0.001f, 1000.0f, rec, scene)) {
                        total_depth += rec.t;
                        pixel_normal = pixel_normal + Color(rec.normal.x, rec.normal.y, rec.normal.z);
                        if (rec.mat) {
                            pixel_albedo = pixel_albedo + rec.mat->albedo;
                        }

                        if (ray_renderer.enable_shadows) {
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
            }

            const float scale = 1.0f / static_cast<float>(actual_samples);
            pixel_color = pixel_color * scale;

            if (!ray_renderer.enable_adaptive) {
                pixel_normal = pixel_normal * scale;
                pixel_albedo = pixel_albedo * scale;
                total_depth *= scale;
                analysis.record_pixel(i, image_height - 1 - j, shadow_rays, total_rays, total_depth, pixel_normal,
                                      pixel_albedo);
            }

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

static void tonemap_to_rgb24(const std::vector<Color>& src, int image_width, int image_height,
                             std::vector<unsigned char>& framebuffer) {
    #pragma omp parallel for schedule(dynamic, 16)
    for (int j = image_height - 1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            Color pixel_color =
                src[static_cast<size_t>(j) * static_cast<size_t>(image_width) + static_cast<size_t>(i)];
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

static double mrays_est(int w, int h, int spp, double sec) {
    if (sec <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(w) * h * spp / (1e6 * sec);
}

static std::string slug_for_filename(const std::string& label) {
    std::string s;
    s.reserve(label.size());
    for (unsigned char uc : label) {
        if (std::isalnum(uc) != 0) {
            s += static_cast<char>(std::tolower(uc));
        } else {
            s += '_';
        }
    }
    while (s.size() > 1 && s[0] == '_') {
        s.erase(0, 1);
    }
    for (size_t i = 0; i + 1 < s.size();) {
        if (s[i] == '_' && s[i + 1] == '_') {
            s.erase(i, 1);
        } else {
            ++i;
        }
    }
    while (!s.empty() && s.back() == '_') {
        s.pop_back();
    }
    if (s.empty()) {
        s = "frame";
    }
    if (s.size() > 100) {
        s.resize(100);
    }
    return s;
}

static std::string markdown_path_to_png(const std::string& md_file, const std::string& png_file) {
    namespace fs = std::filesystem;
    try {
        const fs::path md_p(md_file);
        const fs::path png_p(png_file);
        if (md_p.has_parent_path()) {
            return fs::relative(png_p, md_p.parent_path()).generic_string();
        }
        return png_p.filename().string();
    } catch (...) {
        return std::filesystem::path(png_file).filename().string();
    }
}

struct BenchStats {
    double median = 0;
    double min_t = 0;
    double max_t = 0;
    double stddev = 0;
};

template <typename Fn>
BenchStats run_bench_stats(int warmup_count, int rep_count, Fn&& fn) {
    std::vector<double> times;
    times.reserve(static_cast<size_t>(std::max(0, rep_count)));
    for (int w = 0; w < warmup_count; ++w) {
        fn();
    }
    for (int r = 0; r < rep_count; ++r) {
        const auto t0 = clock_hr::now();
        fn();
        const auto t1 = clock_hr::now();
        times.push_back(std::chrono::duration<double>(t1 - t0).count());
    }
    double sd = 0;
    if (times.size() >= 2) {
        double sum = 0;
        for (double x : times) {
            sum += x;
        }
        const double mean = sum / static_cast<double>(times.size());
        double acc = 0;
        for (double x : times) {
            const double d = x - mean;
            acc += d * d;
        }
        sd = std::sqrt(acc / static_cast<double>(times.size() - 1));
    }
    std::sort(times.begin(), times.end());
    const size_t n = times.size();
    double med = 0;
    if (n == 0) {
        med = 0;
    } else if (n % 2 == 1) {
        med = times[n / 2];
    } else {
        med = 0.5 * (times[n / 2 - 1] + times[n / 2]);
    }
    BenchStats out;
    out.median = med;
    out.min_t = n ? times.front() : 0;
    out.max_t = n ? times.back() : 0;
    out.stddev = sd;
    return out;
}

struct SceneCounts {
    int objects = 0;
    int spheres = 0;
    int non_spheres = 0;
    int lights = 0;
};

static SceneCounts count_scene(const Scene& scene) {
    SceneCounts c;
    c.lights = static_cast<int>(scene.lights.size());
    for (const auto& obj : scene.objects) {
        c.objects++;
        if (std::dynamic_pointer_cast<Sphere>(obj)) {
            c.spheres++;
        } else {
            c.non_spheres++;
        }
    }
    return c;
}

static std::string escape_md_cell(std::string s) {
    for (char& ch : s) {
        if (ch == '|') {
            ch = ' ';
        }
    }
    return s;
}

static std::string cpu_brand_string() {
#if defined(__APPLE__)
    char buf[512];
    size_t buflen = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &buflen, nullptr, 0) == 0 && buflen > 0) {
        return std::string(buf, strnlen(buf, sizeof(buf)));
    }
#elif defined(__linux__)
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, 10, "model name") == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string v = line.substr(colon + 1);
                while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) {
                    v.erase(0, 1);
                }
                return v;
            }
        }
    }
#endif
    return {};
}

static std::string compiler_desc() {
#ifdef __clang__
    std::ostringstream o;
    o << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
    return o.str();
#elif defined(__GNUC__)
    std::ostringstream o;
    o << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
    return o.str();
#else
    return "unknown";
#endif
}

static double peak_rss_mb_posix() {
#if !defined(_WIN32)
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
#if defined(__APPLE__)
        return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0); // ru_maxrss is bytes
#else
        return static_cast<double>(ru.ru_maxrss) / 1024.0; // kilobytes -> MB
#endif
    }
#endif
    return -1.0;
}

int main(int argc, char** argv) {
    int image_width = 640;
    int samples = 4;
    int reps = 5;
    int warmup = 2;
    int omp_threads_override = -1;
    bool sweep_mode = true;
    std::string out_md;
    std::string out_png;

    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "-w") == 0 || std::strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            image_width = std::atoi(argv[++i]);
        } else if ((std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--samples") == 0) && i + 1 < argc) {
            samples = std::atoi(argv[++i]);
        } else if ((std::strcmp(argv[i], "-r") == 0 || std::strcmp(argv[i], "--reps") == 0) && i + 1 < argc) {
            reps = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            warmup = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            omp_threads_override = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--sweep") == 0) {
            sweep_mode = true;
        } else if (std::strcmp(argv[i], "--no-sweep") == 0) {
            sweep_mode = false;
        } else if (std::strcmp(argv[i], "--markdown-out") == 0 && i + 1 < argc) {
            out_md = argv[++i];
        } else if (std::strcmp(argv[i], "--png-out") == 0 && i + 1 < argc) {
            out_png = argv[++i]; // directory for this run's PNG set (default: .../feature_deltas_latest_run)
        }
    }

    namespace fs = std::filesystem;
    std::string run_img_dir;
    if (!out_png.empty()) {
        run_img_dir = out_png;
    } else if (!out_md.empty()) {
        run_img_dir = (fs::path(out_md).parent_path() / "feature_deltas_latest_run").string();
    } else {
        run_img_dir = "benchmark_results/summaries/feature_deltas_latest_run";
    }
    {
        std::error_code ec;
        fs::remove_all(run_img_dir, ec);
        fs::create_directories(run_img_dir, ec);
    }

#ifdef _OPENMP
    if (omp_threads_override > 0) {
        omp_set_num_threads(omp_threads_override);
    }
#endif

    const auto wall_run_start = clock_hr::now();

    const int image_height = static_cast<int>(image_width / (16.0f / 9.0f));
    const size_t nbytes = static_cast<size_t>(image_width) * static_cast<size_t>(image_height) * 3u;
    const float aspect_ratio = 16.0f / 9.0f;
    Point3 lookfrom(0, 2, 15);
    Point3 lookat(0, 2, 0);
    Vec3 vup(0, 1, 0);
    const float vfov = 60;
    const float dist_to_focus = (lookfrom - lookat).length();
    const float aperture = 0.0f;
    Camera cam(lookfrom, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);

    Scene scene;
    setup_cornell_box_scene(scene);
    scene.optimize_spatial_layout();
    scene.build_simd_cache();
    const SceneCounts sc_counts = count_scene(scene);

    RenderAnalysis analysis;
    analysis.resize(image_width, image_height);

    auto bench = [&](int wu, int rp, auto&& fn) {
        std::vector<double> times;
        times.reserve(static_cast<size_t>(rp));
        for (int w = 0; w < wu; ++w) {
            fn();
        }
        for (int r = 0; r < rp; ++r) {
            const auto t0 = clock_hr::now();
            fn();
            const auto t1 = clock_hr::now();
            times.push_back(std::chrono::duration<double>(t1 - t0).count());
        }
        return median_of(times);
    };

    std::ostringstream md;
    md << "# Feature delta benchmark\n\n";
    md << "- **Scene:** Cornell box\n";
    md << "- **Resolution:** " << image_width << "x" << image_height << "\n";
    md << "- **Samples (baseline):** " << samples << "\n";
    md << "- **Timed reps (median):** " << reps << ", warmup: " << warmup << "\n";
#ifdef _OPENMP
    md << "- **OpenMP threads:** " << omp_get_max_threads();
    if (omp_threads_override > 0) {
        md << " (set via `--threads " << omp_threads_override << "`)";
    }
    md << "\n";
#endif
    if (sweep_mode) {
        md << "- **Scaling sweeps:** on (resolution, SPP, max_depth, threads); use `--no-sweep` to omit\n";
    } else {
        md << "- **Scaling sweeps:** off (`--no-sweep`)\n";
    }
    md << "\n";

    std::vector<std::pair<std::string, std::string>> run_gallery;
    int run_img_seq = 0;
    auto add_run_png = [&](const std::string& caption, const std::vector<unsigned char>& fb) {
        ++run_img_seq;
        std::ostringstream stem;
        stem << std::setw(2) << std::setfill('0') << run_img_seq << "_" << slug_for_filename(caption) << ".png";
        const fs::path full = fs::path(run_img_dir) / stem.str();
        const int ok =
            stbi_write_png(full.string().c_str(), image_width, image_height, 3, fb.data(), image_width * 3);
        if (!ok) {
            std::cerr << "WARNING: failed to write PNG: " << full.string() << "\n";
            return;
        }
        std::cout << "Preview PNG: " << full.string() << "\n";
        if (!out_md.empty()) {
            run_gallery.emplace_back(caption, markdown_path_to_png(out_md, full.string()));
        }
    };

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Preview image folder (cleared this run): " << run_img_dir << "\n";

    auto row_scalar = [&](const std::string& label, Renderer& rr, int eff_samples, const std::string& notes,
                          double t_base, std::vector<unsigned char>& fb) {
        const double t = bench(warmup, reps, [&]() {
            std::fill(fb.begin(), fb.end(), 0);
            render_scalar_interactive_like(cam, scene, rr, analysis, image_width, image_height, samples, fb);
        });
        const double mr = mrays_est(image_width, image_height, eff_samples, t);
        std::ostringstream vs;
        if (t_base > 0.0 && std::isfinite(t) && t > 0.0) {
            const double pct = 100.0 * (t - t_base) / t_base;
            vs << std::fixed << std::setprecision(1) << pct << "% time";
        } else {
            vs << "—";
        }
        md << "| " << label << " | " << std::setprecision(4) << t << " | " << vs.str() << " | " << std::setprecision(3)
           << mr << " | " << notes << " |\n";
        std::cout << label << "  t=" << t << "  MRays/s~" << mr << "\n";
        add_run_png(label, fb);
        return t;
    };

    auto row_path = [&](const std::string& label, int eff_samples, auto&& fn, double t_base, const std::string& notes) {
        std::vector<unsigned char> fb(nbytes);
        const double t = bench(warmup, reps, [&]() {
            std::fill(fb.begin(), fb.end(), 0);
            fn(fb);
        });
        const double mr = mrays_est(image_width, image_height, eff_samples, t);
        std::ostringstream vs;
        if (t_base > 0.0 && std::isfinite(t) && t > 0.0) {
            const double speedup = 100.0 * (t_base / t - 1.0);
            vs << std::fixed << std::setprecision(1) << speedup << "% faster";
        } else {
            vs << "—";
        }
        md << "| " << label << " | " << std::setprecision(4) << t << " | " << vs.str() << " | " << std::setprecision(3)
           << mr << " | " << notes << " |\n";
        std::cout << label << "  t=" << t << "  MRays/s~" << mr << "\n";
        add_run_png(label, fb);
        return t;
    };

    std::vector<unsigned char> framebuffer(nbytes);

    Renderer baseline(5);
    baseline.enable_shadows = true;
    baseline.enable_reflections = true;
    baseline.enable_adaptive = false;
    baseline.enable_wavefront = false;
    baseline.enable_simd_packets = false;

    const BenchStats base_st = run_bench_stats(warmup, reps, [&]() {
        std::fill(framebuffer.begin(), framebuffer.end(), 0);
        render_scalar_interactive_like(cam, scene, baseline, analysis, image_width, image_height, samples,
                                       framebuffer);
    });
    const double t_base = base_st.median;
    const double mr_base = mrays_est(image_width, image_height, samples, t_base);

    add_run_png("Baseline scalar (reference)", framebuffer);

    md << "| Configuration | Median time (s) | vs baseline scalar | MRays/s (est.) | Notes |\n";
    md << "|---|---:|---:|---:|---|\n";
    md << "| **Baseline scalar** (shadows+reflections, adaptive off, depth 5) | " << std::setprecision(4) << t_base
       << " | — | " << std::setprecision(3) << mr_base << " | reference |\n";
    std::cout << "BASELINE scalar  t=" << t_base << "  MRays/s~" << mr_base << "\n";

    {
        Renderer rr(5);
        rr.enable_shadows = false;
        rr.enable_reflections = true;
        rr.enable_adaptive = false;
        row_scalar("Shadows OFF", rr, samples, "interactive-equivalent scalar", t_base, framebuffer);
    }
    {
        Renderer rr(5);
        rr.enable_shadows = true;
        rr.enable_reflections = false;
        row_scalar("Reflections OFF", rr, samples, "no recursive bounce path", t_base, framebuffer);
    }
    {
        Renderer rr(5);
        rr.enable_shadows = false;
        rr.enable_reflections = false;
        row_scalar("Shadows OFF + Reflections OFF", rr, samples, "", t_base, framebuffer);
    }
    {
        Renderer rr(5);
        rr.enable_shadows = true;
        rr.enable_reflections = true;
        rr.enable_adaptive = true;
        const int eff = std::max(1, samples / 2);
        row_scalar("Adaptive ON (½ samples)", rr, eff, "~half primary rays vs baseline", t_base, framebuffer);
    }
    {
        Renderer rr(3);
        rr.enable_shadows = true;
        rr.enable_reflections = true;
        rr.enable_adaptive = false;
        row_scalar("max_depth 3 (was 5)", rr, samples, "shallower ray_color recursion", t_base, framebuffer);
    }

    {
        Renderer rr(5);
        rr.enable_shadows = true;
        rr.enable_reflections = true;
        rr.enable_adaptive = false;
        rr.enable_bvh = true;
        rr.build_bvh(scene);
        row_scalar("BVH ON (spheres + linear quads)", rr, samples,
                   "BVH spheres + linear quads; tiny scenes often slower than one linear scan", t_base, framebuffer);
    }
    {
        Renderer rr(5);
        rr.enable_shadows = true;
        rr.enable_reflections = true;
        rr.enable_adaptive = false;
        rr.enable_frustum = true;
        row_scalar("View frustum culling ON (primary rays)", rr, samples,
                   "primary rays only; per-sphere frustum tests can cost more than they save here", t_base, framebuffer);
    }
    {
        Renderer rr(5);
        rr.enable_shadows = true;
        rr.enable_reflections = true;
        rr.enable_adaptive = false;
        rr.enable_stratified = true;
        row_scalar("Stratified primary samples ON", rr, samples,
                   "same spp; jittered grid + shuffle vs white noise per sample", t_base, framebuffer);
    }

    const double t_simd = row_path(
        "SIMD packets (vs scalar baseline)", samples,
        [&](std::vector<unsigned char>& fb) {
            Renderer rr(5);
            rr.enable_shadows = true;
            rr.enable_reflections = true;
            rr.enable_simd_packets = true;
            std::vector<Color> simd_fb(static_cast<size_t>(image_width) * static_cast<size_t>(image_height));
            rr.render_simd_packets(cam, scene, simd_fb, image_width, image_height, samples);
            tonemap_to_rgb24(simd_fb, image_width, image_height, fb);
        },
        t_base, "AVX2 packet path; same spp");

    const double t_wave = row_path(
        "Wavefront (vs scalar baseline)", samples,
        [&](std::vector<unsigned char>& fb) {
            Renderer rr(5);
            rr.enable_shadows = true;
            rr.enable_reflections = true;
            rr.enable_wavefront = true;
            std::vector<Color> wf_fb(static_cast<size_t>(image_width) * static_cast<size_t>(image_height));
            rr.render_wavefront(cam, scene, wf_fb, image_width, image_height, samples);
            tonemap_to_rgb24(wf_fb, image_width, image_height, fb);
        },
        t_base, "tiled wavefront path");

    const double t_morton = row_path(
        "Morton traversal (vs scalar baseline)", samples,
        [&](std::vector<unsigned char>& fb) {
            Renderer rr(5);
            rr.enable_shadows = true;
            rr.enable_reflections = true;
            std::vector<Color> mf(static_cast<size_t>(image_width) * static_cast<size_t>(image_height));
            rr.render_morton(cam, scene, mf, image_width, image_height, samples);
            tonemap_to_rgb24(mf, image_width, image_height, fb);
        },
        t_base, "Z-curve pixel order");

    if (!run_gallery.empty()) {
        std::string run_dir_rel = "feature_deltas_latest_run";
        if (!out_md.empty()) {
            try {
                run_dir_rel =
                    fs::relative(fs::path(run_img_dir), fs::path(out_md).parent_path()).generic_string();
            } catch (...) {
                run_dir_rel = "feature_deltas_latest_run";
            }
        }
        md << "\n### Preview images (latest run only)\n\n";
        md << "PNGs live under **`" << run_dir_rel << "/`** (the folder is removed at the start of each benchmark; "
              "only images from the latest run remain).\n\n";
        for (const auto& entry : run_gallery) {
            md << "**" << escape_md_cell(entry.first) << "**  \n";
            md << "![](" << entry.second << ")\n\n";
        }
    }

    if (sweep_mode) {
        md << "\n## Scaling sweeps (baseline scalar)\n\n";
        md << "Shadows+reflections, adaptive off, SIMD/wavefront off, BVH/frustum/stratified off. "
              "Default **`Renderer` max depth is 5** for resolution / samples / thread sweeps. "
              "SPP and OpenMP tables are run at **640×360** (default workload) and again at **1280×720** (max width in the resolution sweep). "
              "Deltas are **% change in median time** vs the reference row in each table (negative = faster). "
              "**MRays/s** is estimated primary throughput (`width×height×spp / time`); max_depth changes secondary rays without changing that formula.\n\n";

        auto make_baseline_renderer = [](int max_depth) {
            Renderer rr(max_depth);
            rr.enable_shadows = true;
            rr.enable_reflections = true;
            rr.enable_adaptive = false;
            rr.enable_wavefront = false;
            rr.enable_simd_packets = false;
            rr.enable_bvh = false;
            rr.enable_frustum = false;
            rr.enable_stratified = false;
            return rr;
        };

        const float aspect = 16.0f / 9.0f;
        const float vfov = 60;
        const float aperture = 0.0f;

        // --- Resolution (fixed spp = CLI samples) ---
        md << "### Resolution (fixed spp = " << samples << ")\n\n";
        md << "| Width | Height | Median (s) | vs 640w | MRays/s |\n|---:|---:|---:|---:|---:|\n";
        struct ResRow {
            int w;
            int h;
            double t;
            double mr;
        };
        std::vector<ResRow> res_rows;
        res_rows.reserve(8);
        for (int w : {320, 480, 640, 960, 1280}) {
            const int h = static_cast<int>(w / aspect);
            Camera csw(lookfrom, lookat, vup, vfov, aspect, aperture, (lookfrom - lookat).length());
            analysis.resize(w, h);
            std::vector<unsigned char> fb(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
            Renderer rr = make_baseline_renderer(5);
            const double t = bench(warmup, reps, [&]() {
                std::fill(fb.begin(), fb.end(), 0);
                render_scalar_interactive_like(csw, scene, rr, analysis, w, h, samples, fb);
            });
            res_rows.push_back(ResRow{w, h, t, mrays_est(w, h, samples, t)});
        }
        double ref_res_640 = 0.0;
        for (const auto& row : res_rows) {
            if (row.w == 640) {
                ref_res_640 = row.t;
                break;
            }
        }
        for (const auto& row : res_rows) {
            std::ostringstream vs;
            if (row.w == 640 || ref_res_640 <= 0.0) {
                vs << "—";
            } else if (std::isfinite(row.t) && row.t > 0.0) {
                vs << std::fixed << std::setprecision(1) << (100.0 * (row.t - ref_res_640) / ref_res_640) << "% time";
            } else {
                vs << "—";
            }
            md << "| " << row.w << " | " << row.h << " | " << std::setprecision(4) << row.t << " | " << vs.str()
               << " | " << std::setprecision(3) << row.mr << " |\n";
        }

        // reset analysis to main image size for sanity
        analysis.resize(image_width, image_height);

        // --- Samples (fixed 640w) ---
        md << "\n### Samples per pixel (fixed 640×360)\n\n";
        md << "| SPP | Median (s) | vs spp=4 | MRays/s |\n|---:|---:|---:|---:|\n";
        const int sw = 640;
        const int sh = static_cast<int>(sw / aspect);
        Camera csp(lookfrom, lookat, vup, vfov, aspect, aperture, (lookfrom - lookat).length());
        analysis.resize(sw, sh);
        std::vector<unsigned char> fbs(static_cast<size_t>(sw) * static_cast<size_t>(sh) * 3u);
        struct SppRow {
            int spp;
            double t;
            double mr;
        };
        std::vector<SppRow> spp_rows;
        for (int spp : {2, 4, 8, 16}) {
            Renderer rr = make_baseline_renderer(5);
            const double t = bench(warmup, reps, [&]() {
                std::fill(fbs.begin(), fbs.end(), 0);
                render_scalar_interactive_like(csp, scene, rr, analysis, sw, sh, spp, fbs);
            });
            spp_rows.push_back(SppRow{spp, t, mrays_est(sw, sh, spp, t)});
        }
        double ref_spp4 = 0.0;
        for (const auto& row : spp_rows) {
            if (row.spp == 4) {
                ref_spp4 = row.t;
                break;
            }
        }
        for (const auto& row : spp_rows) {
            std::ostringstream vs;
            if (row.spp == 4 || ref_spp4 <= 0.0) {
                vs << "—";
            } else if (std::isfinite(row.t) && row.t > 0.0) {
                vs << std::fixed << std::setprecision(1) << (100.0 * (row.t - ref_spp4) / ref_spp4) << "% time";
            } else {
                vs << "—";
            }
            md << "| " << row.spp << " | " << std::setprecision(4) << row.t << " | " << vs.str() << " | "
               << std::setprecision(3) << row.mr << " |\n";
        }

        // --- max_depth (fixed 640×360, spp=4) ---
        md << "\n### Max recursion depth / ray bounces (fixed 640×360, spp=4)\n\n";
        md << "Caps `ray_color` recursion (reflection path). MRays/s still uses primary SPP only (same as other sweeps).\n\n";
        md << "| max_depth | Median (s) | vs depth 5 | MRays/s |\n|---:|---:|---:|---:|\n";
        analysis.resize(sw, sh);
        struct DepthRow {
            int max_depth;
            double t;
            double mr;
        };
        std::vector<DepthRow> depth_rows;
        for (int max_d : {1, 2, 3, 4, 5, 6, 8}) {
            Renderer rr = make_baseline_renderer(max_d);
            const double t = bench(warmup, reps, [&]() {
                std::fill(fbs.begin(), fbs.end(), 0);
                render_scalar_interactive_like(csp, scene, rr, analysis, sw, sh, 4, fbs);
            });
            depth_rows.push_back(DepthRow{max_d, t, mrays_est(sw, sh, 4, t)});
        }
        double ref_depth5 = 0.0;
        for (const auto& row : depth_rows) {
            if (row.max_depth == 5) {
                ref_depth5 = row.t;
                break;
            }
        }
        for (const auto& row : depth_rows) {
            std::ostringstream vs;
            if (row.max_depth == 5 || ref_depth5 <= 0.0) {
                vs << "—";
            } else if (std::isfinite(row.t) && row.t > 0.0) {
                vs << std::fixed << std::setprecision(1) << (100.0 * (row.t - ref_depth5) / ref_depth5) << "% time";
            } else {
                vs << "—";
            }
            md << "| " << row.max_depth << " | " << std::setprecision(4) << row.t << " | " << vs.str() << " | "
               << std::setprecision(3) << row.mr << " |\n";
        }

        analysis.resize(sw, sh);

        // --- OpenMP threads (fixed 640w, spp=4) ---
        md << "\n### OpenMP threads (fixed 640×360, spp=4)\n\n";
        md << "| Threads | Median (s) | vs first row | MRays/s |\n|---:|---:|---:|---:|\n";
#ifdef _OPENMP
        const int max_hw = omp_get_num_procs();
        double ref_thr_t = 0.0;
        int thr_i = 0;
        for (int nt : {1, 2, 4, 8, 16, 32}) {
            if (nt > max_hw && nt > 8) {
                continue;
            }
            omp_set_num_threads(nt);
            Renderer rr = make_baseline_renderer(5);
            const double t = bench(warmup, reps, [&]() {
                std::fill(fbs.begin(), fbs.end(), 0);
                render_scalar_interactive_like(csp, scene, rr, analysis, sw, sh, 4, fbs);
            });
            const double mr = mrays_est(sw, sh, 4, t);
            std::ostringstream vs;
            if (thr_i == 0) {
                ref_thr_t = t;
                vs << "—";
            } else if (ref_thr_t > 0.0 && std::isfinite(t) && t > 0.0) {
                vs << std::fixed << std::setprecision(1) << (100.0 * (t - ref_thr_t) / ref_thr_t) << "% time";
            } else {
                vs << "—";
            }
            ++thr_i;
            md << "| " << nt << " | " << std::setprecision(4) << t << " | " << vs.str() << " | " << std::setprecision(3)
               << mr << " |\n";
        }
#else
        md << "| *OpenMP not compiled in* | — | — | — |\n";
#endif

        // --- Samples at max resolution in sweep (1280×720; same SPP grid as 640w table) ---
        constexpr int k_max_sweep_width = 1280;
        const int sw_max = k_max_sweep_width;
        const int sh_max = static_cast<int>(sw_max / aspect);
        md << "\n### Samples per pixel (fixed " << sw_max << "×" << sh_max << ")\n\n";
        md << "| SPP | Median (s) | vs spp=4 | MRays/s |\n|---:|---:|---:|---:|\n";
        analysis.resize(sw_max, sh_max);
        std::vector<unsigned char> fb_max(static_cast<size_t>(sw_max) * static_cast<size_t>(sh_max) * 3u);
        Camera cam_max(lookfrom, lookat, vup, vfov, aspect, aperture, (lookfrom - lookat).length());
        std::vector<SppRow> spp_rows_max;
        for (int spp : {2, 4, 8, 16}) {
            Renderer rr = make_baseline_renderer(5);
            const double t = bench(warmup, reps, [&]() {
                std::fill(fb_max.begin(), fb_max.end(), 0);
                render_scalar_interactive_like(cam_max, scene, rr, analysis, sw_max, sh_max, spp, fb_max);
            });
            spp_rows_max.push_back(SppRow{spp, t, mrays_est(sw_max, sh_max, spp, t)});
        }
        double ref_spp4_max = 0.0;
        for (const auto& row : spp_rows_max) {
            if (row.spp == 4) {
                ref_spp4_max = row.t;
                break;
            }
        }
        for (const auto& row : spp_rows_max) {
            std::ostringstream vs;
            if (row.spp == 4 || ref_spp4_max <= 0.0) {
                vs << "—";
            } else if (std::isfinite(row.t) && row.t > 0.0) {
                vs << std::fixed << std::setprecision(1) << (100.0 * (row.t - ref_spp4_max) / ref_spp4_max) << "% time";
            } else {
                vs << "—";
            }
            md << "| " << row.spp << " | " << std::setprecision(4) << row.t << " | " << vs.str() << " | "
               << std::setprecision(3) << row.mr << " |\n";
        }

        // --- OpenMP threads at max resolution (1280×720, spp=4) ---
        md << "\n### OpenMP threads (fixed " << sw_max << "×" << sh_max << ", spp=4)\n\n";
        md << "| Threads | Median (s) | vs first row | MRays/s |\n|---:|---:|---:|---:|\n";
#ifdef _OPENMP
        {
            const int max_hw_max = omp_get_num_procs();
            double ref_thr_max_t = 0.0;
            int thr_max_i = 0;
            for (int nt : {1, 2, 4, 8, 16, 32}) {
                if (nt > max_hw_max && nt > 8) {
                    continue;
                }
                omp_set_num_threads(nt);
                Renderer rr = make_baseline_renderer(5);
                const double t = bench(warmup, reps, [&]() {
                    std::fill(fb_max.begin(), fb_max.end(), 0);
                    render_scalar_interactive_like(cam_max, scene, rr, analysis, sw_max, sh_max, 4, fb_max);
                });
                const double mr = mrays_est(sw_max, sh_max, 4, t);
                std::ostringstream vs;
                if (thr_max_i == 0) {
                    ref_thr_max_t = t;
                    vs << "—";
                } else if (ref_thr_max_t > 0.0 && std::isfinite(t) && t > 0.0) {
                    vs << std::fixed << std::setprecision(1) << (100.0 * (t - ref_thr_max_t) / ref_thr_max_t) << "% time";
                } else {
                    vs << "—";
                }
                ++thr_max_i;
                md << "| " << nt << " | " << std::setprecision(4) << t << " | " << vs.str() << " | " << std::setprecision(3)
                   << mr << " |\n";
            }
        }
#else
        md << "| *OpenMP not compiled in* | — | — | — |\n";
#endif

#ifdef _OPENMP
        if (omp_threads_override > 0) {
            omp_set_num_threads(omp_threads_override);
        }
#endif

        analysis.resize(image_width, image_height);
    }

    const double wall_sec = std::chrono::duration<double>(clock_hr::now() - wall_run_start).count();
    const double rss_mb = peak_rss_mb_posix();

    md << "\n## At a glance\n\n";
    md << "| Metric | Value |\n|:---|:---|\n";
    {
        std::string cpu = cpu_brand_string();
        if (cpu.empty()) {
            cpu = "— (run on macOS or Linux for CPU model string)";
        }
        md << "| CPU model | " << escape_md_cell(cpu) << " |\n";
    }
    md << "| `std::thread::hardware_concurrency()` | " << std::thread::hardware_concurrency() << " |\n";
    md << "| Compiler (this TU) | " << compiler_desc() << " |\n";
    md << "| Scene | " << sc_counts.objects << " primitives (" << sc_counts.spheres << " spheres, "
       << sc_counts.non_spheres << " non-spheres), " << sc_counts.lights << " lights |\n";
    md << "| Baseline timed reps | median **" << std::setprecision(4) << t_base << " s**, min " << base_st.min_t
       << " s, max " << base_st.max_t << " s";
    if (reps >= 2 && base_st.stddev > 0.0 && std::isfinite(base_st.stddev)) {
        md << ", sample σ **" << base_st.stddev << " s**";
    }
    if (t_base > 0.0 && base_st.max_t > 0.0) {
        const double spr = 100.0 * (base_st.max_t - base_st.min_t) / t_base;
        md << ", rep spread (max−min)/median **" << std::fixed << std::setprecision(1) << spr << "%**";
    }
    md << " |\n";
    if (t_base > 0.0) {
        const double px = static_cast<double>(image_width) * static_cast<double>(image_height);
        md << "| Baseline throughput | **" << std::fixed << std::setprecision(0) << (px / t_base)
           << "** px/s; **" << (px * static_cast<double>(samples) / t_base) << "** primary samples/s |\n";
    }
    if (rss_mb >= 0.0) {
        md << "| Peak resident set (getrusage maxrss) | **" << std::fixed << std::setprecision(2) << rss_mb
           << " MB** (high-water mark for this process; OS-dependent units) |\n";
    }
    md << "| Wall time (entire `bench_feature_deltas` run) | **" << std::fixed << std::setprecision(2) << wall_sec
       << " s** |\n";

    struct Alt {
        const char* name;
        double t;
    };
    const Alt alts[] = {{"SIMD packets", t_simd}, {"Wavefront", t_wave}, {"Morton", t_morton}};
    const Alt* best = nullptr;
    for (const Alt& a : alts) {
        if (a.t > 0.0 && std::isfinite(a.t)) {
            if (best == nullptr || a.t < best->t) {
                best = &a;
            }
        }
    }
    if (best != nullptr && t_base > 0.0) {
        const double sp = 100.0 * (t_base / best->t - 1.0);
        md << "| Fastest non-scalar row | **" << best->name << "** at " << std::setprecision(4) << best->t
           << " s → **" << std::setprecision(1) << sp << "% faster** than baseline scalar |\n";
    }
    md << "\n";

    std::cout << "\n--- At a glance ---\n";
    std::cout << "Wall time (full run): " << std::fixed << std::setprecision(2) << wall_sec << " s\n";
    if (rss_mb >= 0.0) {
        std::cout << "Peak RSS (approx.): " << std::setprecision(2) << rss_mb << " MB\n";
    }
    std::cout << "Baseline reps: min " << base_st.min_t << "  median " << t_base << "  max " << base_st.max_t;
    if (reps >= 2) {
        std::cout << "  sigma " << base_st.stddev;
    }
    std::cout << "\n";

    md << "\n## How to read **vs baseline**\n\n";
    md << "- **Scalar rows:** *% time* = `100 * (t - t_baseline) / t_baseline` — **negative** means faster than baseline.\n";
    md << "- **SIMD / wavefront / Morton:** *% faster* = `100 * (t_baseline / t - 1)` — **positive** means faster than baseline scalar.\n";
    md << "- **BVH / frustum / stratified:** same scalar *% time* semantics as other scalar toggles.\n";
    md << "- **Adaptive:** uses half the primary samples; MRays/s uses the effective sample count.\n";
    md << "- **Scaling tables** (on by default; `--no-sweep` to skip): *% time* vs the reference row in each subsection "
          "(resolution vs 640w, SPP vs 4 at **640×360 and 1280×720**, **max_depth vs 5**, OpenMP threads vs first row at "
          "**640×360 and 1280×720**). MRays/s ≈ primary work rate.\n";

    const std::string md_str = md.str();
    std::cout << "\n--- Markdown ---\n" << md_str;
    if (!out_md.empty()) {
        std::ofstream out(out_md);
        out << md_str;
        std::cout << "\nWrote " << out_md << "\n";
    }
    if (run_img_seq > 0) {
        std::cout << "Preview PNGs written: " << run_img_seq << " in " << run_img_dir << "\n";
    }

    return 0;
}
