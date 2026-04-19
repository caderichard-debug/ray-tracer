#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <limits>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <omp.h>
#include <fstream>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "post/rgb24_postprocess.h"
#include "math/vec3.h"
#include "math/ray.h"
#include "math/morton.h"
#include "math/stratified.h"
#include "math/frustum.h"
#include "math/pcg_random.h"

#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "primitives/primitive.h"
#include "material/material.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "scene/light.h"
#include "scene/cornell_box.h"
#include "renderer/renderer.h"
#include "renderer/render_analysis.h"

// Conditionally include GPU renderer
#ifdef GPU_RENDERING
#include "renderer/gpu_renderer.h"

// Renderer type selection
enum class RendererType {
    CPU,
    GPU
};
#endif


// Quality presets (resolution, samples, max_depth)
struct QualityPreset {
    int width;
    int samples;
    int max_depth;
    const char* name;
};

QualityPreset quality_levels[] = {
    {960, 4, 3, "Large Window (Default)"},  // Medium resolution (960x540) with large window
    {640, 1, 3, "Low (Fast)"},
    {800, 4, 3, "Medium"},
    {1280, 16, 5, "High"},
    {1600, 32, 5, "Ultra"},
    {2560, 1, 3, "Maximum Quality"}
};

const int NUM_QUALITY_LEVELS = 6;

// Estimate memory usage for a render
int estimate_memory_mb(int width, int height, int samples) {
    int pixels = width * height;
    int rays = pixels * samples;
    // Rough estimate: ~100 bytes per ray (framebuffer + overhead)
    return (rays * 100) / (1024 * 1024);
}

// Check if quality setting is safe
bool is_quality_safe(const QualityPreset& preset) {
    int height = preset.width * 9 / 16;
    int pixels = preset.width * height;
    int rays = pixels * preset.samples;

    // Warn if more than 50 million rays
    if (rays > 50000000) {
        return false;
    }

    // Warn if estimated memory > 500MB
    int memory_mb = estimate_memory_mb(preset.width, height, preset.samples);
    if (memory_mb > 500) {
        return false;
    }

    return true;
}

// Check if samples per pixel is safe for current resolution
bool is_samples_safe(int samples, int width) {
    int height = width * 9 / 16;
    int pixels = width * height;
    int rays = pixels * samples;

    // Conservative limit for interactive use: max 50M rays
    if (rays > 50000000) {
        return false;
    }

    return true;
}

// Validate current settings are safe, returns true if safe
bool validate_current_settings(const QualityPreset& preset) {
    if (!is_samples_safe(preset.samples, preset.width)) {
        std::cout << "\nWARNING: DANGER: Current settings are UNSAFE!\n";
        std::cout << "  Resolution: " << preset.width << "x" << (preset.width * 9 / 16) << "\n";
        std::cout << "  Samples: " << preset.samples << " (must be < 8)\n";
        std::cout << "  This combination may crash the application.\n";
        std::cout << "  Please reduce samples or resolution.\n\n";
        return false;
    }
    return true;
}

// Read back the stretched scene from the renderer right after it is drawn (no histogram, panel, or help).
// Full output size — the scene was already drawn across the whole target; UI is not composited yet.
// Pixel format: never pass non-RGBA layouts straight to stbi_write_png (ARGB/BGRA order differs per backend).
static bool save_window_scene_readback_png(SDL_Renderer* renderer, const char* filename) {
    int out_w = 0, out_h = 0;
    if (SDL_GetRendererOutputSize(renderer, &out_w, &out_h) != 0 || out_w <= 0 || out_h <= 0) {
        std::cerr << "Screenshot failed: SDL_GetRendererOutputSize: " << SDL_GetError() << "\n";
        return false;
    }
    const SDL_Rect clip{0, 0, out_w, out_h};
    const size_t n = static_cast<size_t>(out_w) * static_cast<size_t>(out_h);
    const int pitch4 = out_w * 4;

    // Prefer packed RGB (matches on-screen color without channel reorder bugs).
    {
        std::vector<unsigned char> rgb(n * 3u);
        const int pitch3 = out_w * 3;
        if (SDL_RenderReadPixels(renderer, &clip, SDL_PIXELFORMAT_RGB24, rgb.data(), pitch3) == 0) {
            stbi_write_png(filename, out_w, out_h, 3, rgb.data(), pitch3);
            std::cout << "Screenshot saved: " << filename << " (" << out_w << "x" << out_h << ")\n";
            return true;
        }
    }
    SDL_ClearError();

    std::vector<unsigned char> src(n * 4u);
    const Uint32 try_read[] = {
        SDL_PIXELFORMAT_RGBA8888,
        SDL_PIXELFORMAT_BGRA8888,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_PIXELFORMAT_ABGR8888,
    };
    for (Uint32 fmt : try_read) {
        if (SDL_RenderReadPixels(renderer, &clip, fmt, src.data(), pitch4) != 0) {
            SDL_ClearError();
            continue;
        }
        if (fmt == SDL_PIXELFORMAT_RGBA8888) {
            stbi_write_png(filename, out_w, out_h, 4, src.data(), pitch4);
            std::cout << "Screenshot saved: " << filename << " (" << out_w << "x" << out_h << ")\n";
            return true;
        }
        std::vector<unsigned char> rgba(n * 4u);
        if (SDL_ConvertPixels(out_w, out_h, fmt, src.data(), pitch4, SDL_PIXELFORMAT_RGBA8888, rgba.data(), pitch4) !=
            0) {
            SDL_ClearError();
            continue;
        }
        stbi_write_png(filename, out_w, out_h, 4, rgba.data(), pitch4);
        std::cout << "Screenshot saved: " << filename << " (" << out_w << "x" << out_h << ")\n";
        return true;
    }
    std::cerr << "Screenshot failed: SDL_RenderReadPixels: " << SDL_GetError() << "\n";
    return false;
}

// Camera controller
class CameraController {
public:
    Point3 position;
    Point3 lookat;
    Vec3 vup;
    float vfov;
    float aspect_ratio;
    float aperture;
    float dist_to_focus;

    float yaw;   // Horizontal rotation
    float pitch; // Vertical rotation

public:
    CameraController()
        : position(0, 0, 8), lookat(0, 0, 0), vup(0, 1, 0),
          vfov(60), aspect_ratio(16.0f / 9.0f), aperture(0.0f),
          dist_to_focus(3.0f), yaw(-90.0f), pitch(0.0f) {
        update_from_angles();
    }

    void update_from_angles() {
        // Convert spherical coordinates to cartesian
        float yaw_rad = yaw * M_PI / 180.0f;
        float pitch_rad = pitch * M_PI / 180.0f;

        Vec3 direction;
        direction.x = cos(yaw_rad) * cos(pitch_rad);
        direction.y = sin(pitch_rad);
        direction.z = sin(yaw_rad) * cos(pitch_rad);

        lookat = position + direction;
    }

    void move_forward(float delta) {
        Vec3 forward = (lookat - position).normalized();
        position = position + forward * delta;
        update_from_angles();
    }

    void move_right(float delta) {
        Vec3 forward = (lookat - position).normalized();
        Vec3 right = cross(forward, vup).normalized();
        position = position + right * delta;
        update_from_angles();
    }

    void move_up(float delta) {
        position.y += delta;
        update_from_angles();
    }

    void rotate(float delta_yaw, float delta_pitch) {
        yaw += delta_yaw;
        pitch += delta_pitch;

        // Clamp pitch to avoid gimbal lock
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        update_from_angles();
    }

    Camera get_camera() const {
        return Camera(position, lookat, vup, vfov, aspect_ratio, aperture, dist_to_focus);
    }

    Point3 get_position() const { return position; }

    // Apply eye / look-at from a simple text file (hot reload). Updates yaw/pitch via update_from_angles().
    bool try_load_hot_reload_file(const char* path) {
        std::ifstream in(path);
        if (!in) {
            return false;
        }
        bool have_eye = false;
        bool have_at = false;
        Point3 eye = position;
        Point3 at = lookat;
        Vec3 up = vup;
        float new_vfov = vfov;
        float new_aperture = aperture;
        float new_focus = dist_to_focus;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream iss(line);
            std::string key;
            iss >> key;
            if (key == "eye" || key == "lookfrom") {
                iss >> eye.x >> eye.y >> eye.z;
                have_eye = true;
            } else if (key == "at" || key == "lookat") {
                iss >> at.x >> at.y >> at.z;
                have_at = true;
            } else if (key == "up" || key == "vup") {
                iss >> up.x >> up.y >> up.z;
            } else if (key == "vfov") {
                iss >> new_vfov;
            } else if (key == "aperture") {
                iss >> new_aperture;
            } else if (key == "focus" || key == "dist_to_focus") {
                iss >> new_focus;
            }
        }
        if (!have_eye || !have_at) {
            return false;
        }
        position = eye;
        lookat = at;
        vup = up;
        vfov = new_vfov;
        aperture = new_aperture;
        dist_to_focus = new_focus;
        Vec3 d = lookat - position;
        const float len = d.length();
        if (len > 1e-6f) {
            d = d * (1.0f / len);
            pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f)) * 180.0f / static_cast<float>(M_PI);
            yaw = std::atan2(d.z, d.x) * 180.0f / static_cast<float>(M_PI);
        }
        update_from_angles();
        std::cout << "Hot reload: camera updated from \"" << path << "\"\n";
        return true;
    }
};

// Help overlay system
class HelpOverlay {
private:
    TTF_Font* font;
    TTF_Font* title_font;
    SDL_Color text_color;
    SDL_Color background_color;
    SDL_Color title_color;
    bool initialized;

public:
    HelpOverlay() : font(nullptr), title_font(nullptr), initialized(false) {
        // Match controls panel: dark translucent chrome + light text + cyan title
        text_color = {210, 215, 225, 255};
        background_color = {50, 55, 65, 230};
        title_color = {100, 200, 255, 255};
    }

    bool init() {
        if (initialized) return true;

        if (TTF_Init() == -1) {
            std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
            return false;
        }

        // Try to load a system font
        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
            "/System/Library/Fonts/Menlo.ttc",
            "/System/Library/Fonts/Courier.dfont",
            "C:\\Windows\\Fonts\\consola.ttf",
            nullptr
        };

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            font = TTF_OpenFont(font_paths[i], 14);  // Match controls panel body
            if (font) break;
        }

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            title_font = TTF_OpenFont(font_paths[i], 16);  // Match controls panel title
            if (title_font) break;
        }

        if (!font || !title_font) {
            std::cerr << "Failed to load fonts, help overlay unavailable" << std::endl;
            return false;
        }

        initialized = true;
        return true;
    }

    bool has_overlay_fonts() const {
        return initialized && font && title_font;
    }
    TTF_Font* overlay_body_font() const {
        return font;
    }
    TTF_Font* overlay_title_font() const {
        return title_font;
    }

    void render(SDL_Renderer* renderer, int window_width, int window_height) {
        if (!initialized || !font || !title_font) return;

        const char* title_text = "HELP";
        const char* controls_text[] = {
            "Click window to capture mouse for looking around",
            "MOVE: WASD + Arrows | LOOK: Mouse",
            "1-6: Quality | M: Analysis | SPACE: Pause",
            "G: Diagnostics (histogram + stats) | F5: Reload camera file (RT_HOT_RELOAD_CAMERA)",
            "S: Window screenshot (no UI) | C: Controls Panel | H: Help | ESC: Quit"
        };
        const char* footer = "Press H to close";

        auto text_pixel_width = [](TTF_Font* f, const char* s) -> int {
            int w = 0, h = 0;
            if (!s || TTF_SizeText(f, s, &w, &h) != 0)
                return 0;
            return w;
        };

        int max_line_w = text_pixel_width(title_font, title_text);
        for (size_t i = 0; i < sizeof(controls_text) / sizeof(controls_text[0]); ++i) {
            max_line_w = std::max(max_line_w, text_pixel_width(font, controls_text[i]));
        }
        max_line_w = std::max(max_line_w, text_pixel_width(font, footer));

        const int horizontal_pad = 48;
        const int help_w = std::max(100, std::min(window_width - 40, max_line_w + horizontal_pad));
        const int help_h = 400;

        SDL_Rect overlay_rect = {(window_width - help_w) / 2, (window_height - help_h) / 2, help_w, help_h};

        SDL_Surface* surface =
            SDL_CreateRGBSurfaceWithFormat(0, overlay_rect.w, overlay_rect.h, 32, SDL_PIXELFORMAT_RGBA8888);
        if (!surface) return;

        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 50, 55, 65, 230));
        SDL_Rect help_sep = {10, 34, help_w - 20, 1};
        SDL_FillRect(surface, &help_sep, SDL_MapRGBA(surface->format, 100, 100, 120, 200));

        // Render title (left-aligned like settings)
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {15, 10, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        int y_offset = 48;
        const int line_skip = 28;
        for (size_t i = 0; i < sizeof(controls_text) / sizeof(controls_text[0]); ++i) {
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, controls_text[i], text_color);

            if (text_surface) {
                SDL_Rect text_rect = {15, y_offset, text_surface->w, text_surface->h};
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }
            y_offset += line_skip;
        }

        SDL_Surface* footer_surface = TTF_RenderText_Blended(font, footer, text_color);
        if (footer_surface) {
            SDL_Rect footer_rect = {15, help_h - footer_surface->h - 14, footer_surface->w, footer_surface->h};
            SDL_BlitSurface(footer_surface, nullptr, surface, &footer_rect);
            SDL_FreeSurface(footer_surface);
        }

        // Convert surface to texture
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, &overlay_rect);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
    }

    ~HelpOverlay() {
        if (font) TTF_CloseFont(font);
        if (title_font) TTF_CloseFont(title_font);
        if (initialized) TTF_Quit();
    }
};

class ControlsPanel {
private:
    TTF_Font* font;
    TTF_Font* title_font;
    SDL_Color text_color;
    SDL_Color background_color;
    SDL_Color title_color;
    SDL_Color value_color;
    SDL_Color button_color;
    SDL_Color button_hover_color;
    SDL_Color button_active_color;
    bool initialized;

    // Button regions for click detection
    struct Button {
        SDL_Rect rect;
        std::string label;
        int value;
        int category; // 0: quality, 1: samples, 2: depth, 3: shadows, 4: reflections, 5: resolution, 6: debug,
                      // 16 denoise toggle, 17 min prog, 18 denoise strength (value 0–2),
                      // 19 DOF aperture (milli-radius), 20 DOF focus distance (hundredths),
                      // 21 denoise amount slider (0-100)
    };
    std::vector<Button> buttons;
    int panel_x, panel_y;
    // Last overlay size from render() — needed so hit tests match SDL_RenderCopy (non-scrollable stretch).
    int panel_layout_w, panel_layout_h;
    // Screenshot button rect in panel content coordinates (for pressed-state hit test after layout).
    int screenshot_hit_x, screenshot_hit_y, screenshot_hit_w, screenshot_hit_h;
    bool screenshot_button_pressed;

    // Scroll functionality
    int scroll_offset;
    int max_scroll_offset;
    int content_height;
    bool is_scrollable;

    SDL_Texture* panel_snap_tex;
    uint64_t panel_snap_key;
    int panel_snap_w;
    int panel_snap_h;

    uint64_t panel_state_hash(int window_width, int window_height, int quality_idx, const QualityPreset& preset,
                              double fps, double render_time, const char* analysis_mode_name,
                              bool enable_shadows, bool enable_reflections, bool enable_progressive, bool enable_adaptive,
                              bool enable_denoiser, int denoise_strength, int denoise_amount_percent,
                              int min_progressive_display_passes,
                              bool enable_wavefront,
                              bool enable_morton, bool enable_stratified, bool enable_frustum, bool enable_simd_packets,
                              bool enable_bvh, float dof_lens_radius, float dof_focus_dist
#ifdef GPU_RENDERING
                              , RendererType current_renderer
#endif
                              ) const {
        auto mix = [](uint64_t& h, uint64_t v) {
            h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        };
        uint64_t h = 1469598103934665603ULL;
        mix(h, static_cast<uint64_t>(window_width));
        mix(h, static_cast<uint64_t>(window_height));
        mix(h, static_cast<uint64_t>(quality_idx));
        mix(h, static_cast<uint64_t>(preset.width));
        mix(h, static_cast<uint64_t>(preset.samples));
        mix(h, static_cast<uint64_t>(preset.max_depth));
        mix(h, static_cast<uint64_t>(std::lround(fps)));
        mix(h, static_cast<uint64_t>(std::lround(render_time * 50.0)));
        if (analysis_mode_name) {
            mix(h, static_cast<uint64_t>(static_cast<unsigned char>(analysis_mode_name[0])));
        }
        mix(h, enable_shadows ? 3u : 1u);
        mix(h, enable_reflections ? 3u : 1u);
        mix(h, enable_progressive ? 3u : 1u);
        mix(h, enable_adaptive ? 3u : 1u);
        mix(h, enable_denoiser ? 3u : 1u);
        mix(h, static_cast<uint64_t>(denoise_strength + 16));
        mix(h, static_cast<uint64_t>(denoise_amount_percent + 100));
        mix(h, static_cast<uint64_t>(min_progressive_display_passes));
        mix(h, enable_wavefront ? 3u : 1u);
        mix(h, enable_morton ? 3u : 1u);
        mix(h, enable_stratified ? 3u : 1u);
        mix(h, enable_frustum ? 3u : 1u);
        mix(h, enable_simd_packets ? 3u : 1u);
        mix(h, enable_bvh ? 3u : 1u);
        mix(h, static_cast<uint64_t>(std::lround(static_cast<double>(dof_lens_radius) * 1000.0)));
        mix(h, static_cast<uint64_t>(std::lround(static_cast<double>(dof_focus_dist) * 100.0)));
        mix(h, static_cast<uint64_t>(scroll_offset));
#ifdef GPU_RENDERING
        mix(h, static_cast<uint64_t>(current_renderer == RendererType::GPU ? 2 : 1));
#endif
        return h;
    }

public:
    ControlsPanel()
        : font(nullptr), title_font(nullptr), initialized(false), panel_x(0), panel_y(0), panel_layout_w(0),
          panel_layout_h(0), screenshot_hit_x(0), screenshot_hit_y(0), screenshot_hit_w(0), screenshot_hit_h(0),
          screenshot_button_pressed(false),
          scroll_offset(0), max_scroll_offset(0), content_height(0), is_scrollable(false),
          panel_snap_tex(nullptr), panel_snap_key(0), panel_snap_w(0), panel_snap_h(0) {
        text_color = {20, 20, 20, 255};
        background_color = {50, 50, 60, 230};  // Dark blue-gray
        title_color = {100, 200, 255, 255};     // Light blue
        value_color = {255, 200, 100, 255};     // Orange
        button_color = {70, 70, 90, 255};
        button_hover_color = {90, 90, 110, 255};
        button_active_color = {100, 150, 200, 255};
    }

