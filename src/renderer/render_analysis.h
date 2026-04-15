#ifndef RENDER_ANALYSIS_H
#define RENDER_ANALYSIS_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../scene/scene.h"
#include "../renderer/performance.h"
#include <vector>
#include <chrono>
#include <cmath>
#include <limits>

enum class AnalysisMode {
    NORMAL = 0,          // Standard rendering
    NORMALS = 1,         // Surface normals
    DEPTH = 2,           // Depth buffer
    ALBEDO = 3           // Material colors
};

class RenderAnalysis {
private:
    std::vector<int> pixel_shadow_rays;      // Shadow rays per pixel
    std::vector<int> pixel_total_rays;       // Total rays per pixel
    std::vector<float> pixel_depths;
    std::vector<Color> pixel_normals;
    std::vector<Color> pixel_albedos;
    int width, height;

    AnalysisMode current_mode;

public:
    RenderAnalysis() : current_mode(AnalysisMode::NORMAL) {}

    void set_mode(AnalysisMode mode) { current_mode = mode; }
    AnalysisMode get_mode() const { return current_mode; }
    const char* get_mode_name() const {
        switch (current_mode) {
            case AnalysisMode::NORMAL: return "None";
            case AnalysisMode::NORMALS: return "Normals";
            case AnalysisMode::DEPTH: return "Depth";
            case AnalysisMode::ALBEDO: return "Albedo";
            default: return "Unknown";
        }
    }

    void resize(int w, int h) {
        width = w;
        height = h;
        size_t size = w * h;
        pixel_shadow_rays.resize(size);
        pixel_total_rays.resize(size);
        pixel_depths.resize(size);
        pixel_normals.resize(size);
        pixel_albedos.resize(size);

        // Clear buffers
        std::fill(pixel_shadow_rays.begin(), pixel_shadow_rays.end(), 0);
        std::fill(pixel_total_rays.begin(), pixel_total_rays.end(), 0);
        std::fill(pixel_depths.begin(), pixel_depths.end(), 0.0f);
        std::fill(pixel_normals.begin(), pixel_normals.end(), Color(0, 0, 0));
        std::fill(pixel_albedos.begin(), pixel_albedos.end(), Color(0, 0, 0));
    }

    void record_pixel(int x, int y, int shadow_rays, int total_rays, float depth,
                     const Color& normal, const Color& albedo) {
        int idx = y * width + x;
        if (idx >= 0 && idx < width * height) {
            pixel_shadow_rays[idx] = shadow_rays;
            pixel_total_rays[idx] = total_rays;
            pixel_depths[idx] = depth;
            pixel_normals[idx] = normal;
            pixel_albedos[idx] = albedo;
        }
    }

    Color get_analysis_color(int x, int y, const Color& normal_render) const {
        int idx = y * width + x;
        if (idx < 0 || idx >= width * height) return Color(1, 0, 1); // Error: magenta

        switch (current_mode) {
            case AnalysisMode::NORMAL:
                return normal_render;

            case AnalysisMode::NORMALS: {
                // Visualize normals as colors
                return Color(
                    0.5f * (pixel_normals[idx].x + 1.0f),
                    0.5f * (pixel_normals[idx].y + 1.0f),
                    0.5f * (pixel_normals[idx].z + 1.0f)
                );
            }

            case AnalysisMode::DEPTH: {
                // Depth buffer: near (black) -> far (white)
                float d = pixel_depths[idx];
                float normalized = std::min(d / 30.0f, 1.0f);  // Normalize to 30 units max
                return Color(normalized, normalized, normalized);
            }

            case AnalysisMode::ALBEDO: {
                // Material albedo colors
                return pixel_albedos[idx];
            }

            default:
                return normal_render;
        }
    }

    // Get statistics for overlay
    int get_total_rays_cast() const {
        // Placeholder for compatibility
        return 0;
    }


private:
    Color heat_map_color(float value) const {
        // Blue -> Green -> Red heat map
        value = std::clamp(value, 0.0f, 1.0f);

        if (value < 0.5f) {
            // Blue to Green
            float t = value * 2.0f;
            return Color(0, t, 1.0f - t);
        } else {
            // Green to Red
            float t = (value - 0.5f) * 2.0f;
            return Color(t, 1.0f - t, 0);
        }
    }
};

#endif // RENDER_ANALYSIS_H
