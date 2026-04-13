#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>

class PerformanceTracker {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
    std::string phase_name;

    // Metrics
    int image_width;
    int image_height;
    long long total_rays;
    int samples_per_pixel;
    int max_depth;

    bool stopped;

public:
    PerformanceTracker(const std::string& phase, int width, int height, int samples = 1, int depth = 5)
        : phase_name(phase), image_width(width), image_height(height),
          total_rays(0), samples_per_pixel(samples), max_depth(depth), stopped(false) {
        start_time = std::chrono::high_resolution_clock::now();
    }

    // Stop timing and calculate results
    void stop() {
        if (!stopped) {
            end_time = std::chrono::high_resolution_clock::now();
            stopped = true;
        }
    }

    // Set ray count manually (or calculate from resolution)
    void set_ray_count(long long rays) {
        total_rays = rays;
    }

    // Calculate ray count from resolution and samples
    void calculate_ray_count() {
        // Base rays: 1 primary ray per sample per pixel
        long long base_rays = (long long)image_width * image_height * samples_per_pixel;

        // Estimate additional rays (shadows, reflections)
        // This is approximate - actual count depends on scene
        // Average: 1 shadow ray per hit + 0.5 reflection rays
        long long avg_bounces = 1 + max_depth * 0.5f;
        total_rays = base_rays * avg_bounces;
    }

    // Get elapsed time in seconds
    double get_elapsed_seconds() const {
        auto end = stopped ? end_time : std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_time);
        return duration.count() / 1000000.0;
    }

    // Calculate rays per second
    double get_rays_per_second() const {
        double seconds = get_elapsed_seconds();
        return (seconds > 0) ? (total_rays / seconds) : 0;
    }

    // Calculate pixels per second
    double get_pixels_per_second() const {
        double seconds = get_elapsed_seconds();
        long long total_pixels = (long long)image_width * image_height;
        return (seconds > 0) ? (total_pixels / seconds) : 0;
    }

    // Print performance report
    void print_report() {
        stop();

        double seconds = get_elapsed_seconds();
        long long total_pixels = (long long)image_width * image_height;

        std::cerr << "\n";
        std::cerr << "===========================================\n";
        std::cerr << "    PERFORMANCE REPORT: " << phase_name << "\n";
        std::cerr << "===========================================\n";
        std::cerr << "Image Size:       " << image_width << " x " << image_height
                  << " (" << total_pixels << " pixels)\n";
        std::cerr << "Samples/Pixel:    " << samples_per_pixel << "\n";
        std::cerr << "Max Depth:        " << max_depth << "\n";
        std::cerr << "Total Rays:       " << std::fixed << std::setprecision(0)
                  << (double)total_rays << "\n";
        std::cerr << "Render Time:      " << std::setprecision(3) << seconds << " seconds\n";
        std::cerr << "Throughput:       " << std::setprecision(2) << get_rays_per_second() / 1e6
                  << " MRays/sec\n";
        std::cerr << "Pixel Rate:       " << std::setprecision(0) << get_pixels_per_second()
                  << " pixels/sec\n";
        std::cerr << "===========================================\n";
        std::cerr << "\n";
        std::cerr.flush();
    }

    // Print single line summary (for progress tracking)
    void print_summary() {
        double seconds = get_elapsed_seconds();
        std::cout << "Rendered in " << std::fixed << std::setprecision(2) << seconds
                  << "s (" << std::setprecision(1) << get_rays_per_second() / 1e6
                  << " MRays/sec)\n";
    }
};

// RAII wrapper for automatic timing
class ScopedTimer {
private:
    PerformanceTracker& tracker;

public:
    ScopedTimer(PerformanceTracker& t) : tracker(t) {}

    ~ScopedTimer() {
        tracker.stop();
        tracker.print_report();
    }
};

#endif // PERFORMANCE_H