    bool init() {
        if (initialized) return true;

        if (TTF_Init() == -1) {
            std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
            return false;
        }

        // Try to load a system font
        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
            "/System/Library/Fonts/Menlo.ttc",
            "/System/Library/Fonts/Courier.dfont",
            "C:\\Windows\\Fonts\\consola.ttf",
            nullptr
        };

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            font = TTF_OpenFont(font_paths[i], 14);
            if (font) break;
        }

        for (int i = 0; font_paths[i] != nullptr; ++i) {
            title_font = TTF_OpenFont(font_paths[i], 16);
            if (title_font) break;
        }

        if (!font || !title_font) {
            std::cerr << "Failed to load fonts, controls panel unavailable" << std::endl;
            return false;
        }

        initialized = true;
        return true;
    }

    bool has_overlay_fonts() const {
        return initialized && font && title_font;
    }
    TTF_Font* overlay_body_font() const {
        return font;
    }
    TTF_Font* overlay_title_font() const {
        return title_font;
    }

    bool point_is_over_panel(int mouse_x, int mouse_y, int window_width, int window_height) const {
        int panel_width = std::min(360, std::max(1, window_width - 20));
        int panel_height = std::max(1, window_height - 20);
        int px = window_width - panel_width - 10;
        int py = 10;
        return mouse_x >= px && mouse_x < px + panel_width &&
               mouse_y >= py && mouse_y < py + panel_height;
    }

    void render(SDL_Renderer* renderer, int window_width, int window_height,
                int quality_idx, const QualityPreset& preset, double fps, double render_time,
                const char* analysis_mode_name = nullptr, bool enable_shadows = true, bool enable_reflections = true,
                bool enable_progressive = false, bool enable_adaptive = false, bool enable_denoiser = false,
                int denoise_strength = 0,
                int denoise_amount_percent = 70,
                int min_progressive_display_passes = 1,
                bool enable_wavefront = false,
                bool enable_morton = false, bool enable_stratified = false, bool enable_frustum = false,
                bool enable_simd_packets = false, bool enable_bvh = false,
                float dof_lens_radius = 0.0f, float dof_focus_dist = 3.0f
#ifdef GPU_RENDERING
                , RendererType current_renderer = RendererType::CPU
#endif
    ) {
        if (!initialized || !font || !title_font) return;

        // Sidebar: fixed top/right margins; full window height minus vertical margin
        int panel_width = std::min(360, std::max(1, window_width - 20));
        int panel_height = std::max(1, window_height - 20);
        panel_x = window_width - panel_width - 10;
        panel_y = 10;
        panel_layout_w = panel_width;
        panel_layout_h = panel_height;
        SDL_Rect overlay_rect = {panel_x, panel_y, panel_width, panel_height};

        const uint64_t hkey = panel_state_hash(window_width, window_height, quality_idx, preset, fps, render_time,
            analysis_mode_name, enable_shadows, enable_reflections, enable_progressive, enable_adaptive, enable_denoiser,
            denoise_strength, denoise_amount_percent, min_progressive_display_passes, enable_wavefront, enable_morton, enable_stratified,
            enable_frustum, enable_simd_packets, enable_bvh, dof_lens_radius, dof_focus_dist
#ifdef GPU_RENDERING
            , current_renderer
#endif
        );
        // Do not short-circuit with a cached texture: skipping the layout pass leaves `buttons` stale vs
        // `panel_x`/`panel_y` (e.g. after resize) and breaks hit testing. Rebuild the panel every frame.

        // Scrollable document surface (cap size to reduce allocation on very tall windows)
        int content_surface_height = std::min(3200, std::max(1800, panel_height + 1200));
        SDL_Surface* content_surface =
            SDL_CreateRGBSurfaceWithFormat(0, panel_width, content_surface_height, 32, SDL_PIXELFORMAT_RGBA8888);
        if (!content_surface) return;

        SDL_FillRect(content_surface, nullptr, SDL_MapRGBA(content_surface->format, 0, 0, 0, 0));
        SDL_FillRect(content_surface, nullptr, SDL_MapRGBA(content_surface->format, 50, 50, 60, 135));

        // Render title
        const char* title_text = "SETTINGS";
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {15, 10, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, content_surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        // Draw separator line
        SDL_Rect separator = {10, 35, panel_width - 20, 1};
        SDL_FillRect(content_surface, &separator, SDL_MapRGBA(content_surface->format, 100, 100, 120, 200));

        int y_offset = 50;
        const int line_height = 28;
        buttons.clear(); // Clear previous buttons

        // Helper lambda to render label-value pairs
        auto render_setting = [&](const char* label, const char* value) {
            SDL_Surface* label_surface = TTF_RenderText_Blended(font, label, text_color);
            if (label_surface) {
                SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
                SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
                SDL_FreeSurface(label_surface);
            }

            SDL_Surface* value_surface = TTF_RenderText_Blended(font, value, value_color);
            if (value_surface) {
                SDL_Rect value_rect = {panel_width - value_surface->w - 15, y_offset, value_surface->w, value_surface->h};
                SDL_BlitSurface(value_surface, nullptr, content_surface, &value_rect);
                SDL_FreeSurface(value_surface);
            }

            y_offset += line_height;
        };

        // Render current settings
        char fps_str[32], time_str[32];
        snprintf(fps_str, sizeof(fps_str), "%.1f FPS", fps);
        snprintf(time_str, sizeof(time_str), "%.3f s", render_time);

        render_setting("Quality:", preset.name);
        render_setting("Resolution:", (std::to_string(preset.width) + "x" + std::to_string(preset.width * 9 / 16)).c_str());
        render_setting("Samples:", std::to_string(preset.samples).c_str());
        render_setting("Depth:", std::to_string(preset.max_depth).c_str());
        render_setting("FPS:", fps_str);

        // Always show analysis mode
        const char* current_analysis = analysis_mode_name ? analysis_mode_name : "None";
        render_setting("Analysis:", current_analysis);

#ifdef GPU_RENDERING
        // Show current renderer mode
        const char* renderer_mode = (current_renderer == RendererType::GPU) ? "GPU" : "CPU";
        SDL_Color renderer_color = (current_renderer == RendererType::GPU) ?
            SDL_Color({50, 200, 50, 255}) :  // Green for GPU
            SDL_Color({200, 100, 50, 255}); // Orange for CPU

        SDL_Surface* renderer_surface = TTF_RenderText_Blended(font, "Renderer:", title_color);
        if (renderer_surface) {
            SDL_Rect renderer_rect = {15, y_offset, renderer_surface->w, renderer_surface->h};
            SDL_BlitSurface(renderer_surface, nullptr, content_surface, &renderer_rect);
            SDL_FreeSurface(renderer_surface);

            // Render the actual mode with color
            SDL_Surface* mode_surface = TTF_RenderText_Blended(font, renderer_mode, renderer_color);
            if (mode_surface) {
                SDL_Rect mode_rect = {70, y_offset, mode_surface->w, mode_surface->h};
                SDL_BlitSurface(mode_surface, nullptr, content_surface, &mode_rect);
                SDL_FreeSurface(mode_surface);
            }
            y_offset += 22;
        }
#endif

        y_offset += 10;

        // Helper lambda to render a button
        auto render_button = [&](const char* label, int value, int category, bool is_active) {
            int button_width = 50;
            int button_height = 24;
            int button_spacing = 5;
            static int button_x = 15;
            static int buttons_in_row = 0;

            if (buttons.empty() || buttons.back().category != category) {
                button_x = 15;
                buttons_in_row = 0;
            }

            SDL_Rect button_rect = {button_x, y_offset, button_width, button_height};

            // Store button for click detection
            buttons.push_back({{button_rect.x + panel_x, button_rect.y + panel_y, button_rect.w, button_rect.h},
                              label, value, category});

            // Draw button background
            Uint32 button_bg;
            if (is_active) {
                button_bg = SDL_MapRGBA(content_surface->format, 100, 150, 200, 235);
            } else {
                button_bg = SDL_MapRGBA(content_surface->format, 70, 70, 90, 235);
            }
            SDL_FillRect(content_surface, &button_rect, button_bg);

            // Draw button border
            SDL_Rect border = {button_rect.x, button_rect.y, button_rect.w, 2};
            SDL_FillRect(content_surface, &border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
            border = {button_rect.x, button_rect.y + button_rect.h - 2, button_rect.w, 2};
            SDL_FillRect(content_surface, &border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

            // Draw button text
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, label, text_color);
            if (text_surface) {
                SDL_Rect text_rect = {
                    button_x + (button_width - text_surface->w) / 2,
                    y_offset + (button_height - text_surface->h) / 2,
                    text_surface->w, text_surface->h
                };
                SDL_BlitSurface(text_surface, nullptr, content_surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }

            button_x += button_width + button_spacing;
            buttons_in_row++;

            if (buttons_in_row >= 7) {
                button_x = 15;
                buttons_in_row = 0;
                y_offset += button_height + button_spacing;
            }
        };

        // Quality level buttons
        SDL_Surface* label_surface = TTF_RenderText_Blended(font, "Quality Level:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        for (int i = 0; i < 6; i++) {
            char label[8];
            snprintf(label, sizeof(label), "%d", i + 1);
            render_button(label, i, 0, i == quality_idx);
        }
        y_offset += 35;

        // Samples per pixel buttons
        label_surface = TTF_RenderText_Blended(font, "Samples Per Pixel:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        // Samples per pixel options
        int sample_values[] = {1, 4, 8, 16};
        for (int s : sample_values) {
            char label[8];
            if (s >= 100) {
                snprintf(label, sizeof(label), "%d", s);
            } else {
                snprintf(label, sizeof(label), "%d", s);
            }
            bool is_active = (preset.samples == s);
            render_button(label, s, 1, is_active);
        }
        y_offset += 35;

        // Max depth buttons
        label_surface = TTF_RenderText_Blended(font, "Max Depth:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        int depth_values[] = {1, 3, 5, 8};
        for (int d : depth_values) {
            char label[8];
            snprintf(label, sizeof(label), "%d", d);
            bool is_active = (preset.max_depth == d);
            render_button(label, d, 2, is_active);
        }
        y_offset += 35;

        // Resolution buttons
        label_surface = TTF_RenderText_Blended(font, "Resolution:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        int resolution_values[] = {640, 960, 1280, 1600, 1920};
        const char* resolution_names[] = {"Low", "Medium", "High", "Ultra", "Max"};

        for (int i = 0; i < 5; i++) {
            bool is_active = (preset.width == resolution_values[i]);
            render_button(resolution_names[i], resolution_values[i], 5, is_active);
        }
        y_offset += 35;

        // Depth of field (thin lens; lens radius == aperture, matches GPU-style controls)
        label_surface = TTF_RenderText_Blended(font, "DOF lens radius:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        const int active_ap_milli = static_cast<int>(std::lround(static_cast<double>(dof_lens_radius) * 1000.0));
        struct {
            const char* lab;
            int milli;
        } dof_ap[] = {{"0", 0}, {".02", 20}, {".05", 50}, {".10", 100}, {".15", 150}, {".25", 250}};
        for (const auto& e : dof_ap) {
            render_button(e.lab, e.milli, 19, active_ap_milli == e.milli);
        }
        y_offset += 35;

        label_surface = TTF_RenderText_Blended(font, "DOF focus dist:", title_color);
        if (label_surface) {
            SDL_Rect lr = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &lr);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        const int active_focus_hund = static_cast<int>(std::lround(static_cast<double>(dof_focus_dist) * 100.0));
        struct {
            const char* lab;
            int hund;
        } dof_fd[] = {
            {"1", 100}, {"1.5", 150}, {"2", 200}, {"3", 300}, {"5", 500}, {"8", 800}, {"12", 1200}};
        for (const auto& e : dof_fd) {
            render_button(e.lab, e.hund, 20, active_focus_hund == e.hund);
        }
        y_offset += 35;

        // Render Features section
        label_surface = TTF_RenderText_Blended(font, "Render Features:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        // Shadows toggle button
        const char* shadows_label = enable_shadows ? "Shadows: ON" : "Shadows: OFF";
        int shadows_button_width = 110;
        SDL_Rect shadows_button_rect = {15, y_offset, shadows_button_width, 24};

        // Store button for click detection (category 3 = shadows)
        buttons.push_back({{shadows_button_rect.x + panel_x, shadows_button_rect.y + panel_y, shadows_button_rect.w, shadows_button_rect.h},
                          "shadows", enable_shadows ? 1 : 0, 3});

        // Draw shadows button background
        Uint32 shadows_button_bg = SDL_MapRGBA(content_surface->format,
            enable_shadows ? button_active_color.r : button_color.r,
            enable_shadows ? button_active_color.g : button_color.g,
            enable_shadows ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &shadows_button_rect, shadows_button_bg);

        // Draw shadows button border
        SDL_Rect shadows_border = {shadows_button_rect.x, shadows_button_rect.y, shadows_button_rect.w, 2};
        SDL_FillRect(content_surface, &shadows_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        shadows_border = {shadows_button_rect.x, shadows_button_rect.y + shadows_button_rect.h - 2, shadows_button_rect.w, 2};
        SDL_FillRect(content_surface, &shadows_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw shadows button text
        SDL_Surface* shadows_text_surface = TTF_RenderText_Blended(font, shadows_label, text_color);
        if (shadows_text_surface) {
            SDL_Rect shadows_text_rect = {
                shadows_button_rect.x + (shadows_button_width - shadows_text_surface->w) / 2,
                y_offset + (24 - shadows_text_surface->h) / 2,
                shadows_text_surface->w, shadows_text_surface->h
            };
            SDL_BlitSurface(shadows_text_surface, nullptr, content_surface, &shadows_text_rect);
            SDL_FreeSurface(shadows_text_surface);
        }

        // Reflections toggle button (next to shadows)
        const char* reflections_label = enable_reflections ? "Reflections: ON" : "Reflections: OFF";
        int reflections_button_width = 130;
        SDL_Rect reflections_button_rect = {140, y_offset, reflections_button_width, 24};

        // Store button for click detection (category 4 = reflections)
        buttons.push_back({{reflections_button_rect.x + panel_x, reflections_button_rect.y + panel_y, reflections_button_rect.w, reflections_button_rect.h},
                          "reflections", enable_reflections ? 1 : 0, 4});

        // Draw reflections button background
        Uint32 reflections_button_bg = SDL_MapRGBA(content_surface->format,
            enable_reflections ? button_active_color.r : button_color.r,
            enable_reflections ? button_active_color.g : button_color.g,
            enable_reflections ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &reflections_button_rect, reflections_button_bg);

        // Draw reflections button border
        SDL_Rect reflections_border = {reflections_button_rect.x, reflections_button_rect.y, reflections_button_rect.w, 2};
        SDL_FillRect(content_surface, &reflections_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        reflections_border = {reflections_button_rect.x, reflections_button_rect.y + reflections_button_rect.h - 2, reflections_button_rect.w, 2};
        SDL_FillRect(content_surface, &reflections_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw reflections button text
        SDL_Surface* reflections_text_surface = TTF_RenderText_Blended(font, reflections_label, text_color);
        if (reflections_text_surface) {
            SDL_Rect reflections_text_rect = {
                reflections_button_rect.x + (reflections_button_width - reflections_text_surface->w) / 2,
                y_offset + (24 - reflections_text_surface->h) / 2,
                reflections_text_surface->w, reflections_text_surface->h
            };
            SDL_BlitSurface(reflections_text_surface, nullptr, content_surface, &reflections_text_rect);
            SDL_FreeSurface(reflections_text_surface);
        }

        y_offset += 35;

        // Screenshot button (new row)
        label_surface = TTF_RenderText_Blended(font, "Screenshot:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        const char* screenshot_label = "Save Screenshot";
        int screenshot_button_width = 120;
        SDL_Rect screenshot_button_rect = {15, y_offset, screenshot_button_width, 24};
        screenshot_hit_x = screenshot_button_rect.x;
        screenshot_hit_y = screenshot_button_rect.y;
        screenshot_hit_w = screenshot_button_rect.w;
        screenshot_hit_h = screenshot_button_rect.h;

        // Store button for click detection (category 7 = screenshot)
        buttons.push_back({{screenshot_button_rect.x + panel_x, screenshot_button_rect.y + panel_y, screenshot_button_rect.w, screenshot_button_rect.h},
                          "screenshot", 1, 7});

        // Draw screenshot button background (make it brighter like the "on" toggle state)
        Uint32 screenshot_button_bg =
            SDL_MapRGBA(content_surface->format, button_active_color.r, button_active_color.g, button_active_color.b, 232);
        SDL_FillRect(content_surface, &screenshot_button_rect, screenshot_button_bg);

        // Draw screenshot button border
        SDL_Rect screenshot_border = {screenshot_button_rect.x, screenshot_button_rect.y, screenshot_button_rect.w, 2};
        SDL_FillRect(content_surface, &screenshot_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        screenshot_border = {screenshot_button_rect.x, screenshot_button_rect.y + screenshot_button_rect.h - 2, screenshot_button_rect.w, 2};
        SDL_FillRect(content_surface, &screenshot_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw screenshot button text
        SDL_Surface* screenshot_text_surface = TTF_RenderText_Blended(font, screenshot_label, text_color);
        if (screenshot_text_surface) {
            SDL_Rect screenshot_text_rect = {
                screenshot_button_rect.x + (screenshot_button_width - screenshot_text_surface->w) / 2,
                y_offset + (24 - screenshot_text_surface->h) / 2,
                screenshot_text_surface->w, screenshot_text_surface->h
            };
            SDL_BlitSurface(screenshot_text_surface, nullptr, content_surface, &screenshot_text_rect);
            SDL_FreeSurface(screenshot_text_surface);
        }

        y_offset += 35;

        // Debug features section
        label_surface = TTF_RenderText_Blended(font, "Debug Features:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        // Individual analysis mode buttons
        const char* analysis_modes[] = {"None", "Normals", "Depth", "Albedo"};
        AnalysisMode modes[] = {
            AnalysisMode::NORMAL,
            AnalysisMode::NORMALS,
            AnalysisMode::DEPTH,
            AnalysisMode::ALBEDO
        };

        // Determine current mode from mode_name
        AnalysisMode current_mode = AnalysisMode::NORMAL;
        if (analysis_mode_name) {
            for (int i = 0; i < 4; i++) {
                if (strcmp(analysis_mode_name, analysis_modes[i]) == 0) {
                    current_mode = modes[i];
                    break;
                }
            }
        }

        // Manually layout debug buttons in 1 row
        int button_width = 80;
        int button_height = 24;
        int button_spacing = 5;
        int start_x = 15;

        // Single row: None, Normals, Depth, Albedo
        for (int i = 0; i < 4; i++) {
            bool is_active = (current_mode == modes[i]);
            SDL_Rect button_rect = {start_x + i * (button_width + button_spacing), y_offset, button_width, button_height};

            // Store button for click detection (category 6 = debug modes)
            buttons.push_back({{button_rect.x + panel_x, button_rect.y + panel_y, button_rect.w, button_rect.h},
                              analysis_modes[i], static_cast<int>(modes[i]), 6});

            // Draw button background
            Uint32 button_bg = SDL_MapRGBA(content_surface->format,
                is_active ? button_active_color.r : button_color.r,
                is_active ? button_active_color.g : button_color.g,
                is_active ? button_active_color.b : button_color.b,
                232);
            SDL_FillRect(content_surface, &button_rect, button_bg);

            // Draw button border
            SDL_Rect border = {button_rect.x, button_rect.y, button_rect.w, 2};
            SDL_FillRect(content_surface, &border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
            border = {button_rect.x, button_rect.y + button_rect.h - 2, button_rect.w, 2};
            SDL_FillRect(content_surface, &border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

            // Draw button text
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, analysis_modes[i], text_color);
            if (text_surface) {
                SDL_Rect text_rect = {
                    button_rect.x + (button_width - text_surface->w) / 2,
                    y_offset + (button_height - text_surface->h) / 2,
                    text_surface->w, text_surface->h
                };
                SDL_BlitSurface(text_surface, nullptr, content_surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }
        }

        // Optimizations: paths, sampling, acceleration (single panel section)
        y_offset += 35;
        label_surface = TTF_RenderText_Blended(font, "Optimizations:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        // Progressive rendering toggle button
        const char* progressive_label = enable_progressive ? "Progressive: ON" : "Progressive: OFF";
        int progressive_button_width = 120;
        SDL_Rect progressive_button_rect = {15, y_offset, progressive_button_width, 24};

        // Store button for click detection (category 8 = progressive)
        buttons.push_back({{progressive_button_rect.x + panel_x, progressive_button_rect.y + panel_y, progressive_button_rect.w, progressive_button_rect.h},
                          "progressive", enable_progressive ? 1 : 0, 8});

        // Draw progressive button background
        Uint32 progressive_button_bg = SDL_MapRGBA(content_surface->format,
            enable_progressive ? button_active_color.r : button_color.r,
            enable_progressive ? button_active_color.g : button_color.g,
            enable_progressive ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &progressive_button_rect, progressive_button_bg);

        // Draw progressive button border
        SDL_Rect progressive_border = {progressive_button_rect.x, progressive_button_rect.y, progressive_button_rect.w, 2};
        SDL_FillRect(content_surface, &progressive_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        progressive_border = {progressive_button_rect.x, progressive_button_rect.y + progressive_button_rect.h - 2, progressive_button_rect.w, 2};
        SDL_FillRect(content_surface, &progressive_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw progressive button text
        SDL_Surface* progressive_text_surface = TTF_RenderText_Blended(font, progressive_label, text_color);
        if (progressive_text_surface) {
            SDL_Rect progressive_text_rect = {
                progressive_button_rect.x + (progressive_button_width - progressive_text_surface->w) / 2,
                y_offset + (24 - progressive_text_surface->h) / 2,
                progressive_text_surface->w, progressive_text_surface->h
            };
            SDL_BlitSurface(progressive_text_surface, nullptr, content_surface, &progressive_text_rect);
            SDL_FreeSurface(progressive_text_surface);
        }

        // Adaptive sampling toggle button (next to progressive)
        const char* adaptive_label = enable_adaptive ? "Adaptive: ON" : "Adaptive: OFF";
        int adaptive_button_width = 120;
        SDL_Rect adaptive_button_rect = {150, y_offset, adaptive_button_width, 24};

        // Store button for click detection (category 9 = adaptive)
        buttons.push_back({{adaptive_button_rect.x + panel_x, adaptive_button_rect.y + panel_y, adaptive_button_rect.w, adaptive_button_rect.h},
                          "adaptive", enable_adaptive ? 1 : 0, 9});

        // Draw adaptive button background
        Uint32 adaptive_button_bg = SDL_MapRGBA(content_surface->format,
            enable_adaptive ? button_active_color.r : button_color.r,
            enable_adaptive ? button_active_color.g : button_color.g,
            enable_adaptive ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &adaptive_button_rect, adaptive_button_bg);

        // Draw adaptive button border
        SDL_Rect adaptive_border = {adaptive_button_rect.x, adaptive_button_rect.y, adaptive_button_rect.w, 2};
        SDL_FillRect(content_surface, &adaptive_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        adaptive_border = {adaptive_button_rect.x, adaptive_button_rect.y + adaptive_button_rect.h - 2, adaptive_button_rect.w, 2};
        SDL_FillRect(content_surface, &adaptive_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw adaptive button text
        SDL_Surface* adaptive_text_surface = TTF_RenderText_Blended(font, adaptive_label, text_color);
        if (adaptive_text_surface) {
            SDL_Rect adaptive_text_rect = {
                adaptive_button_rect.x + (adaptive_button_width - adaptive_text_surface->w) / 2,
                y_offset + (24 - adaptive_text_surface->h) / 2,
                adaptive_text_surface->w, adaptive_text_surface->h
            };
            SDL_BlitSurface(adaptive_text_surface, nullptr, content_surface, &adaptive_text_rect);
            SDL_FreeSurface(adaptive_text_surface);
        }

        y_offset += 35;

        // Morton Z-curve toggle button
        const char* morton_label = enable_morton ? "Morton: ON" : "Morton: OFF";
        int morton_button_width = 120;
        SDL_Rect morton_button_rect = {15, y_offset, morton_button_width, 24};

        // Store button for click detection (category 11 = morton)
        buttons.push_back({{morton_button_rect.x + panel_x, morton_button_rect.y + panel_y, morton_button_rect.w, morton_button_rect.h},
                          "morton", enable_morton ? 1 : 0, 11});

        // Draw morton button background
        Uint32 morton_button_bg = SDL_MapRGBA(content_surface->format,
            enable_morton ? button_active_color.r : button_color.r,
            enable_morton ? button_active_color.g : button_color.g,
            enable_morton ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &morton_button_rect, morton_button_bg);

        // Draw morton button border
        SDL_Rect morton_border = {morton_button_rect.x, morton_button_rect.y, morton_button_rect.w, 2};
        SDL_FillRect(content_surface, &morton_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        morton_border = {morton_button_rect.x, morton_button_rect.y + morton_button_rect.h - 2, morton_button_rect.w, 2};
        SDL_FillRect(content_surface, &morton_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw morton button text
        SDL_Surface* morton_text_surface = TTF_RenderText_Blended(font, morton_label, text_color);
        if (morton_text_surface) {
            SDL_Rect morton_text_rect = {
                morton_button_rect.x + (morton_button_width - morton_text_surface->w) / 2,
                y_offset + (24 - morton_text_surface->h) / 2,
                morton_text_surface->w, morton_text_surface->h
            };
            SDL_BlitSurface(morton_text_surface, nullptr, content_surface, &morton_text_rect);
            SDL_FreeSurface(morton_text_surface);
        }

        // Stratified sampling toggle button (next to morton)
        const char* stratified_label = enable_stratified ? "Stratified: ON" : "Stratified: OFF";
        int stratified_button_width = 120;
        SDL_Rect stratified_button_rect = {150, y_offset, stratified_button_width, 24};

        // Store button for click detection (category 12 = stratified)
        buttons.push_back({{stratified_button_rect.x + panel_x, stratified_button_rect.y + panel_y, stratified_button_rect.w, stratified_button_rect.h},
                          "stratified", enable_stratified ? 1 : 0, 12});

        // Draw stratified button background
        Uint32 stratified_button_bg = SDL_MapRGBA(content_surface->format,
            enable_stratified ? button_active_color.r : button_color.r,
            enable_stratified ? button_active_color.g : button_color.g,
            enable_stratified ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &stratified_button_rect, stratified_button_bg);

        // Draw stratified button border
        SDL_Rect stratified_border = {stratified_button_rect.x, stratified_button_rect.y, stratified_button_rect.w, 2};
        SDL_FillRect(content_surface, &stratified_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        stratified_border = {stratified_button_rect.x, stratified_button_rect.y + stratified_button_rect.h - 2, stratified_button_rect.w, 2};
        SDL_FillRect(content_surface, &stratified_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        // Draw stratified button text
        SDL_Surface* stratified_text_surface = TTF_RenderText_Blended(font, stratified_label, text_color);
        if (stratified_text_surface) {
            SDL_Rect stratified_text_rect = {
                stratified_button_rect.x + (stratified_button_width - stratified_text_surface->w) / 2,
                y_offset + (24 - stratified_text_surface->h) / 2,
                stratified_text_surface->w, stratified_text_surface->h
            };
            SDL_BlitSurface(stratified_text_surface, nullptr, content_surface, &stratified_text_rect);
            SDL_FreeSurface(stratified_text_surface);
        }

        y_offset += 35;

        // Frustum + BVH (independent of SIMD/wavefront packet path)
        const char* frustum_label = enable_frustum ? "Frustum: ON" : "Frustum: OFF";
        int frustum_button_width = 120;
        SDL_Rect frustum_button_rect = {15, y_offset, frustum_button_width, 24};

        // Store button for click detection (category 13 = frustum)
        buttons.push_back({{frustum_button_rect.x + panel_x, frustum_button_rect.y + panel_y, frustum_button_rect.w, frustum_button_rect.h},
                          "frustum", enable_frustum ? 1 : 0, 13});

        Uint32 frustum_button_bg = SDL_MapRGBA(content_surface->format,
            enable_frustum ? button_active_color.r : button_color.r,
            enable_frustum ? button_active_color.g : button_color.g,
            enable_frustum ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &frustum_button_rect, frustum_button_bg);

        SDL_Rect frustum_border = {frustum_button_rect.x, frustum_button_rect.y, frustum_button_rect.w, 2};
        SDL_FillRect(content_surface, &frustum_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        frustum_border = {frustum_button_rect.x, frustum_button_rect.y + frustum_button_rect.h - 2, frustum_button_rect.w, 2};
        SDL_FillRect(content_surface, &frustum_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        SDL_Surface* frustum_text_surface = TTF_RenderText_Blended(font, frustum_label, text_color);
        if (frustum_text_surface) {
            SDL_Rect frustum_text_rect = {
                frustum_button_rect.x + (frustum_button_width - frustum_text_surface->w) / 2,
                y_offset + (24 - frustum_text_surface->h) / 2,
                frustum_text_surface->w, frustum_text_surface->h
            };
            SDL_BlitSurface(frustum_text_surface, nullptr, content_surface, &frustum_text_rect);
            SDL_FreeSurface(frustum_text_surface);
        }

        const char* bvh_label = enable_bvh ? "BVH: ON" : "BVH: OFF";
        int bvh_button_width = 120;
        SDL_Rect bvh_button_rect = {150, y_offset, bvh_button_width, 24};
        buttons.push_back({{bvh_button_rect.x + panel_x, bvh_button_rect.y + panel_y, bvh_button_rect.w, bvh_button_rect.h},
                          "bvh", enable_bvh ? 1 : 0, 15});
        Uint32 bvh_button_bg = SDL_MapRGBA(content_surface->format,
            enable_bvh ? button_active_color.r : button_color.r,
            enable_bvh ? button_active_color.g : button_color.g,
            enable_bvh ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &bvh_button_rect, bvh_button_bg);
        SDL_Rect bvh_border = {bvh_button_rect.x, bvh_button_rect.y, bvh_button_rect.w, 2};
        SDL_FillRect(content_surface, &bvh_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        bvh_border = {bvh_button_rect.x, bvh_button_rect.y + bvh_button_rect.h - 2, bvh_button_rect.w, 2};
        SDL_FillRect(content_surface, &bvh_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        SDL_Surface* bvh_text_surface = TTF_RenderText_Blended(font, bvh_label, text_color);
        if (bvh_text_surface) {
            SDL_Rect bvh_text_rect = {
                bvh_button_rect.x + (bvh_button_width - bvh_text_surface->w) / 2,
                y_offset + (24 - bvh_text_surface->h) / 2,
                bvh_text_surface->w, bvh_text_surface->h
            };
            SDL_BlitSurface(bvh_text_surface, nullptr, content_surface, &bvh_text_rect);
            SDL_FreeSurface(bvh_text_surface);
        }

        y_offset += 35;

        // SIMD vs wavefront: mutually exclusive packet render paths (BVH can stay on for scalar hits)
        const char* simd_label = enable_simd_packets ? "SIMD: ON" : "SIMD: OFF";
        int simd_button_width = 120;
        SDL_Rect simd_button_rect = {15, y_offset, simd_button_width, 24};

        buttons.push_back({{simd_button_rect.x + panel_x, simd_button_rect.y + panel_y, simd_button_rect.w, simd_button_rect.h},
                          "simd_packets", enable_simd_packets ? 1 : 0, 14});

        Uint32 simd_button_bg = SDL_MapRGBA(content_surface->format,
            enable_simd_packets ? button_active_color.r : button_color.r,
            enable_simd_packets ? button_active_color.g : button_color.g,
            enable_simd_packets ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &simd_button_rect, simd_button_bg);

        SDL_Rect simd_border = {simd_button_rect.x, simd_button_rect.y, simd_button_rect.w, 2};
        SDL_FillRect(content_surface, &simd_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        simd_border = {simd_button_rect.x, simd_button_rect.y + simd_button_rect.h - 2, simd_button_rect.w, 2};
        SDL_FillRect(content_surface, &simd_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        SDL_Surface* simd_text_surface = TTF_RenderText_Blended(font, simd_label, text_color);
        if (simd_text_surface) {
            SDL_Rect simd_text_rect = {
                simd_button_rect.x + (simd_button_width - simd_text_surface->w) / 2,
                y_offset + (24 - simd_text_surface->h) / 2,
                simd_text_surface->w, simd_text_surface->h
            };
            SDL_BlitSurface(simd_text_surface, nullptr, content_surface, &simd_text_rect);
            SDL_FreeSurface(simd_text_surface);
        }

        const char* wavefront_label = enable_wavefront ? "Wavefront: ON" : "Wavefront: OFF";
        int wavefront_button_width = 120;
        SDL_Rect wavefront_button_rect = {150, y_offset, wavefront_button_width, 24};

        buttons.push_back({{wavefront_button_rect.x + panel_x, wavefront_button_rect.y + panel_y, wavefront_button_rect.w, wavefront_button_rect.h},
                          "wavefront", enable_wavefront ? 1 : 0, 10});

        Uint32 wavefront_button_bg = SDL_MapRGBA(content_surface->format,
            enable_wavefront ? button_active_color.r : button_color.r,
            enable_wavefront ? button_active_color.g : button_color.g,
            enable_wavefront ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &wavefront_button_rect, wavefront_button_bg);

        SDL_Rect wavefront_border = {wavefront_button_rect.x, wavefront_button_rect.y, wavefront_button_rect.w, 2};
        SDL_FillRect(content_surface, &wavefront_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        wavefront_border = {wavefront_button_rect.x, wavefront_button_rect.y + wavefront_button_rect.h - 2, wavefront_button_rect.w, 2};
        SDL_FillRect(content_surface, &wavefront_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));

        SDL_Surface* wavefront_text_surface = TTF_RenderText_Blended(font, wavefront_label, text_color);
        if (wavefront_text_surface) {
            SDL_Rect wavefront_text_rect = {
                wavefront_button_rect.x + (wavefront_button_width - wavefront_text_surface->w) / 2,
                y_offset + (24 - wavefront_text_surface->h) / 2,
                wavefront_text_surface->w, wavefront_text_surface->h
            };
            SDL_BlitSurface(wavefront_text_surface, nullptr, content_surface, &wavefront_text_rect);
            SDL_FreeSurface(wavefront_text_surface);
        }

        y_offset += 35;

        // Post denoise (edge-preserving filter on CPU worker after each traced frame)
        const char* denoise_label = enable_denoiser ? "Denoise: ON" : "Denoise: OFF";
        int denoise_button_width = 120;
        SDL_Rect denoise_button_rect = {15, y_offset, denoise_button_width, 24};
        buttons.push_back({{denoise_button_rect.x + panel_x, denoise_button_rect.y + panel_y, denoise_button_rect.w, denoise_button_rect.h},
                          "denoise", enable_denoiser ? 1 : 0, 16});

        Uint32 denoise_button_bg = SDL_MapRGBA(content_surface->format,
            enable_denoiser ? button_active_color.r : button_color.r,
            enable_denoiser ? button_active_color.g : button_color.g,
            enable_denoiser ? button_active_color.b : button_color.b,
            232);
        SDL_FillRect(content_surface, &denoise_button_rect, denoise_button_bg);
        SDL_Rect denoise_border = {denoise_button_rect.x, denoise_button_rect.y, denoise_button_rect.w, 2};
        SDL_FillRect(content_surface, &denoise_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        denoise_border = {denoise_button_rect.x, denoise_button_rect.y + denoise_button_rect.h - 2, denoise_button_rect.w, 2};
        SDL_FillRect(content_surface, &denoise_border, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
        SDL_Surface* denoise_text_surface = TTF_RenderText_Blended(font, denoise_label, text_color);
        if (denoise_text_surface) {
            SDL_Rect denoise_text_rect = {
                denoise_button_rect.x + (denoise_button_width - denoise_text_surface->w) / 2,
                y_offset + (24 - denoise_text_surface->h) / 2,
                denoise_text_surface->w, denoise_text_surface->h
            };
            SDL_BlitSurface(denoise_text_surface, nullptr, content_surface, &denoise_text_rect);
            SDL_FreeSurface(denoise_text_surface);
        }

        y_offset += 28;
        label_surface = TTF_RenderText_Blended(font, "Denoise str:", title_color);
        if (label_surface) {
            SDL_Rect lr = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &lr);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        {
            const char* str_names[] = {"Light", "Med", "Strong"};
            const int str_bw = 74;
            const int str_gap = 5;
            const int str_x0 = 15;
            for (int si = 0; si < 3; ++si) {
                SDL_Rect r = {str_x0 + si * (str_bw + str_gap), y_offset, str_bw, 24};
                const bool active = (denoise_strength == si);
                buttons.push_back({{r.x + panel_x, r.y + panel_y, r.w, r.h}, str_names[si], si, 18});
                Uint32 bg = SDL_MapRGBA(content_surface->format,
                    active ? button_active_color.r : button_color.r,
                    active ? button_active_color.g : button_color.g,
                    active ? button_active_color.b : button_color.b,
                    232);
                SDL_FillRect(content_surface, &r, bg);
                SDL_Rect btop = {r.x, r.y, r.w, 2};
                SDL_FillRect(content_surface, &btop, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
                SDL_Rect bbot = {r.x, r.y + r.h - 2, r.w, 2};
                SDL_FillRect(content_surface, &bbot, SDL_MapRGBA(content_surface->format, 120, 120, 140, 200));
                SDL_Surface* ts = TTF_RenderText_Blended(font, str_names[si], text_color);
                if (ts) {
                    SDL_Rect tr = {r.x + (str_bw - ts->w) / 2, r.y + (24 - ts->h) / 2, ts->w, ts->h};
                    SDL_BlitSurface(ts, nullptr, content_surface, &tr);
                    SDL_FreeSurface(ts);
                }
            }
        }

        y_offset += 35;

        {
            SDL_Rect slider_track = {15, y_offset + 8, 255, 8};
            SDL_FillRect(content_surface, &slider_track, SDL_MapRGBA(content_surface->format, 55, 60, 75, 210));
            const int amount = std::clamp(denoise_amount_percent, 0, 100);
            const int fill_w = std::max(1, (slider_track.w * amount) / 100);
            SDL_Rect slider_fill = {slider_track.x, slider_track.y, fill_w, slider_track.h};
            SDL_FillRect(content_surface, &slider_fill, SDL_MapRGBA(content_surface->format, 100, 150, 200, 225));
            const int knob_x = slider_track.x + ((slider_track.w - 1) * amount) / 100;
            SDL_Rect knob = {knob_x - 4, slider_track.y - 4, 8, 16};
            SDL_FillRect(content_surface, &knob, SDL_MapRGBA(content_surface->format, 170, 210, 245, 230));

            SDL_Rect slider_hit = {15, y_offset, 255, 24};
            buttons.push_back({{slider_hit.x + panel_x, slider_hit.y + panel_y, slider_hit.w, slider_hit.h},
                              "denoise_amount", amount, 21});
        }
        y_offset += 35;

        // Minimum accumulated progressive passes before publishing a frame (CPU worker).
        label_surface = TTF_RenderText_Blended(font, "Progressive display min passes:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, content_surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        int min_disp_values[] = {1, 2, 4, 8, 16, 32};
        for (int v : min_disp_values) {
            char label[12];
            snprintf(label, sizeof(label), "%d", v);
            bool is_active = (min_progressive_display_passes == v);
            render_button(label, v, 17, is_active);
        }
        y_offset += 35;

        // Add padding at bottom for scrolling
        y_offset += 40;

        // Calculate actual content height and scrolling
        content_height = y_offset;
        is_scrollable = (content_height > panel_height);
        max_scroll_offset = is_scrollable ? (content_height - panel_height) : 0;
        scroll_offset = std::min(scroll_offset, max_scroll_offset);  // Clamp scroll offset

        if (screenshot_button_pressed && screenshot_hit_w > 0) {
            SDL_Rect pr = {screenshot_hit_x, screenshot_hit_y, screenshot_hit_w, screenshot_hit_h};
            SDL_FillRect(content_surface, &pr, SDL_MapRGBA(content_surface->format, 48, 56, 72, 228));
            SDL_Rect s_top = {pr.x, pr.y, pr.w, 2};
            SDL_FillRect(content_surface, &s_top, SDL_MapRGBA(content_surface->format, 32, 38, 48, 210));
            SDL_Rect s_bot = {pr.x, pr.y + pr.h - 2, pr.w, 2};
            SDL_FillRect(content_surface, &s_bot, SDL_MapRGBA(content_surface->format, 32, 38, 48, 210));
            const char* pressed_label = "Save Screenshot";
            SDL_Color lit_text = {210, 220, 235, 255};
            SDL_Surface* ts = TTF_RenderText_Blended(font, pressed_label, lit_text);
            if (ts) {
                SDL_Rect tr = {pr.x + (pr.w - ts->w) / 2, pr.y + (pr.h - ts->h) / 2, ts->w, ts->h};
                SDL_BlitSurface(ts, nullptr, content_surface, &tr);
                SDL_FreeSurface(ts);
            }
        }

        // Convert surface to texture (alpha blend over the ray-traced view)
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, content_surface);
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            // Source rect: only show the visible portion based on scroll_offset
            SDL_Rect src_rect = {0, is_scrollable ? scroll_offset : 0, panel_width,
                                 is_scrollable ? panel_height : content_height};

            SDL_RenderCopy(renderer, texture, &src_rect, &overlay_rect);

            // Draw scrollbar if scrollable
            if (is_scrollable) {
                int scrollbar_width = 10;
                int scrollbar_x = panel_width - scrollbar_width - 5;
                int scrollbar_height = panel_height;
                float thumb_ratio = static_cast<float>(panel_height) / content_height;
                int thumb_height = static_cast<int>(thumb_ratio * scrollbar_height);
                int thumb_y = static_cast<float>(scroll_offset) / max_scroll_offset * (scrollbar_height - thumb_height);

                // Scrollbar is a separate blended texture (main panel texture is already uploaded above).
                SDL_Surface* scrollbar_surface = SDL_CreateRGBSurfaceWithFormat(
                    0, scrollbar_width, scrollbar_height, 32, SDL_PIXELFORMAT_RGBA8888);
                if (scrollbar_surface) {
                    SDL_FillRect(scrollbar_surface, nullptr, SDL_MapRGBA(scrollbar_surface->format, 0, 0, 0, 0));
                    SDL_FillRect(scrollbar_surface, nullptr, SDL_MapRGBA(scrollbar_surface->format, 40, 40, 50, 170));
                    SDL_Rect thumb_local = {0, thumb_y, scrollbar_width, thumb_height};
                    SDL_FillRect(scrollbar_surface, &thumb_local,
                                 SDL_MapRGBA(scrollbar_surface->format, 100, 150, 200, 220));

                    SDL_Texture* scrollbar_texture = SDL_CreateTextureFromSurface(renderer, scrollbar_surface);
                    if (scrollbar_texture) {
                        SDL_SetTextureBlendMode(scrollbar_texture, SDL_BLENDMODE_BLEND);
                        SDL_Rect scrollbar_dest = {panel_x + scrollbar_x, panel_y, scrollbar_width, scrollbar_height};
                        SDL_RenderCopy(renderer, scrollbar_texture, nullptr, &scrollbar_dest);
                        SDL_DestroyTexture(scrollbar_texture);
                    }
                    SDL_FreeSurface(scrollbar_surface);
                }
            }

            if (!is_scrollable) {
                if (panel_snap_tex) {
                    SDL_DestroyTexture(panel_snap_tex);
                }
                panel_snap_tex = texture;
                panel_snap_key = hkey;
                panel_snap_w = overlay_rect.w;
                panel_snap_h = overlay_rect.h;
            } else {
                SDL_DestroyTexture(texture);
                if (panel_snap_tex) {
                    SDL_DestroyTexture(panel_snap_tex);
                    panel_snap_tex = nullptr;
                    panel_snap_key = 0;
                }
            }
        }

        SDL_FreeSurface(content_surface);
    }

    // Handle mouse clicks, returns true if a setting was changed
    struct ClickResult {
        bool quality_changed;
        int new_quality;
        bool samples_changed;
        int new_samples;
        bool depth_changed;
        int new_depth;
        bool resolution_changed;
        int new_resolution;
        bool shadows_changed;
        bool reflections_changed;
        bool analysis_mode_changed;
        int new_analysis_mode;
        bool screenshot_requested;
        bool progressive_changed;
        bool adaptive_changed;
        bool denoiser_changed;
        bool denoise_strength_changed;
        int new_denoise_strength;
        bool denoise_amount_changed;
        int new_denoise_amount_percent;
        bool min_prog_display_changed;
        int new_min_prog_display_passes;
        bool wavefront_changed;
        bool morton_changed;
        bool stratified_changed;
        bool frustum_changed;
        bool simd_packets_changed;
        bool new_simd_packets;
        bool bvh_changed;
        bool new_bvh;
        bool dof_aperture_changed;
        int new_dof_aperture_milli;
        bool dof_focus_changed;
        int new_dof_focus_hundredths;
        bool button_clicked;

        ClickResult() : quality_changed(false), new_quality(0),
                       samples_changed(false), new_samples(1),
                       depth_changed(false), new_depth(1),
                       resolution_changed(false), new_resolution(0),
                       shadows_changed(false), reflections_changed(false),
                       analysis_mode_changed(false), new_analysis_mode(0),
                       screenshot_requested(false), progressive_changed(false),
                       adaptive_changed(false), denoiser_changed(false), denoise_strength_changed(false),
                       new_denoise_strength(0), denoise_amount_changed(false), new_denoise_amount_percent(70),
                       min_prog_display_changed(false),
                       new_min_prog_display_passes(1), wavefront_changed(false),
                       morton_changed(false), stratified_changed(false), frustum_changed(false),
                       simd_packets_changed(false), new_simd_packets(false),
                       bvh_changed(false), new_bvh(false),
                       dof_aperture_changed(false), new_dof_aperture_milli(0),
                       dof_focus_changed(false), new_dof_focus_hundredths(300),
                       button_clicked(false) {}
    };

    ClickResult handle_click(int mouse_x, int mouse_y) {
        ClickResult result;
        if (panel_layout_w <= 0 || panel_layout_h <= 0 || content_height <= 0) {
            return result;
        }

        const int local_x = mouse_x - panel_x;
        const int local_y = mouse_y - panel_y;
        if (local_x < 0 || local_x >= panel_layout_w || local_y < 0 || local_y >= panel_layout_h) {
            return result;
        }

        // Button rects use content Y + panel_y. Map click to content coordinates.
        int content_mouse_y;
        if (is_scrollable) {
            content_mouse_y = local_y + scroll_offset;
        } else {
            // Match vertical stretch: src (content_height) scaled to dest (panel_layout_h).
            content_mouse_y = static_cast<int>(std::floor(
                (static_cast<double>(local_y) + 0.5) * static_cast<double>(content_height) /
                static_cast<double>(panel_layout_h)));
            if (content_mouse_y < 0) {
                content_mouse_y = 0;
            } else if (content_mouse_y >= content_height) {
                content_mouse_y = content_height - 1;
            }
        }

        for (const auto& button : buttons) {
            const int bx = button.rect.x - panel_x;
            const int by = button.rect.y - panel_y;
            if (local_x >= bx && local_x < bx + button.rect.w && content_mouse_y >= by &&
                content_mouse_y < by + button.rect.h) {

                result.button_clicked = true;

                switch (button.category) {
                    case 0: // Quality level
                        result.quality_changed = true;
                        result.new_quality = button.value;
                        break;
                    case 1: // Samples
                        result.samples_changed = true;
                        result.new_samples = button.value;
                        break;
                    case 2: // Depth
                        result.depth_changed = true;
                        result.new_depth = button.value;
                        break;
                    case 3: // Shadows toggle
                        result.shadows_changed = true;
                        break;
                    case 4: // Reflections toggle
                        result.reflections_changed = true;
                        break;
                    case 5: // Resolution
                        result.resolution_changed = true;
                        result.new_resolution = button.value;
                        break;
                    case 6: // Analysis mode
                        result.analysis_mode_changed = true;
                        result.new_analysis_mode = button.value;
                        break;
                    case 7: // Screenshot
                        screenshot_button_pressed = true;
                        result.screenshot_requested = true;
                        break;
                    case 8: // Progressive toggle
                        result.progressive_changed = true;
                        break;
                    case 9: // Adaptive toggle
                        result.adaptive_changed = true;
                        break;
                    case 16: // Post denoise toggle
                        result.denoiser_changed = true;
                        break;
                    case 18: // Denoise strength (0 Light, 1 Med, 2 Strong)
                        result.denoise_strength_changed = true;
                        result.new_denoise_strength = button.value;
                        break;
                    case 21: { // Denoise amount slider
                        result.denoise_amount_changed = true;
                        const int rel = std::clamp(local_x - bx, 0, std::max(1, button.rect.w - 1));
                        const int pct = static_cast<int>(std::lround((100.0 * static_cast<double>(rel)) /
                                                                     static_cast<double>(std::max(1, button.rect.w - 1))));
                        result.new_denoise_amount_percent = std::clamp(pct, 0, 100);
                        break;
                    }
                    case 17: // Min progressive passes before display
                        result.min_prog_display_changed = true;
                        result.new_min_prog_display_passes = button.value;
                        break;
                    case 10: // Wavefront toggle
                        result.wavefront_changed = true;
                        break;
                    case 11: // Morton toggle
                        result.morton_changed = true;
                        break;
                    case 12: // Stratified toggle
                        result.stratified_changed = true;
                        break;
                    case 13: // Frustum toggle
                        result.frustum_changed = true;
                        break;
                    case 14: // SIMD packets toggle
                        result.simd_packets_changed = true;
                        result.new_simd_packets = button.value;
                        break;
                    case 15: // BVH toggle
                        result.bvh_changed = true;
                        result.new_bvh = button.value;
                        break;
                    case 19: // DOF lens radius (milli)
                        result.dof_aperture_changed = true;
                        result.new_dof_aperture_milli = button.value;
                        break;
                    case 20: // DOF focus distance (hundredths of world unit)
                        result.dof_focus_changed = true;
                        result.new_dof_focus_hundredths = button.value;
                        break;
                }
                break;
            }
        }
        return result;
    }

    void release_pressed_buttons() {
        screenshot_button_pressed = false;
    }

    // Adjust scroll offset by delta (positive = scroll down, negative = scroll up)
    void scroll(int delta) {
        if (!is_scrollable) {
            if (scroll_offset != 0) {
                scroll_offset = 0;
                if (panel_snap_tex) {
                    SDL_DestroyTexture(panel_snap_tex);
                    panel_snap_tex = nullptr;
                    panel_snap_key = 0;
                }
            }
            return;
        }
        scroll_offset -= delta;
        // Clamp scroll offset
        scroll_offset = std::max(0, std::min(scroll_offset, max_scroll_offset));
        if (panel_snap_tex) {
            SDL_DestroyTexture(panel_snap_tex);
            panel_snap_tex = nullptr;
            panel_snap_key = 0;
        }
    }

    ~ControlsPanel() {
        if (panel_snap_tex) {
            SDL_DestroyTexture(panel_snap_tex);
            panel_snap_tex = nullptr;
        }
        if (font) TTF_CloseFont(font);
        if (title_font) TTF_CloseFont(title_font);
        if (initialized) TTF_Quit();
    }
};

// Copy packed RGB24 (tight w*3 stride) into a streaming SDL texture, honoring row pitch.
static void blit_packed_rgb24_to_texture(SDL_Texture* texture, const unsigned char* src, int width, int height) {
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0) {
        return;
    }
    const int src_stride = width * 3;
    if (pitch == src_stride) {
        std::memcpy(pixels, src, static_cast<size_t>(src_stride) * static_cast<size_t>(height));
    } else {
        auto* dst = static_cast<unsigned char*>(pixels);
        for (int y = 0; y < height; ++y) {
            std::memcpy(dst + static_cast<size_t>(y) * static_cast<size_t>(pitch),
                        src + static_cast<size_t>(y) * static_cast<size_t>(src_stride),
                        static_cast<size_t>(src_stride));
        }
    }
    SDL_UnlockTexture(texture);
}

// Maps panel "Light / Med / Strong" to bilateral parameters (smaller spatial radius and sigmas => less blur).
static void denoise_params_from_strength(int strength, float* out_sigma_s, float* out_sigma_r, int* out_spatial_radius) {
    const int s = std::max(0, std::min(2, strength));
    if (s <= 0) {
        *out_sigma_s = 0.72f;
        *out_sigma_r = 0.034f;
        *out_spatial_radius = 1;
    } else if (s == 1) {
        *out_sigma_s = 1.35f;
        *out_sigma_r = 0.065f;
        *out_spatial_radius = 2;
    } else {
        *out_sigma_s = 1.58f;
        *out_sigma_r = 0.088f;
        *out_spatial_radius = 2;
    }
}

// Half-resolution bilateral pass then bilinear upscale (large frames only). Inner filter = separable bilateral.
static void apply_rgb24_edge_preserving_denoise_large(std::vector<unsigned char>& rgb, int w, int h, float sigma_s,
                                                      float sigma_r, int spatial_radius) {
    const int need_half = spatial_radius * 2 + 1;
    if (w < 10 || h < 10 || static_cast<long long>(w) * h < 400000) {
        rgb24_post::apply_separable_bilateral_rgb24(rgb, w, h, sigma_s, sigma_r, spatial_radius);
        return;
    }
    const int w2 = w / 2;
    const int h2 = h / 2;
    if (w2 < need_half || h2 < need_half) {
        rgb24_post::apply_separable_bilateral_rgb24(rgb, w, h, sigma_s, sigma_r, spatial_radius);
        return;
    }
    std::vector<unsigned char> half(static_cast<size_t>(w2) * static_cast<size_t>(h2) * 3u);
    for (int y2 = 0; y2 < h2; ++y2) {
        for (int x2 = 0; x2 < w2; ++x2) {
            const int x0 = x2 * 2;
            const int y0 = y2 * 2;
            size_t acc_r = 0, acc_g = 0, acc_b = 0;
            int cnt = 0;
            for (int dy = 0; dy < 2 && y0 + dy < h; ++dy) {
                for (int dx = 0; dx < 2 && x0 + dx < w; ++dx) {
                    const size_t si = (static_cast<size_t>(y0 + dy) * static_cast<size_t>(w) + static_cast<size_t>(x0 + dx)) * 3u;
                    acc_r += rgb[si];
                    acc_g += rgb[si + 1];
                    acc_b += rgb[si + 2];
                    ++cnt;
                }
            }
            const size_t di = (static_cast<size_t>(y2) * static_cast<size_t>(w2) + static_cast<size_t>(x2)) * 3u;
            half[di] = static_cast<unsigned char>(acc_r / cnt);
            half[di + 1] = static_cast<unsigned char>(acc_g / cnt);
            half[di + 2] = static_cast<unsigned char>(acc_b / cnt);
        }
    }
    rgb24_post::apply_separable_bilateral_rgb24(half, w2, h2, sigma_s, sigma_r, spatial_radius);
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) * 0.5f - 0.5f;
            const float fy = (static_cast<float>(y) + 0.5f) * 0.5f - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, w2 - 1);
            const int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, h2 - 1);
            const int x1 = std::min(x0 + 1, w2 - 1);
            const int y1 = std::min(y0 + 1, h2 - 1);
            const float tx = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
            const float ty = std::clamp(fy - static_cast<float>(y0), 0.0f, 1.0f);
            const size_t i00 = (static_cast<size_t>(y0) * static_cast<size_t>(w2) + static_cast<size_t>(x0)) * 3u;
            const size_t i10 = (static_cast<size_t>(y0) * static_cast<size_t>(w2) + static_cast<size_t>(x1)) * 3u;
            const size_t i01 = (static_cast<size_t>(y1) * static_cast<size_t>(w2) + static_cast<size_t>(x0)) * 3u;
            const size_t i11 = (static_cast<size_t>(y1) * static_cast<size_t>(w2) + static_cast<size_t>(x1)) * 3u;
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3u;
            for (int c = 0; c < 3; ++c) {
                const float v00 = static_cast<float>(half[i00 + static_cast<size_t>(c)]);
                const float v10 = static_cast<float>(half[i10 + static_cast<size_t>(c)]);
                const float v01 = static_cast<float>(half[i01 + static_cast<size_t>(c)]);
                const float v11 = static_cast<float>(half[i11 + static_cast<size_t>(c)]);
                const float top = v00 + (v10 - v00) * tx;
                const float bot = v01 + (v11 - v01) * tx;
                const float v = top + (bot - top) * ty;
                rgb[di + static_cast<size_t>(c)] =
                    static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
            }
        }
    }
}

// Fast unsharp after denoise: 3x3 box blur as low-pass, one boost pass (OpenMP). Keeps edges from over-shooting.
static void apply_rgb24_light_unsharp_after_denoise(std::vector<unsigned char>& rgb, int w, int h,
                                                    int denoise_strength_0_2) {
    if (w < 3 || h < 3) {
        return;
    }
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * 3u;
    if (rgb.size() < n) {
        return;
    }
    const int s = std::max(0, std::min(2, denoise_strength_0_2));
    // Slightly more pull-through for stronger bilateral (which softens more).
    const float k = 0.38f + 0.11f * static_cast<float>(s);
    const size_t row_stride = static_cast<size_t>(w) * 3u;

    // Must not be thread_local: OpenMP worker threads each had their own tiny/unresized buffer → OOB/crash after denoise.
    std::vector<unsigned char> out(n);

    #pragma omp parallel for schedule(static)
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            const size_t ci = static_cast<size_t>(y) * row_stride + static_cast<size_t>(x) * 3u;
            unsigned sumr = 0;
            unsigned sumg = 0;
            unsigned sumb = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const unsigned char* row = rgb.data() + (static_cast<size_t>(y + dy) * static_cast<size_t>(w) + static_cast<size_t>(x - 1)) * 3u;
                sumr += static_cast<unsigned>(row[0]) + static_cast<unsigned>(row[3]) + static_cast<unsigned>(row[6]);
                sumg += static_cast<unsigned>(row[1]) + static_cast<unsigned>(row[4]) + static_cast<unsigned>(row[7]);
                sumb += static_cast<unsigned>(row[2]) + static_cast<unsigned>(row[5]) + static_cast<unsigned>(row[8]);
            }
            const float br = static_cast<float>(sumr) * (1.0f / 9.0f);
            const float bg = static_cast<float>(sumg) * (1.0f / 9.0f);
            const float bb = static_cast<float>(sumb) * (1.0f / 9.0f);
            const float cr = static_cast<float>(rgb[ci]);
            const float cg = static_cast<float>(rgb[ci + 1]);
            const float cb = static_cast<float>(rgb[ci + 2]);
            out[ci] = static_cast<unsigned char>(std::clamp(cr + k * (cr - br), 0.0f, 255.0f));
            out[ci + 1] = static_cast<unsigned char>(std::clamp(cg + k * (cg - bg), 0.0f, 255.0f));
            out[ci + 2] = static_cast<unsigned char>(std::clamp(cb + k * (cb - bb), 0.0f, 255.0f));
        }
    }

    for (int x = 0; x < w; ++x) {
        const size_t t = static_cast<size_t>(x) * 3u;
        const size_t b = (static_cast<size_t>(h - 1) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3u;
        out[t] = rgb[t];
        out[t + 1] = rgb[t + 1];
        out[t + 2] = rgb[t + 2];
        out[b] = rgb[b];
        out[b + 1] = rgb[b + 1];
        out[b + 2] = rgb[b + 2];
    }
    for (int y = 1; y < h - 1; ++y) {
        const size_t left = static_cast<size_t>(y) * row_stride;
        const size_t right = left + static_cast<size_t>(w - 1) * 3u;
        out[left] = rgb[left];
        out[left + 1] = rgb[left + 1];
        out[left + 2] = rgb[left + 2];
        out[right] = rgb[right];
        out[right + 1] = rgb[right + 1];
        out[right + 2] = rgb[right + 2];
    }

    rgb.swap(out);
}

static void apply_rgb24_denoise_pipeline(std::vector<unsigned char>& rgb, int w, int h, int denoise_strength_0_2,
                                         int denoise_amount_percent) {
    const int amount = std::clamp(denoise_amount_percent, 0, 100);
    if (amount <= 0 || w < 2 || h < 2) {
        return;
    }
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * 3u;
    if (rgb.size() < n) {
        return;
    }

    const std::vector<unsigned char> original = rgb;
    std::vector<float> l_orig;
    rgb24_post::build_luminance_buffer(original, w, h, l_orig);
    std::vector<float> lum_var;
    rgb24_post::luminance_variance_3x3(l_orig, w, h, lum_var);

    float ds = 1.35f;
    float dr = 0.065f;
    int rad = 2;
    denoise_params_from_strength(denoise_strength_0_2, &ds, &dr, &rad);

    // Scale filter aggressiveness with slider amount to preserve AA detail at lower amounts.
    const float amount01 = static_cast<float>(amount) * (1.0f / 100.0f);
    const float strength_scale = 0.58f + 0.72f * amount01;
    ds *= strength_scale;
    dr *= (0.55f + 0.80f * amount01);
    if (amount < 45) {
        rad = std::max(1, rad - 1);
    }

    apply_rgb24_edge_preserving_denoise_large(rgb, w, h, ds, dr, rad);
    apply_rgb24_light_unsharp_after_denoise(rgb, w, h, denoise_strength_0_2);

    // Variance-guided mix: high local luminance variance → keep more of the original (edges / noise structure).
    constexpr float k_var = 320.0f;
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; ++y) {
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(w) * 3u;
        const size_t lrow = static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            const size_t i = row + static_cast<size_t>(x) * 3u;
            const float var = lum_var[lrow + static_cast<size_t>(x)];
            const float edge_preserve = 1.0f / (1.0f + k_var * var);
            const float mix = amount01 * edge_preserve;
            for (int c = 0; c < 3; ++c) {
                const float o = static_cast<float>(original[i + static_cast<size_t>(c)]);
                const float dn = static_cast<float>(rgb[i + static_cast<size_t>(c)]);
                const float v = o + (dn - o) * mix;
                rgb[i + static_cast<size_t>(c)] = static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
            }
        }
    }
}

static void diagnostics_blit_line(SDL_Surface* dst, TTF_Font* f, const char* txt, SDL_Color c, int x, int y) {
    if (!dst || !f || !txt) return;
    SDL_Surface* s = TTF_RenderText_Blended(f, txt, c);
    if (!s) return;
    SDL_Rect r = {x, y, s->w, s->h};
    SDL_BlitSurface(s, nullptr, dst, &r);
    SDL_FreeSurface(s);
}

static constexpr int kCpuDiagProfileN = 96;

static void diag_draw_hist_64(SDL_Surface* card, int x0, int y0, int plot_w, int plot_h, const uint32_t bins[64],
                                Uint8 R, Uint8 G, Uint8 B) {
    if (!card || plot_w < 4 || plot_h < 2) return;
    uint32_t mx = 1u;
    for (int i = 0; i < 64; ++i) {
        mx = std::max(mx, bins[i]);
    }
    SDL_Rect back = {x0 - 2, y0 - 1, plot_w + 4, plot_h + 2};
    SDL_FillRect(card, &back, SDL_MapRGBA(card->format, 22, 25, 32, 210));
    for (int i = 0; i < 64; ++i) {
        const int bh = static_cast<int>((static_cast<uint64_t>(bins[i]) * static_cast<uint64_t>(plot_h)) /
                                         static_cast<uint64_t>(mx));
        const int bx = x0 + (i * plot_w) / 64;
        const int bw = std::max(1, plot_w / 64 - 1);
        SDL_Rect bar = {bx, y0 + plot_h - bh, bw, std::max(1, bh)};
        SDL_FillRect(card, &bar, SDL_MapRGBA(card->format, R, G, B, 225));
    }
}

static void diag_draw_sparkline(SDL_Surface* card, int x0, int y0, int plot_w, int plot_h, const float* v, int n,
                                Uint8 R, Uint8 G, Uint8 B) {
    if (!card || !v || n < 1 || plot_w < 4 || plot_h < 2) return;
    float mn = v[0];
    float mx = v[0];
    for (int i = 1; i < n; ++i) {
        mn = std::min(mn, v[i]);
        mx = std::max(mx, v[i]);
    }
    if (mx <= mn + 1e-7f) {
        mx = mn + 1e-3f;
    }
    SDL_Rect back = {x0 - 2, y0 - 1, plot_w + 4, plot_h + 2};
    SDL_FillRect(card, &back, SDL_MapRGBA(card->format, 22, 25, 32, 210));
    const int bar_w = std::max(1, plot_w / n);
    for (int i = 0; i < n; ++i) {
        const float t = (v[i] - mn) / (mx - mn);
        const int bh = std::max(1, static_cast<int>(t * static_cast<float>(plot_h - 1)));
        int bx = x0;
        if (n > 1) {
            bx = x0 + static_cast<int>((static_cast<long long>(i) * static_cast<long long>(plot_w - bar_w)) /
                                       static_cast<long long>(n - 1));
        }
        bx = std::min(bx, x0 + plot_w - bar_w);
        SDL_Rect bar = {bx, y0 + plot_h - bh, bar_w, bh};
        SDL_FillRect(card, &bar, SDL_MapRGBA(card->format, R, G, B, 218));
    }
}

static void diag_plot_line(SDL_Surface* card, int x0, int y0, int x1, int y1, Uint8 R, Uint8 G, Uint8 B) {
    if (!card) return;
    if (x0 == x1 && y0 == y1) {
        SDL_Rect p = {x0, y0, 2, 2};
        SDL_FillRect(card, &p, SDL_MapRGBA(card->format, R, G, B, 245));
        return;
    }
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int x = x0;
    int y = y0;
    for (;;) {
        SDL_Rect p = {x, y, 2, 2};
        SDL_FillRect(card, &p, SDL_MapRGBA(card->format, R, G, B, 245));
        if (x == x1 && y == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// Cumulative luminance distribution from histogram (0..1 vs bin). No extra framebuffer pass.
static void diag_draw_luma_cdf(SDL_Surface* card, int x0, int y0, int plot_w, int plot_h, const uint32_t bins[64],
                               uint64_t total) {
    if (!card || plot_w < 8 || plot_h < 6 || total == 0ull) return;
    SDL_Rect back = {x0 - 2, y0 - 1, plot_w + 4, plot_h + 2};
    SDL_FillRect(card, &back, SDL_MapRGBA(card->format, 22, 25, 32, 210));
    // Reference: 50% and 90% cumulative (exposure / rolloff at a glance)
    const int y50 = y0 + plot_h - 2 - static_cast<int>(0.5f * static_cast<float>(plot_h - 4));
    const int y90 = y0 + plot_h - 2 - static_cast<int>(0.9f * static_cast<float>(plot_h - 4));
    SDL_Rect ref50 = {x0, y50, plot_w, 1};
    SDL_Rect ref90 = {x0, y90, plot_w, 1};
    SDL_FillRect(card, &ref50, SDL_MapRGBA(card->format, 80, 90, 110, 120));
    SDL_FillRect(card, &ref90, SDL_MapRGBA(card->format, 80, 90, 110, 90));

    uint64_t cum = 0ull;
    int px[64];
    int py[64];
    for (int i = 0; i < 64; ++i) {
        cum += bins[i];
        const float f = static_cast<float>(static_cast<double>(cum) / static_cast<double>(total));
        px[i] = x0 + (plot_w - 1) * i / 63;
        py[i] = y0 + plot_h - 2 - static_cast<int>(f * static_cast<float>(plot_h - 4));
    }
    for (int i = 0; i < 63; ++i) {
        diag_plot_line(card, px[i], py[i], px[i + 1], py[i + 1], 255, 220, 140);
    }
}

// Bottom-left diagnostics card: stats + luma / RGB histograms + row/column mean-luma profiles.
static void render_diagnostics_g_overlay(SDL_Renderer* renderer, TTF_Font* title_f, TTF_Font* body_f,
                                         const uint32_t bins64[64], const uint32_t r64[64], const uint32_t g64[64],
                                         const uint32_t b64[64], const float row_luma[kCpuDiagProfileN],
                                         const float col_luma[kCpuDiagProfileN], int window_width, int window_height,
                                         int img_w, int img_h, int samples_per_pixel, int max_depth, float fps,
                                         double last_cpu_sec, const char* analysis_name, float mean_r, float mean_g,
                                         float mean_b, float mean_l, float min_l, float max_l, const char* path_summary,
                                         bool paused) {
    uint32_t mx = 1u;
    uint64_t total_px = 0ull;
    for (int i = 0; i < 64; ++i) {
        mx = std::max(mx, bins64[i]);
        total_px += bins64[i];
    }

    const int bar_total_w = 220;
    const int h_max = 52;
    const int margin = 12;
    const int text_left = 14;
    const int line_h = 18;
    const int title_y = 8;
    const int body0_y = 32;
    const int hist_top_pad = 6;
    const int rgb_hist_h = 18;
    const int rgb_gap = 3;
    const int spark_plot_h = 22;
    const int cdf_plot_h = 28;
    const bool show_graph_labels = (title_f != nullptr && body_f != nullptr);
    const int luma_caption_h = show_graph_labels ? 14 : 0;
    // Clear gap so the luma label is not covered by hist_back (y0−2) or the top bars.
    const int luma_caption_gap = show_graph_labels ? 8 : 0;
    const int luma_caption_block = luma_caption_h + luma_caption_gap;
    const int cdf_caption_h = show_graph_labels ? 14 : 0;
    const int rgb_caption_h = show_graph_labels ? 14 : 0;
    const int spark_caption_block =
        show_graph_labels ? (14 + spark_plot_h + 8 + 14 + spark_plot_h) : (spark_plot_h + 10 + spark_plot_h);
    const int top_and_luma_hist = 4 + luma_caption_block + h_max + 10;
    const int cdf_block = cdf_caption_h + cdf_plot_h + 8;
    const int rgb_block = rgb_caption_h + (rgb_hist_h * 3 + rgb_gap * 2) + 8;
    const int graphs_block_h = top_and_luma_hist + cdf_block + rgb_block + spark_caption_block + 12;

    char line0[160], line1[160], line2[160], line3[160], line4[160], line5[160], line6[192];
    snprintf(line0, sizeof(line0), "%dx%d  |  %d spp  |  depth %d", img_w, img_h, samples_per_pixel, max_depth);
    snprintf(line1, sizeof(line1), "%.1f FPS  |  last CPU frame %.3f s", fps, last_cpu_sec);
    snprintf(line2, sizeof(line2), "Analysis: %s", analysis_name && analysis_name[0] ? analysis_name : "—");
    snprintf(line3, sizeof(line3), "Luma  mean %.4f   min %.4f   max %.4f", mean_l, min_l, max_l);
    snprintf(line4, sizeof(line4), "RGB mean  %.0f  %.0f  %.0f  (0-255)", mean_r, mean_g, mean_b);
    snprintf(line5, sizeof(line5), "Histogram: %llu px (64 bins)  |  OMP %d threads",
             static_cast<unsigned long long>(total_px), std::max(1, omp_get_max_threads()));
    snprintf(line6, sizeof(line6), "Paths: %s  |  %s", path_summary && path_summary[0] ? path_summary : "—",
             paused ? "PAUSED" : "live");

    int max_tw = bar_total_w + text_left * 2;
    if (title_f && body_f) {
        auto tw = [&](TTF_Font* ff, const char* s) {
            int w = 0, h = 0;
            if (!s || TTF_SizeText(ff, s, &w, &h) != 0) return 0;
            return w;
        };
        max_tw = std::max(max_tw, tw(title_f, "DIAGNOSTICS"));
        max_tw = std::max(max_tw, tw(body_f, line0));
        max_tw = std::max(max_tw, tw(body_f, line1));
        max_tw = std::max(max_tw, tw(body_f, line2));
        max_tw = std::max(max_tw, tw(body_f, line3));
        max_tw = std::max(max_tw, tw(body_f, line4));
        max_tw = std::max(max_tw, tw(body_f, line5));
        max_tw = std::max(max_tw, tw(body_f, line6));
        max_tw = std::max(max_tw, tw(body_f, "Luminance (64 bins)"));
        max_tw = std::max(max_tw, tw(body_f, "Luma CDF (cumulative fraction)"));
        max_tw = std::max(max_tw, tw(body_f, "RGB (64 bins / channel)"));
        max_tw = std::max(max_tw, tw(body_f, "Mean luma vs row (Y)"));
        max_tw = std::max(max_tw, tw(body_f, "Mean luma vs column (X)"));
    }

    const int card_w = std::max(100, std::min(window_width - 2 * margin, max_tw + text_left + 10));
    const int text_block_h = body0_y + 7 * line_h + hist_top_pad;
    const int card_h = text_block_h + graphs_block_h;
    const int card_x = margin;
    const int card_y = std::max(margin, window_height - card_h - margin);

    SDL_Surface* card =
        SDL_CreateRGBSurfaceWithFormat(0, card_w, card_h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!card) return;

    SDL_FillRect(card, nullptr, SDL_MapRGBA(card->format, 50, 55, 65, 230));
    SDL_Rect sep = {10, 28, card_w - 20, 1};
    SDL_FillRect(card, &sep, SDL_MapRGBA(card->format, 100, 100, 120, 200));

    const SDL_Color title_c = {100, 200, 255, 255};
    const SDL_Color body_c = {210, 215, 225, 255};
    const SDL_Color accent_c = {255, 200, 100, 255};

    if (title_f && body_f) {
        diagnostics_blit_line(card, title_f, "DIAGNOSTICS", title_c, text_left, title_y);
        diagnostics_blit_line(card, body_f, line0, body_c, text_left, body0_y);
        diagnostics_blit_line(card, body_f, line1, accent_c, text_left, body0_y + line_h);
        diagnostics_blit_line(card, body_f, line2, body_c, text_left, body0_y + 2 * line_h);
        diagnostics_blit_line(card, body_f, line3, body_c, text_left, body0_y + 3 * line_h);
        diagnostics_blit_line(card, body_f, line4, body_c, text_left, body0_y + 4 * line_h);
        diagnostics_blit_line(card, body_f, line5, body_c, text_left, body0_y + 5 * line_h);
        diagnostics_blit_line(card, body_f, line6, body_c, text_left, body0_y + 6 * line_h);
    }

    const int bx0 = text_left;
    const int graph_y0 = text_block_h + 4;
    if (show_graph_labels) {
        diagnostics_blit_line(card, body_f, "Luminance (64 bins)", body_c, text_left, graph_y0);
    }
    const int hist_y0 = show_graph_labels ? (graph_y0 + luma_caption_block) : graph_y0;
    SDL_Rect hist_back = {text_left - 4, hist_y0 - 2, bar_total_w + 8, h_max + 6};
    SDL_FillRect(card, &hist_back, SDL_MapRGBA(card->format, 25, 28, 38, 200));
    for (int i = 0; i < 64; ++i) {
        const int bh = static_cast<int>((static_cast<uint64_t>(bins64[i]) * static_cast<uint64_t>(h_max)) /
                                         static_cast<uint64_t>(mx));
        const int bx = bx0 + (i * bar_total_w) / 64;
        const int bw = std::max(1, bar_total_w / 64 - 1);
        SDL_Rect bar = {bx, hist_y0 + h_max - bh, bw, std::max(1, bh)};
        const int t = (i * 4) + 32;
        SDL_FillRect(card, &bar,
                     SDL_MapRGBA(card->format, std::min(255, t + 40), std::min(255, 180 - i), 220, 235));
    }

    int y = hist_y0 + h_max + 8;
    if (show_graph_labels) {
        diagnostics_blit_line(card, body_f, "Luma CDF (cumulative fraction)", body_c, text_left, y);
        y += cdf_caption_h;
    }
    diag_draw_luma_cdf(card, bx0, y, bar_total_w, cdf_plot_h, bins64, total_px);
    y += cdf_plot_h + 8;

    if (show_graph_labels) {
        diagnostics_blit_line(card, body_f, "RGB (64 bins / channel)", body_c, text_left, y);
        y += rgb_caption_h;
    }
    diag_draw_hist_64(card, bx0, y, bar_total_w, rgb_hist_h, r64, 220, 95, 95);
    y += rgb_hist_h + rgb_gap;
    diag_draw_hist_64(card, bx0, y, bar_total_w, rgb_hist_h, g64, 90, 200, 110);
    y += rgb_hist_h + rgb_gap;
    diag_draw_hist_64(card, bx0, y, bar_total_w, rgb_hist_h, b64, 100, 140, 245);
    y += rgb_hist_h + 8;

    if (show_graph_labels) {
        diagnostics_blit_line(card, body_f, "Mean luma vs row (Y)", body_c, text_left, y);
        y += 14;
    }
    diag_draw_sparkline(card, bx0, y, bar_total_w, spark_plot_h, row_luma, kCpuDiagProfileN, 180, 220, 120);
    y += spark_plot_h + 8;

    if (show_graph_labels) {
        diagnostics_blit_line(card, body_f, "Mean luma vs column (X)", body_c, text_left, y);
        y += 14;
    }
    diag_draw_sparkline(card, bx0, y, bar_total_w, spark_plot_h, col_luma, kCpuDiagProfileN, 120, 190, 255);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, card);
    SDL_FreeSurface(card);
    if (!tex) return;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Rect dest = {card_x, card_y, card_w, card_h};
    SDL_RenderCopy(renderer, tex, nullptr, &dest);
    SDL_DestroyTexture(tex);
}

static bool rt_profile_enabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* e = std::getenv("RT_PROFILE");
        cached = (e && e[0] != '0') ? 1 : 0;
    }
    return cached == 1;
}

static bool same_quality_preset(const QualityPreset& a, const QualityPreset& b) {
    return a.width == b.width && a.samples == b.samples && a.max_depth == b.max_depth;
}

// Progressive Monte Carlo averages samples for a *fixed* view. If the camera moves between passes,
// old samples must be discarded or every frame becomes a blend of all past viewpoints.
static bool progressive_view_differs(const Camera& a, const Camera& b) {
    constexpr float pe = 1e-4f;
    auto dist2 = [](const Point3& p, const Point3& q) {
        const float dx = p.x - q.x;
        const float dy = p.y - q.y;
        const float dz = p.z - q.z;
        return dx * dx + dy * dy + dz * dz;
    };
    if (dist2(a.lookfrom, b.lookfrom) > pe * pe) {
        return true;
    }
    if (dist2(a.lookat, b.lookat) > pe * pe) {
        return true;
    }
    if (std::fabs(a.vfov - b.vfov) > 1e-3f) {
        return true;
    }
    if (std::fabs(a.aspect_ratio - b.aspect_ratio) > 1e-6f) {
        return true;
    }
    if (std::fabs(a.aperture - b.aperture) > 1e-5f) {
        return true;
    }
    if (std::fabs(a.focus_dist - b.focus_dist) > 1e-4f) {
        return true;
    }
    const Vec3 au = unit_vector(a.vup);
    const Vec3 bu = unit_vector(b.vup);
    if (dot(au, bu) < 0.99995f) {
        return true;
    }
    return false;
}

struct CpuTracePassResult {
    bool schedule_next;
    int progressive_pass_count;
};

// Full CPU ray-tracing pass (runs on background thread). Progressive accumulation resets when the view changes.
static CpuTracePassResult execute_cpu_ray_tracing_pass(Scene& scene, Camera cam, const QualityPreset& preset, int image_width,
                                                       int image_height, Renderer& ray_renderer, RenderAnalysis& analysis,
                                                       bool enable_shadows, std::vector<unsigned char>& framebuffer) {
    if (!scene.simd_scene_cache_ready()) {
        scene.build_simd_cache();
    }
    ray_renderer.sync_frustum(cam);

    bool schedule_next_render = false;
    int progressive_pass_count = 0;
    const bool prof = rt_profile_enabled();
    const auto t0_prof = std::chrono::high_resolution_clock::now();
    int path_id = 6;

    static Camera progressive_cam_anchor;
    static bool progressive_cam_anchor_valid = false;
    static int progressive_last_max_depth = -1;
    static uint32_t last_rt_settings_sig_execute = ~0u;
    if (!ray_renderer.enable_progressive) {
        progressive_cam_anchor_valid = false;
        progressive_last_max_depth = -1;
    }
    uint32_t rt_settings_sig =
        (ray_renderer.enable_stratified ? 1u : 0u) |
        (ray_renderer.enable_frustum ? 2u : 0u) |
        (ray_renderer.enable_bvh ? 4u : 0u) |
        (ray_renderer.enable_morton ? 8u : 0u) |
        (ray_renderer.enable_wavefront ? 16u : 0u) |
        (ray_renderer.enable_simd_packets ? 32u : 0u) |
        (ray_renderer.enable_progressive ? 64u : 0u);
    {
        unsigned bucket = 0u;
        if (ray_renderer.enable_progressive) {
            if (ray_renderer.enable_wavefront) {
                bucket = 1u;
            } else if (ray_renderer.enable_simd_packets) {
                bucket = 2u;
            } else {
                bucket = 3u;
            }
        }
        rt_settings_sig |= bucket << 8u;
    }
    const bool rt_settings_changed = (rt_settings_sig != last_rt_settings_sig_execute);

    const bool progressive_view_changed =
        ray_renderer.enable_progressive &&
        (!progressive_cam_anchor_valid || progressive_view_differs(progressive_cam_anchor, cam) ||
         progressive_last_max_depth != ray_renderer.max_depth || rt_settings_changed);

    if (ray_renderer.enable_progressive && ray_renderer.enable_wavefront) {
        path_id = 1;
        static std::vector<Color> wf_progressive_accum;
        static int wf_progressive_passes = 0;
        static bool wf_progressive_initialized = false;

        const size_t wf_cells = static_cast<size_t>(image_width) * static_cast<size_t>(image_height);
        const bool wf_dims_mismatch =
            wf_progressive_accum.size() != wf_cells || image_height <= 0 || image_width <= 0;
        if (!wf_progressive_initialized || wf_dims_mismatch || progressive_view_changed) {
            wf_progressive_accum.assign(wf_cells, Color(0, 0, 0));
            wf_progressive_passes = 0;
            wf_progressive_initialized = true;
        }

        ray_renderer.render_wavefront(cam, scene, wf_progressive_accum, image_width, image_height, 1, true);
        wf_progressive_passes++;

        const float inv_passes = 1.0f / static_cast<float>(wf_progressive_passes);
        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color linear =
                    wf_progressive_accum[static_cast<size_t>(j) * static_cast<size_t>(image_width) +
                                          static_cast<size_t>(i)] *
                    inv_passes;
                Color pixel_color(std::sqrt(linear.x), std::sqrt(linear.y), std::sqrt(linear.z));
                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
            }
        }

        if (wf_progressive_passes < preset.samples) {
            schedule_next_render = true;
        }
        progressive_pass_count = wf_progressive_passes;
        progressive_cam_anchor = cam;
        progressive_cam_anchor_valid = true;
        progressive_last_max_depth = ray_renderer.max_depth;
    } else if (ray_renderer.enable_progressive && ray_renderer.enable_simd_packets) {
        path_id = 2;
        static std::vector<Color> simd_prog_linear_accum;
        static int simd_prog_passes = 0;
        static bool simd_prog_initialized = false;

        const size_t simd_cells = static_cast<size_t>(image_width) * static_cast<size_t>(image_height);
        const bool simd_prog_dims_mismatch =
            simd_prog_linear_accum.size() != simd_cells || image_height <= 0 || image_width <= 0;

        if (!simd_prog_initialized || simd_prog_dims_mismatch || progressive_view_changed) {
            simd_prog_linear_accum.assign(simd_cells, Color(0, 0, 0));
            simd_prog_passes = 0;
            simd_prog_initialized = true;
        }

        ray_renderer.render_simd_packets(cam, scene, simd_prog_linear_accum, image_width, image_height, 1, true);
        simd_prog_passes++;

        const float inv_passes = 1.0f / static_cast<float>(simd_prog_passes);
        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color linear =
                    simd_prog_linear_accum[static_cast<size_t>(j) * static_cast<size_t>(image_width) +
                                           static_cast<size_t>(i)] *
                    inv_passes;
                Color pixel_color(std::sqrt(linear.x), std::sqrt(linear.y), std::sqrt(linear.z));
                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
            }
        }

        if (simd_prog_passes < preset.samples) {
            schedule_next_render = true;
        }
        progressive_pass_count = simd_prog_passes;
        progressive_cam_anchor = cam;
        progressive_cam_anchor_valid = true;
        progressive_last_max_depth = ray_renderer.max_depth;
    } else if (ray_renderer.enable_progressive) {
        path_id = 3;
        static std::vector<Color> scalar_prog_linear_accum;
        static int scalar_prog_passes = 0;
        static bool scalar_prog_initialized = false;

        const size_t sp_cells = static_cast<size_t>(image_width) * static_cast<size_t>(image_height);
        const bool sp_dims_mismatch =
            scalar_prog_linear_accum.size() != sp_cells || image_height <= 0 || image_width <= 0;

        if (!scalar_prog_initialized || sp_dims_mismatch || progressive_view_changed) {
            scalar_prog_linear_accum.assign(sp_cells, Color(0, 0, 0));
            scalar_prog_passes = 0;
            scalar_prog_initialized = true;
        }

        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                float u = (i + random_float_pcg()) / (image_width - 1);
                float v = (j + random_float_pcg()) / (image_height - 1);
                Ray r = cam.get_ray(u, v);
                Color sample_color = ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                const size_t ix = static_cast<size_t>(j) * static_cast<size_t>(image_width) + static_cast<size_t>(i);
                scalar_prog_linear_accum[ix] = scalar_prog_linear_accum[ix] + sample_color;
            }
        }

        scalar_prog_passes++;

        const float inv_passes = 1.0f / static_cast<float>(scalar_prog_passes);
        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color linear =
                    scalar_prog_linear_accum[static_cast<size_t>(j) * static_cast<size_t>(image_width) +
                                             static_cast<size_t>(i)] *
                    inv_passes;
                Color pixel_color(std::sqrt(linear.x), std::sqrt(linear.y), std::sqrt(linear.z));
                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
            }
        }

        if (scalar_prog_passes < preset.samples) {
            schedule_next_render = true;
        }
        progressive_pass_count = scalar_prog_passes;
        progressive_cam_anchor = cam;
        progressive_cam_anchor_valid = true;
        progressive_last_max_depth = ray_renderer.max_depth;
    } else if (ray_renderer.enable_wavefront) {
        path_id = 4;
        std::vector<Color> wavefront_framebuffer(static_cast<size_t>(image_width) * static_cast<size_t>(image_height));
        ray_renderer.render_wavefront(cam, scene, wavefront_framebuffer, image_width, image_height, preset.samples);

        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color pixel_color =
                    wavefront_framebuffer[static_cast<size_t>(j) * static_cast<size_t>(image_width) +
                                          static_cast<size_t>(i)];
                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
            }
        }
    } else if (ray_renderer.enable_simd_packets) {
        path_id = 5;
        std::vector<Color> simd_framebuffer(static_cast<size_t>(image_width) * static_cast<size_t>(image_height));
        ray_renderer.render_simd_packets(cam, scene, simd_framebuffer, image_width, image_height, preset.samples);

        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color pixel_color =
                    simd_framebuffer[static_cast<size_t>(j) * static_cast<size_t>(image_width) + static_cast<size_t>(i)];
                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
            }
        }
    } else if (ray_renderer.enable_morton) {
        path_id = 7;
        std::vector<Color> morton_framebuffer(static_cast<size_t>(image_width) * static_cast<size_t>(image_height));
        ray_renderer.render_morton(cam, scene, morton_framebuffer, image_width, image_height, preset.samples);

        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color pixel_color =
                    morton_framebuffer[static_cast<size_t>(j) * static_cast<size_t>(image_width) +
                                        static_cast<size_t>(i)];
                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
            }
        }
    } else {
        #pragma omp parallel for schedule(dynamic, 16)
        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                Color pixel_color(0, 0, 0);
                float total_depth = 0.0f;
                Color pixel_normal(0, 0, 0);
                Color pixel_albedo(0, 0, 0);
                int shadow_rays = 0;
                int total_rays = 0;

                int actual_samples =
                    ray_renderer.enable_adaptive ? std::max(1, preset.samples / 2) : preset.samples;

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
                }

                float scale = 1.0f / actual_samples;
                pixel_color = pixel_color * scale;

                if (!ray_renderer.enable_adaptive) {
                    pixel_normal = pixel_normal * scale;
                    pixel_albedo = pixel_albedo * scale;
                    total_depth *= scale;
                    analysis.record_pixel(i, image_height - 1 - j, shadow_rays, total_rays, total_depth, pixel_normal, pixel_albedo);
                }

                Color final_color = analysis.get_analysis_color(i, image_height - 1 - j, pixel_color);
                final_color.x = std::sqrt(final_color.x);
                final_color.y = std::sqrt(final_color.y);
                final_color.z = std::sqrt(final_color.z);

                int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(final_color.x, 0.0f, 0.999f));
                framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(final_color.y, 0.0f, 0.999f));
                framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(final_color.z, 0.0f, 0.999f));
            }
        }
    }

    if (prof) {
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0_prof).count();
        static std::mutex prof_mutex;
        static double sum_ms[8]{};
        static uint64_t prof_cnt[8]{};
        const int pid = std::min(std::max(path_id, 0), 7);
        std::lock_guard<std::mutex> lk(prof_mutex);
        sum_ms[pid] += ms;
        prof_cnt[pid]++;
        if (prof_cnt[pid] % 64u == 0u) {
            std::cerr << "[RT_PROFILE] path " << pid
                      << " avg_ms=" << (sum_ms[pid] / static_cast<double>(prof_cnt[pid])) << std::endl;
        }
    }

    last_rt_settings_sig_execute = rt_settings_sig;

    return {schedule_next_render, progressive_pass_count};
}

struct CpuRenderThreadHub {
    std::mutex job_mutex;
    std::condition_variable job_cv;
    std::atomic<bool> quit{false};

    bool job_pending = false;
    Camera job_cam;
    QualityPreset job_preset{};
    int job_width = 0;
    int job_height = 0;
    bool job_enable_shadows = true;
    bool job_denoise = false;
    int job_denoise_strength = 0;
    int job_denoise_amount_percent = 70;
    int job_min_passes_to_display = 1;

    Scene* scene_ptr = nullptr;
    Renderer* renderer_ptr = nullptr;
    RenderAnalysis* analysis_ptr = nullptr;

    std::mutex result_mutex;
    bool result_ready = false;
    std::vector<unsigned char> result_rgb;
    double result_seconds = 0.0;
    bool result_schedule_next = false;
    std::atomic<bool> frameless_followup_requested{false};

    std::thread worker;

    void worker_loop() {
        while (!quit.load(std::memory_order_acquire)) {
            Camera cam;
            QualityPreset preset;
            int iw = 0;
            int ih = 0;
            bool es = true;
            bool denoise = false;
            int denoise_strength = 0;
            int denoise_amount_percent = 70;
            Scene* sc = nullptr;
            Renderer* rr = nullptr;
            RenderAnalysis* an = nullptr;

            {
                std::unique_lock<std::mutex> lk(job_mutex);
                job_cv.wait(lk, [&] { return job_pending || quit.load(std::memory_order_acquire); });
                if (quit.load(std::memory_order_acquire)) {
                    break;
                }
                cam = job_cam;
                preset = job_preset;
                iw = job_width;
                ih = job_height;
                es = job_enable_shadows;
                denoise = job_denoise;
                denoise_strength = job_denoise_strength;
                denoise_amount_percent = job_denoise_amount_percent;
                sc = scene_ptr;
                rr = renderer_ptr;
                an = analysis_ptr;
                job_pending = false;
            }

            std::vector<unsigned char> fb(static_cast<size_t>(iw) * static_cast<size_t>(ih) * 3u);
            auto t0 = std::chrono::high_resolution_clock::now();

            const CpuTracePassResult pass = execute_cpu_ray_tracing_pass(*sc, cam, preset, iw, ih, *rr, *an, es, fb);
            const bool sched = pass.schedule_next;
            const int prog_passes = pass.progressive_pass_count;
            const int min_disp = std::max(1, job_min_passes_to_display);
            const int sample_cap = std::max(1, preset.samples);
            const int disp_threshold = std::min(min_disp, sample_cap);
            const bool progressive_active = rr->enable_progressive && prog_passes > 0;
            const bool suppress_display = progressive_active && prog_passes < disp_threshold;

            if (suppress_display) {
                if (sched) {
                    frameless_followup_requested.store(true, std::memory_order_release);
                }
            } else {
                const bool skip_denoise_intermediate_progressive = rr->enable_progressive && sched;
                if (denoise && !skip_denoise_intermediate_progressive) {
                    apply_rgb24_denoise_pipeline(fb, iw, ih, denoise_strength, denoise_amount_percent);
                }
                auto t1 = std::chrono::high_resolution_clock::now();

                {
                    std::lock_guard<std::mutex> lk(result_mutex);
                    result_rgb = std::move(fb);
                    result_seconds = std::chrono::duration<double>(t1 - t0).count();
                    result_schedule_next = sched;
                    result_ready = true;
                }
            }
        }
    }

    bool has_identical_pending_job(const Camera& cam, const QualityPreset& preset_in) {
        std::lock_guard<std::mutex> lk(job_mutex);
        if (!job_pending) {
            return false;
        }
        if (progressive_view_differs(job_cam, cam)) {
            return false;
        }
        return same_quality_preset(job_preset, preset_in);
    }

    void submit_job(const Camera& cam, const QualityPreset& preset_in, int iw, int ih, bool enable_shadows,
                      bool denoise_after_trace, int denoise_strength_in, int denoise_amount_percent_in,
                      int min_passes_to_display) {
        std::lock_guard<std::mutex> lk(job_mutex);
        job_cam = cam;
        job_preset = preset_in;
        job_width = iw;
        job_height = ih;
        job_enable_shadows = enable_shadows;
        job_denoise = denoise_after_trace;
        job_denoise_strength = std::max(0, std::min(2, denoise_strength_in));
        job_denoise_amount_percent = std::max(0, std::min(100, denoise_amount_percent_in));
        job_min_passes_to_display = std::max(1, min_passes_to_display);
        job_pending = true;
        job_cv.notify_one();
    }

    // Returns true if a newly completed CPU frame was published (main thread should upload to SDL).
    bool try_consume(std::vector<unsigned char>& framebuffer, SDL_Texture* texture, int tex_width, int tex_height,
                     double* out_seconds, bool* schedule_next_out) {
        std::lock_guard<std::mutex> lk(result_mutex);
        const bool orphan_followup =
            frameless_followup_requested.exchange(false, std::memory_order_acq_rel);
        if (!result_ready) {
            *schedule_next_out = orphan_followup;
            return false;
        }
        const size_t expected = static_cast<size_t>(tex_width) * static_cast<size_t>(tex_height) * 3u;
        if (result_rgb.size() != expected) {
            // Stale frame from before a resolution change (or hub bug) — do not swap/blit.
            const bool pending_followup = result_schedule_next;
            result_rgb.clear();
            result_ready = false;
            *schedule_next_out = orphan_followup || pending_followup;
            return false;
        }
        framebuffer.swap(result_rgb);
        result_ready = false;
        *out_seconds = result_seconds;
        *schedule_next_out = result_schedule_next || orphan_followup;
        blit_packed_rgb24_to_texture(texture, framebuffer.data(), tex_width, tex_height);
        return true;
    }

    void start(Scene* s, Renderer* r, RenderAnalysis* a) {
        scene_ptr = s;
        renderer_ptr = r;
        analysis_ptr = a;
        // stop() leaves quit=true so the old worker exits; clear it before spawning again.
        quit.store(false, std::memory_order_release);
        frameless_followup_requested.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(result_mutex);
            result_ready = false;
            result_rgb.clear();
        }
        worker = std::thread([this] { worker_loop(); });
    }

    void stop() {
        quit.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(job_mutex);
            job_pending = true;
        }
        job_cv.notify_one();
        {
            std::lock_guard<std::mutex> lk(result_mutex);
            result_ready = false;
        }
        if (worker.joinable()) {
            worker.join();
        }
    }
};

// Crash handler signal handling
#include <signal.h>
#include <execinfo.h>

void crash_handler(int sig) {
    void *array[10];
    size_t size;

    // Get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // Print out all the frames to stderr
    fprintf(stderr, "\n=== CRASH DETECTED ===\n");
    fprintf(stderr, "Error: signal %d:\n", sig);

    // Print backtrace symbols
    char** strings = backtrace_symbols(array, size);
    if (strings) {
        for (size_t i = 0; i < size; i++) {
            fprintf(stderr, "  %s\n", strings[i]);
        }
        free(strings);
    }

    fprintf(stderr, "\nCurrent settings:\n");
    fprintf(stderr, "  Resolution: ??? x ???\n");
    fprintf(stderr, "  Samples: ???\n");
    fprintf(stderr, "  Depth: ???\n");
    fprintf(stderr, "\nThe ray tracer has crashed. This may be due to:\n");
    fprintf(stderr, "  - Samples per pixel too high for current resolution\n");
    fprintf(stderr, "  - Memory exhaustion\n");
    fprintf(stderr, "  - GPU driver issues (if using GPU renderer)\n");
    fprintf(stderr, "  - Known bugs in ray tracing code\n");
    fprintf(stderr, "\nTo prevent crashes:\n");
    fprintf(stderr, "  - Keep samples per pixel < 8\n");
    fprintf(stderr, "  - Use lower quality presets (1-3)\n");
    fprintf(stderr, "  - Switch to CPU renderer if GPU renderer crashes\n");
    fprintf(stderr, "\n======================\n\n");

    exit(1);
}

static std::atomic<bool> g_cpu_viewport_motion{false};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Install crash handler for interactive mode
    signal(SIGSEGV, crash_handler);   // Segmentation fault
    signal(SIGABRT, crash_handler);   // Abort
    signal(SIGFPE, crash_handler);    // Floating point exception
    signal(SIGILL, crash_handler);    // Illegal instruction

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

#if SDL_VERSION_ATLEAST(2, 0, 22)
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

    // Initial quality level
    int current_quality = 0; // Start at "Large Window (Default)" quality
    QualityPreset preset = quality_levels[current_quality];

    // Rendering feature toggles
    bool enable_shadows = true;
    bool enable_reflections = true;
    bool enable_morton = false;
    bool enable_stratified = false;
    bool enable_frustum = false;
    bool enable_denoiser = false;
    int denoise_strength = 0;
    int denoise_amount_percent = 70;
    int min_progressive_display_passes = 1;

    // Create window
    std::string window_title = "Real-time Ray Tracer - CPU (OpenMP)";

    // For GPU rendering, we need an OpenGL context
    #ifdef GPU_RENDERING
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        SDL_Window* window = SDL_CreateWindow(
            window_title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            1920,  // Always use large window size
            1080,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
    #else
        SDL_Window* window = SDL_CreateWindow(
            window_title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            1920,  // Always use large window size
            1080,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
    #endif

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create renderer and texture
    int image_width = preset.width;
    int image_height = static_cast<int>(preset.width / (16.0f / 9.0f));

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        image_width,
        image_height
    );

    // Set texture scaling to linear for smooth upscaling
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);

    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    // While the render resolution changes, we keep the last good frame here and stretch it to the
    // window until the worker produces the first frame at the new size (seamless transition).
    SDL_Texture* present_hold_texture = nullptr;

    // Setup scene - use shared Cornell Box scene
    Scene scene;
    setup_cornell_box_scene(scene);

    // Performance optimization: build spatial cache
    scene.optimize_spatial_layout();

    // Camera controller
    CameraController camera_controller;

    // Framebuffer
    std::vector<unsigned char> framebuffer(image_width * image_height * 3);

    // Initialize renderer
    Renderer ray_renderer(preset.max_depth);
    ray_renderer.enable_shadows = enable_shadows;
    ray_renderer.enable_reflections = enable_reflections;
    ray_renderer.enable_progressive = true;
    ray_renderer.enable_adaptive = true;
    ray_renderer.enable_simd_packets = true;
    ray_renderer.initialize_progressive(image_width, image_height);
    std::cout << "CPU Renderer initialized (OpenMP with " << omp_get_max_threads() << " threads; progressive, adaptive, SIMD on by default)" << std::endl;

#ifdef GPU_RENDERING
    // Initialize GPU renderer (but don't crash if it fails)
    RendererType current_renderer = RendererType::CPU;  // Default to CPU
    std::unique_ptr<GPURenderer> gpu_renderer = nullptr;

    // Note: GPU renderer requires OpenGL context, which isn't available
    // with the current SDL2 renderer setup. GPU rendering is disabled for now.
    std::cout << "GPU Renderer: Not available with current SDL2 setup" << std::endl;
    std::cout << "GPU Renderer: CPU rendering only" << std::endl;
#endif

    // Main loop
    bool running = true;
    bool paused = false;
    bool need_render = true;
    bool show_help = false;
    bool show_luma_histogram = false;
    time_t hot_reload_last_mtime = 0;
    bool hot_reload_have_mtime = false;
    const char* hot_reload_path = std::getenv("RT_HOT_RELOAD_CAMERA");
    if (!hot_reload_path || !hot_reload_path[0]) {
        hot_reload_path = "config/camera_hot_reload.txt";
    }

    // Initialize help overlay
    HelpOverlay help_overlay;
    if (help_overlay.init()) {
        std::cout << "Help overlay initialized (press H to toggle)\n";
    } else {
        std::cout << "Note: Help overlay unavailable (SDL_ttf not found)\n";
    }

    // Initialize controls panel
    ControlsPanel controls_panel;
    bool show_controls = false;  // Hide controls by default
    if (controls_panel.init()) {
        std::cout << "Controls panel initialized (press C to toggle)\n";
    } else {
        std::cout << "Note: Controls panel unavailable (SDL_ttf not found)\n";
        show_controls = false;
    }

    bool pending_window_screenshot = false;
    int pending_window_screenshot_delay_frames = 0;

    // Initialize render analysis system
    RenderAnalysis analysis;
    int analysis_height = preset.width * 9 / 16;
    analysis.resize(preset.width, analysis_height);
    std::cout << "Analysis system initialized (press M to cycle modes)\n";

    CpuRenderThreadHub cpu_render_hub;
    cpu_render_hub.start(&scene, &ray_renderer, &analysis);
    std::fill(framebuffer.begin(), framebuffer.end(), 0);
    blit_packed_rgb24_to_texture(texture, framebuffer.data(), image_width, image_height);

    // Mouse capture
    SDL_SetRelativeMouseMode(SDL_FALSE);

    // Frame timing
    auto last_frame_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps = 0.0f;

    std::cout << "\n";
    std::cout << "=== REAL-TIME RAY TRACER ===\n";
    std::cout << "Controls:\n";
    std::cout << "  WASD          - Move camera\n";
    std::cout << "  Arrow Keys    - Move up/down\n";
    std::cout << "  Mouse         - Look around (when captured)\n";
    std::cout << "  Left Click    - Capture/release mouse or click panel buttons\n";
    std::cout << "  1-6           - Change quality level\n";
    std::cout << "  C             - Toggle interactive controls panel\n";
    std::cout << "  H             - Toggle help overlay\n";
    std::cout << "  G             - Toggle diagnostics overlay (histogram + stats)\n";
    std::cout << "  F5            - Reload camera from config (or RT_HOT_RELOAD_CAMERA)\n";
    std::cout << "  R             - Toggle CPU/GPU renderer\n";
    std::cout << "  Space         - Pause rendering\n";
    std::cout << "  S             - Save window screenshot, no UI (screenshots/screenshot_*.png)\n";
    std::cout << "  ESC           - Quit\n";
    std::cout << "\n";
    std::cout << "Quality Levels (affects rendering samples, not window size):\n";
    for (int i = 0; i < NUM_QUALITY_LEVELS; i++) {
        std::cout << "  " << (i + 1) << ". " << quality_levels[i].name << "\n";
    }
    std::cout << "\n";

    while (running) {
        bool cpu_followup = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_h:
                        show_help = !show_help;
                        break;
                    case SDLK_g:
                        show_luma_histogram = !show_luma_histogram;
                        std::cout << "Diagnostics overlay: " << (show_luma_histogram ? "ON" : "OFF") << std::endl;
                        break;
                    case SDLK_F5: {
                        struct stat st;
                        if (stat(hot_reload_path, &st) == 0) {
                            hot_reload_last_mtime = st.st_mtime;
                            hot_reload_have_mtime = true;
                        }
                        camera_controller.try_load_hot_reload_file(hot_reload_path);
                        need_render = true;
                        break;
                    }
                    case SDLK_c:
                        show_controls = !show_controls;
                        break;
                    case SDLK_m: {  // Cycle analysis modes
                        int mode = static_cast<int>(analysis.get_mode());
                        mode = (mode + 1) % 4;  // 4 analysis modes
                        analysis.set_mode(static_cast<AnalysisMode>(mode));
                        std::cout << "Analysis mode: " << analysis.get_mode_name() << std::endl;
                        need_render = true;
                        break;
                    }
#ifdef GPU_RENDERING
                    case SDLK_r: {  // Toggle CPU/GPU renderer
                        if (gpu_renderer) {
                            if (current_renderer == RendererType::CPU) {
                                current_renderer = RendererType::GPU;
                                window_title = "Real-time Ray Tracer - GPU (Compute Shaders)";
                                std::cout << "Switched to GPU renderer" << std::endl;
                            } else {
                                current_renderer = RendererType::CPU;
                                window_title = "Real-time Ray Tracer - CPU (OpenMP)";
                                std::cout << "Switched to CPU renderer" << std::endl;
                            }
                            SDL_SetWindowTitle(window, window_title.c_str());
                            need_render = true;
                        } else {
                            std::cout << "GPU renderer not available" << std::endl;
                        }
                        break;
                    }
#endif
                    case SDLK_SPACE:
                        paused = !paused;
                        std::cout << (paused ? "Paused" : "Resumed") << std::endl;
                        break;
                    case SDLK_s:  // Deferred: read back after scene draw (see pending_window_screenshot)
                        pending_window_screenshot = true;
                        pending_window_screenshot_delay_frames = 1;
                        break;
                    case SDLK_1: case SDLK_2: case SDLK_3:
                    case SDLK_4: case SDLK_5: case SDLK_6: {
                        int new_quality = event.key.keysym.sym - SDLK_1;
                        if (new_quality != current_quality) {
                            // Safety check for high quality levels
                            QualityPreset new_preset = quality_levels[new_quality];
                            if (!is_quality_safe(new_preset)) {
                                int height = new_preset.width * 9 / 16;
                                int pixels = new_preset.width * height;
                                int rays = pixels * new_preset.samples;
                                int memory_mb = estimate_memory_mb(new_preset.width, height, new_preset.samples);

                                std::cout << "\nWARNING: High quality setting!\n";
                                std::cout << "  Resolution: " << new_preset.width << "x" << height << "\n";
                                std::cout << "  Total rays: " << rays << " (" << (rays / 1000000) << " MRays)\n";
                                std::cout << "  Est. memory: " << memory_mb << " MB\n";
                                std::cout << "  This may take 10-20 seconds per frame.\n";
                                std::cout << "  Press " << (new_quality + 1) << " again to confirm, or other key to cancel.\n";

                                // Wait for confirmation (simple debounce)
                                SDL_Event confirm_event;
                                bool confirmed = false;
                                auto start_time = std::chrono::steady_clock::now();
                                while (std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start_time).count() < 3000) {
                                    if (SDL_PollEvent(&confirm_event)) {
                                        if (confirm_event.type == SDL_KEYDOWN) {
                                            int confirm_key = confirm_event.key.keysym.sym - SDLK_1;
                                            if (confirm_key == new_quality) {
                                                confirmed = true;
                                                break;
                                            } else {
                                                std::cout << "Cancelled.\n";
                                                break;
                                            }
                                        }
                                    }
                                    SDL_Delay(10);
                                }

                                if (!confirmed) {
                                    std::cout << "Quality change cancelled.\n";
                                    break;
                                }
                                std::cout << "Confirmed! Applying high quality setting...\n";
                            }

                            current_quality = new_quality;
                            preset = quality_levels[current_quality];

                            // Update CPU renderer (keep window size, only change quality)
                            cpu_render_hub.stop();
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
                            cpu_render_hub.start(&scene, &ray_renderer, &analysis);
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
                            need_render = true;
                        }
                        break;
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                // Handle mouse wheel scrolling for controls panel
                if (show_controls && SDL_GetRelativeMouseMode() != SDL_TRUE) {
                    int scroll_delta = event.wheel.y * 30;  // Scroll 30 pixels per wheel click
                    controls_panel.scroll(scroll_delta);
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    const bool mouse_look = (SDL_GetRelativeMouseMode() == SDL_TRUE);
                    // Check if clicking on controls panel (ignore panel toggles while mouselook is active)
                    if (show_controls && !mouse_look) {
                        auto click_result = controls_panel.handle_click(event.button.x, event.button.y);
                        if (click_result.quality_changed) {
                            current_quality = click_result.new_quality;
                            preset = quality_levels[current_quality];
                            cpu_render_hub.stop();
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
                            cpu_render_hub.start(&scene, &ray_renderer, &analysis);
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
                            need_render = true;
                        } else if (click_result.samples_changed) {
                            // Safety check for samples
                            int new_samples = click_result.new_samples;
                            if (!is_samples_safe(new_samples, preset.width)) {
                                int height = preset.width * 9 / 16;
                                int pixels = preset.width * height;
                                int rays = pixels * new_samples;
                                std::cout << "\nWARNING: UNSAFE SAMPLES SETTING!\n";
                                std::cout << "  Requested: " << new_samples << " samples\n";
                                std::cout << "  Current resolution: " << preset.width << "x" << height << "\n";
                                std::cout << "  Total rays would be: " << rays << " (" << (rays / 1000000) << " MRays)\n";
                                std::cout << "  This exceeds safe limits for interactive use.\n";
                                std::cout << "  Samples limited to < 8 for stability.\n";
                                std::cout << "  Change rejected. Please choose a lower value.\n";
                            } else {
                                preset.samples = new_samples;
                                std::cout << "Samples: " << preset.samples << std::endl;
                                need_render = true;
                            }
                        } else if (click_result.depth_changed) {
                            // Only update max_depth in place — do not stop/join the worker thread here.
                            // Reconstructing Renderer + hub restart blocked the UI until the current trace finished,
                            // which feels like a freeze when traces are long (e.g. higher bounce limits).
                            preset.max_depth = click_result.new_depth;
                            ray_renderer.max_depth = preset.max_depth;
                            std::cout << "Depth: " << preset.max_depth << std::endl;
                            need_render = true;
                        } else if (click_result.shadows_changed) {
                            enable_shadows = !enable_shadows;
                            ray_renderer.enable_shadows = enable_shadows;
                            std::cout << "Shadows: " << (enable_shadows ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.reflections_changed) {
                            enable_reflections = !enable_reflections;
                            ray_renderer.enable_reflections = enable_reflections;
                            std::cout << "Reflections: " << (enable_reflections ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.analysis_mode_changed) {
                            // Specific analysis mode selected - only if different from current
                            AnalysisMode new_mode = static_cast<AnalysisMode>(click_result.new_analysis_mode);
                            if (new_mode != analysis.get_mode()) {
                                analysis.set_mode(new_mode);
                                std::cout << "Analysis mode: " << analysis.get_mode_name() << std::endl;
                                need_render = true;
                            }
                        } else if (click_result.screenshot_requested) {
                            pending_window_screenshot = true;
                            pending_window_screenshot_delay_frames = 1;
                        } else if (click_result.progressive_changed) {
                            // Toggle progressive rendering
                            ray_renderer.enable_progressive = !ray_renderer.enable_progressive;
                            if (ray_renderer.enable_progressive) {
                                ray_renderer.initialize_progressive(image_width, image_height);
                                std::cout << "Progressive rendering: ON" << std::endl;
                            } else {
                                ray_renderer.reset_progressive();
                                std::cout << "Progressive rendering: OFF" << std::endl;
                            }
                            need_render = true;
                        } else if (click_result.adaptive_changed) {
                            // Toggle adaptive sampling
                            ray_renderer.enable_adaptive = !ray_renderer.enable_adaptive;
                            std::cout << "Adaptive sampling: " << (ray_renderer.enable_adaptive ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.denoiser_changed) {
                            enable_denoiser = !enable_denoiser;
                            std::cout << "Post denoise (CPU worker): " << (enable_denoiser ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.denoise_strength_changed) {
                            denoise_strength = std::max(0, std::min(2, click_result.new_denoise_strength));
                            std::cout << "Denoise strength: "
                                      << (denoise_strength == 0 ? "Light" : (denoise_strength == 1 ? "Med" : "Strong"))
                                      << std::endl;
                            need_render = true;
                        } else if (click_result.denoise_amount_changed) {
                            denoise_amount_percent = std::max(0, std::min(100, click_result.new_denoise_amount_percent));
                            std::cout << "Denoise amount: " << denoise_amount_percent << "%" << std::endl;
                            need_render = true;
                        } else if (click_result.min_prog_display_changed) {
                            min_progressive_display_passes = click_result.new_min_prog_display_passes;
                            std::cout << "Progressive min passes before display: " << min_progressive_display_passes
                                      << std::endl;
                            need_render = true;
                        } else if (click_result.dof_aperture_changed) {
                            camera_controller.aperture =
                                static_cast<float>(click_result.new_dof_aperture_milli) / 1000.0f;
                            std::cout << "DOF lens radius: " << camera_controller.aperture << std::endl;
                            need_render = true;
                        } else if (click_result.dof_focus_changed) {
                            camera_controller.dist_to_focus =
                                static_cast<float>(click_result.new_dof_focus_hundredths) / 100.0f;
                            if (camera_controller.dist_to_focus < 0.05f) {
                                camera_controller.dist_to_focus = 0.05f;
                            }
                            std::cout << "DOF focus distance: " << camera_controller.dist_to_focus << std::endl;
                            need_render = true;
                        } else if (click_result.wavefront_changed) {
                            ray_renderer.enable_wavefront = !ray_renderer.enable_wavefront;
                            if (ray_renderer.enable_wavefront && ray_renderer.enable_simd_packets) {
                                ray_renderer.enable_simd_packets = false;
                                std::cout << "SIMD disabled (SIMD and Wavefront use different render paths; pick one)\n";
                            }
                            std::cout << "Wavefront rendering: " << (ray_renderer.enable_wavefront ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.morton_changed) {
                            // Toggle Morton Z-curve ordering (scalar path; turn off packet paths so it takes effect)
                            enable_morton = !enable_morton;
                            ray_renderer.enable_morton = enable_morton;
                            if (enable_morton) {
                                if (ray_renderer.enable_wavefront) {
                                    ray_renderer.enable_wavefront = false;
                                    std::cout << "Wavefront OFF (Morton uses scalar traversal)\n";
                                }
                                if (ray_renderer.enable_simd_packets) {
                                    ray_renderer.enable_simd_packets = false;
                                    std::cout << "SIMD packets OFF (Morton uses scalar traversal)\n";
                                }
                            }
                            std::cout << "Morton Z-curve: " << (enable_morton ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.stratified_changed) {
                            // Toggle stratified sampling
                            enable_stratified = !enable_stratified;
                            ray_renderer.enable_stratified = enable_stratified;
                            std::cout << "Stratified sampling: " << (enable_stratified ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.frustum_changed) {
                            // Toggle frustum culling
                            enable_frustum = !enable_frustum;
                            ray_renderer.enable_frustum = enable_frustum;
                            std::cout << "Frustum culling: " << (enable_frustum ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.simd_packets_changed) {
                            std::cout << "SIMD button clicked - current state: " << (ray_renderer.enable_simd_packets ? "ON" : "OFF") << std::endl;
                            std::cout << "Button value: " << click_result.new_simd_packets << std::endl;

                            if (!ray_renderer.enable_simd_packets) {
                                ray_renderer.enable_simd_packets = true;
                                if (ray_renderer.enable_wavefront) {
                                    ray_renderer.enable_wavefront = false;
                                    std::cout << "Wavefront disabled (SIMD and Wavefront use different render paths; pick one)\n";
                                }
                            } else {
                                ray_renderer.enable_simd_packets = false;
                            }
                            std::cout << "SIMD packets: " << (ray_renderer.enable_simd_packets ? "ON (AVX2 intersection)" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.bvh_changed) {
                            if (!ray_renderer.enable_bvh) {
                                ray_renderer.enable_bvh = true;
                                ray_renderer.build_bvh(scene);
                            } else {
                                ray_renderer.enable_bvh = false;
                                ray_renderer.release_bvh();
                            }
                            std::cout << "BVH: " << (ray_renderer.enable_bvh ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.resolution_changed) {
                            int new_width = click_result.new_resolution;
                            if (!is_samples_safe(preset.samples, new_width)) {
                                std::cout << "WARNING: Unsafe resolution for current samples. Please reduce samples first.\n";
                            } else {
                                int old_width = image_width;
                                int old_height = image_height;

                                preset.width = new_width;
                                image_width = preset.width;
                                image_height = static_cast<int>(preset.width / (16.0f / 9.0f));

                                std::cout << "=== RESOLUTION CHANGE ===\n";
                                std::cout << "Rendering resolution: " << image_width << "x" << image_height << "\n";
                                std::cout << "Window size stays: " << old_width << "x" << old_height << " (scaled to fit)\n";
                                std::cout << "Total pixels: " << (image_width * image_height) << "\n";
                                std::cout << "========================\n";

                                // Stop worker first so no in-flight frame can publish with the old
                                // buffer size while we change texture dimensions (avoids one-frame garbage).
                                cpu_render_hub.stop();

                                // Park the current render in a hold texture (stretched to the window) until
                                // the first new-resolution frame is ready — avoids a black / empty flash.
                                if (present_hold_texture) {
                                    SDL_DestroyTexture(present_hold_texture);
                                    present_hold_texture = nullptr;
                                }
                                if (texture) {
                                    present_hold_texture = texture;
                                    texture = nullptr;
                                }

                                texture = SDL_CreateTexture(
                                    renderer,
                                    SDL_PIXELFORMAT_RGB24,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    image_width,
                                    image_height
                                );

                                // Set texture scaling to linear for smooth upscaling
                                SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
                                if (texture) {
                                    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
                                }

                                if (!texture) {
                                    std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
                                    SDL_DestroyRenderer(renderer);
                                    SDL_DestroyWindow(window);
                                    SDL_Quit();
                                    return 1;
                                }

                                // Resize framebuffer for new resolution
                                framebuffer.resize(image_width * image_height * 3, 0);

                                // Resize analysis buffers
                                analysis.resize(image_width, image_height);

                                cpu_render_hub.start(&scene, &ray_renderer, &analysis);
                                if (ray_renderer.enable_progressive) {
                                    ray_renderer.initialize_progressive(image_width, image_height);
                                }
                                std::fill(framebuffer.begin(), framebuffer.end(), 0);
                                // Defer blitting to the new texture until the worker has a frame (hold covers the gap).

                                std::cout << "Resolution: " << image_width << "x" << image_height << "\n" << std::endl;
                                need_render = true;
                            }
                        } else {
                            int ww = 0, wh = 0;
                            SDL_GetWindowSize(window, &ww, &wh);
                            if (!controls_panel.point_is_over_panel(event.button.x, event.button.y, ww, wh)) {
                                SDL_bool captured = SDL_GetRelativeMouseMode();
                                SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                            }
                        }
                    } else if (show_controls && mouse_look) {
                        // Any click exits look mode (button x/y is unreliable for panel hit tests in relative mode).
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    } else {
                        // Controls panel not visible, toggle mouse capture
                        SDL_bool captured = SDL_GetRelativeMouseMode();
                        SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    controls_panel.release_pressed_buttons();
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (SDL_GetRelativeMouseMode()) {
                    camera_controller.rotate(
                        event.motion.xrel * 0.1f,
                        -event.motion.yrel * 0.1f
                    );
                    need_render = true;
                    g_cpu_viewport_motion.store(true, std::memory_order_relaxed);
                }
            }
        }

        // Handle continuous keyboard input
        const Uint8* keystates = SDL_GetKeyboardState(NULL);
        float move_speed = 0.15f;  // Faster movement for larger room

        if (keystates[SDL_SCANCODE_W]) {
            camera_controller.move_forward(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_S]) {
            camera_controller.move_forward(-move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_A]) {
            camera_controller.move_right(-move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_D]) {
            camera_controller.move_right(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_UP]) {
            camera_controller.move_up(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_DOWN]) {
            camera_controller.move_up(-move_speed);
            need_render = true;
        }

        // Render frame if needed and not paused (GPU: synchronous). CPU ray trace runs on worker thread.
        double render_time = 0.0;
        bool work_done = false;

        if (need_render && !paused) {
            if (!validate_current_settings(preset)) {
                std::cout << "Render aborted due to unsafe settings. Please adjust quality.\n";
                need_render = false;
                continue;
            }

            Camera cam = camera_controller.get_camera();

#ifdef GPU_RENDERING
            if (current_renderer == RendererType::GPU && gpu_renderer) {
                auto render_start = std::chrono::high_resolution_clock::now();
                std::cout << "GPU rendering..." << std::endl;

                gpu_renderer->resize(image_width, image_height);

                std::vector<std::vector<Color>> gpu_framebuffer;
                gpu_renderer->render(cam, gpu_framebuffer);

                #pragma omp parallel for schedule(dynamic, 4)
                for (int j = image_height - 1; j >= 0; --j) {
                    for (int i = 0; i < image_width; ++i) {
                        Color color = gpu_framebuffer[image_height - 1 - j][i];
                        color.x = sqrt(color.x);
                        color.y = sqrt(color.y);
                        color.z = sqrt(color.z);
                        float r = color.x > 1.0f ? 1.0f : (color.x < 0.0f ? 0.0f : color.x);
                        float g = color.y > 1.0f ? 1.0f : (color.y < 0.0f ? 0.0f : color.y);
                        float b = color.z > 1.0f ? 1.0f : (color.z < 0.0f ? 0.0f : color.z);
                        int ir = static_cast<int>(255.999 * r);
                        int ig = static_cast<int>(255.999 * g);
                        int ib = static_cast<int>(255.999 * b);
                        size_t pixel_idx = ((image_height - 1 - j) * image_width + i) * 3;
                        framebuffer[pixel_idx + 0] = static_cast<unsigned char>(ir);
                        framebuffer[pixel_idx + 1] = static_cast<unsigned char>(ig);
                        framebuffer[pixel_idx + 2] = static_cast<unsigned char>(ib);
                    }
                }

                std::cout << "GPU render complete" << std::endl;
                blit_packed_rgb24_to_texture(texture, framebuffer.data(), image_width, image_height);
                auto render_end = std::chrono::high_resolution_clock::now();
                render_time = std::chrono::duration<double>(render_end - render_start).count();
                work_done = true;
                need_render = false;
                if (present_hold_texture) {
                    SDL_DestroyTexture(present_hold_texture);
                    present_hold_texture = nullptr;
                }
            } else
#endif
            {
                const Uint8* ks_motion = SDL_GetKeyboardState(nullptr);
                bool viewport_user_motion = g_cpu_viewport_motion.exchange(false);
                viewport_user_motion = viewport_user_motion || ks_motion[SDL_SCANCODE_W] || ks_motion[SDL_SCANCODE_S] ||
                                        ks_motion[SDL_SCANCODE_A] || ks_motion[SDL_SCANCODE_D] ||
                                        ks_motion[SDL_SCANCODE_UP] || ks_motion[SDL_SCANCODE_DOWN];
                int min_disp_submit = min_progressive_display_passes;
                if (ray_renderer.enable_progressive && viewport_user_motion) {
                    min_disp_submit = 1;
                }
                if (!cpu_render_hub.has_identical_pending_job(cam, preset)) {
                    cpu_render_hub.submit_job(cam, preset, image_width, image_height, enable_shadows, enable_denoiser,
                                              denoise_strength, denoise_amount_percent, min_disp_submit);
                }
                need_render = false;
            }
        }

        {
            double cpu_rt = 0.0;
            if (cpu_render_hub.try_consume(framebuffer, texture, image_width, image_height, &cpu_rt, &cpu_followup)) {
                render_time = cpu_rt;
                work_done = true;
                if (present_hold_texture) {
                    SDL_DestroyTexture(present_hold_texture);
                    present_hold_texture = nullptr;
                }
            }
            if (cpu_followup) {
                need_render = true;
            }
        }

        if (work_done) {
            frame_count++;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_frame_time).count();
            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                frame_count = 0;
                last_frame_time = now;
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                         << " | Render: " << std::setprecision(3) << render_time << "s"
                         << " | Quality: " << preset.name
                         << " | Cam: " << camera_controller.get_position()
                         << "     " << std::flush;
            }
        }

        // Copy texture to renderer (stretch last frame across the window while recomputing at a new resolution)
        SDL_RenderClear(renderer);
        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        if (present_hold_texture) {
            SDL_Rect dst = {0, 0, window_width, window_height};
            SDL_RenderCopy(renderer, present_hold_texture, nullptr, &dst);
        } else if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        }

        if (pending_window_screenshot) {
            if (pending_window_screenshot_delay_frames > 0) {
                --pending_window_screenshot_delay_frames;
            } else {
                pending_window_screenshot = false;
                system("mkdir -p screenshots");
                const auto now = std::chrono::system_clock::now();
                const std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::stringstream ss;
                ss << "screenshots/screenshot_" << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S") << ".png";
                const std::string path = ss.str();
                if (!save_window_scene_readback_png(renderer, path.c_str())) {
                    std::cerr << "Window screenshot failed (try again next frame)\n";
                }
            }
        }

        if (show_luma_histogram && !framebuffer.empty() && image_width > 0 && image_height > 0) {
            uint32_t hbins[64];
            uint32_t r_bins[64], g_bins[64], b_bins[64];
            float row_luma[kCpuDiagProfileN];
            float col_luma[kCpuDiagProfileN];
            rgb24_post::histogram_luminance_64(framebuffer, image_width, image_height, hbins);
            rgb24_post::histogram_rgb_channels_64(framebuffer, image_width, image_height, r_bins, g_bins, b_bins);
            rgb24_post::luminance_row_profile(framebuffer, image_width, image_height, kCpuDiagProfileN, row_luma);
            rgb24_post::luminance_col_profile(framebuffer, image_width, image_height, kCpuDiagProfileN, col_luma);
            float mr = 0.f, mg = 0.f, mb = 0.f, ml = 0.f, lmin = 0.f, lmax = 0.f;
            rgb24_post::rgb24_mean_luminance_bounds(framebuffer, image_width, image_height, mr, mg, mb, ml, lmin, lmax);

            TTF_Font* diag_title = nullptr;
            TTF_Font* diag_body = nullptr;
            if (controls_panel.has_overlay_fonts()) {
                diag_title = controls_panel.overlay_title_font();
                diag_body = controls_panel.overlay_body_font();
            } else if (help_overlay.has_overlay_fonts()) {
                diag_title = help_overlay.overlay_title_font();
                diag_body = help_overlay.overlay_body_font();
            }

            char pathbuf[128];
            int pb = 0;
            if (ray_renderer.enable_progressive)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "Prog ");
            if (ray_renderer.enable_adaptive)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "Adapt ");
            if (ray_renderer.enable_simd_packets)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "SIMD ");
            if (ray_renderer.enable_wavefront)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "WF ");
            if (enable_morton)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "Morton ");
            if (enable_stratified)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "Strat ");
            if (ray_renderer.enable_bvh)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "BVH ");
            if (enable_frustum)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "Frus ");
            if (enable_denoiser)
                pb += snprintf(pathbuf + pb, sizeof(pathbuf) - static_cast<size_t>(pb), "Denoise ");
            while (pb > 0 && pathbuf[pb - 1] == ' ') {
                pathbuf[--pb] = '\0';
            }
            if (pb == 0) {
                snprintf(pathbuf, sizeof(pathbuf), "defaults");
            }

            render_diagnostics_g_overlay(renderer, diag_title, diag_body, hbins, r_bins, g_bins, b_bins, row_luma,
                                       col_luma, window_width, window_height, image_width, image_height, preset.samples,
                                       preset.max_depth, fps, render_time, analysis.get_mode_name(), mr, mg, mb, ml, lmin,
                                       lmax, pathbuf, paused);
        }

        // Render controls panel if active
        if (show_controls) {
            const char* mode_name = analysis.get_mode_name();
#ifdef GPU_RENDERING
            controls_panel.render(renderer, window_width, window_height,
                                 current_quality, preset, fps, render_time, mode_name, enable_shadows, enable_reflections,
                                 ray_renderer.enable_progressive, ray_renderer.enable_adaptive, enable_denoiser,
                                 denoise_strength, denoise_amount_percent, min_progressive_display_passes,
                                 ray_renderer.enable_wavefront,
                                 enable_morton, enable_stratified, enable_frustum, ray_renderer.enable_simd_packets,
                                 ray_renderer.enable_bvh,
                                 camera_controller.aperture, camera_controller.dist_to_focus,
                                 current_renderer);
#else
            controls_panel.render(renderer, window_width, window_height,
                                 current_quality, preset, fps, render_time, mode_name, enable_shadows, enable_reflections,
                                 ray_renderer.enable_progressive, ray_renderer.enable_adaptive, enable_denoiser,
                                 denoise_strength, denoise_amount_percent, min_progressive_display_passes,
                                 ray_renderer.enable_wavefront,
                                 enable_morton, enable_stratified, enable_frustum, ray_renderer.enable_simd_packets,
                                 ray_renderer.enable_bvh,
                                 camera_controller.aperture, camera_controller.dist_to_focus);
#endif
        }

        // Render help overlay if active
        if (show_help) {
            help_overlay.render(renderer, window_width, window_height);
        }

        SDL_RenderPresent(renderer);

        if (std::getenv("RT_HOT_RELOAD_POLL")) {
            static int hr_poll_i = 0;
            if (++hr_poll_i >= 90) {
                hr_poll_i = 0;
                struct stat st;
                if (stat(hot_reload_path, &st) == 0) {
                    if (!hot_reload_have_mtime) {
                        hot_reload_last_mtime = st.st_mtime;
                        hot_reload_have_mtime = true;
                        if (camera_controller.try_load_hot_reload_file(hot_reload_path)) {
                            need_render = true;
                        }
                    } else if (st.st_mtime != hot_reload_last_mtime) {
                        hot_reload_last_mtime = st.st_mtime;
                        if (camera_controller.try_load_hot_reload_file(hot_reload_path)) {
                            need_render = true;
                        }
                    }
                }
            }
        }

        const bool idle_cpu = !work_done && !need_render && !cpu_followup && !paused;
        SDL_Delay(idle_cpu ? 14 : 2);
    }

    // Cleanup
    cpu_render_hub.stop();
    if (present_hold_texture) {
        SDL_DestroyTexture(present_hold_texture);
        present_hold_texture = nullptr;
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "\n\n=== Exiting ===\n";
    std::cout << "Final Stats:\n";
    std::cout << "  Resolution: " << image_width << "x" << image_height << "\n";
    std::cout << "  Samples: " << preset.samples << "\n";
    std::cout << "  Max Depth: " << preset.max_depth << "\n";

    return 0;
}
