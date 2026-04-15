#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <limits>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <omp.h>
#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "math/vec3.h"
#include "math/ray.h"
#include "math/morton.h"
#include "math/stratified.h"
#include "math/frustum.h"

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
    {1920, 1, 3, "Large Window (Default)"},  // Larger default window
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

// Save CPU renderer texture to PNG file
void save_cpu_screenshot(SDL_Renderer* renderer, SDL_Texture* texture, int width, int height, const char* filename) {
    (void)texture;  // Unused parameter - we read from renderer instead
    // Allocate buffer for pixel data
    std::vector<unsigned char> pixels(width * height * 3);

    // Read pixels from texture
    SDL_Rect rect = {0, 0, width, height};
    SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGB24, pixels.data(), width * 3);

    // Flip vertically (SDL has top-left origin, but texture might be flipped)
    std::vector<unsigned char> flipped_pixels(width * height * 3);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int src_idx = ((height - 1 - y) * width + x) * 3;
            int dst_idx = (y * width + x) * 3;
            flipped_pixels[dst_idx + 0] = pixels[src_idx + 0]; // R
            flipped_pixels[dst_idx + 1] = pixels[src_idx + 1]; // G
            flipped_pixels[dst_idx + 2] = pixels[src_idx + 2]; // B
        }
    }

    // Save as PNG
    stbi_write_png(filename, width, height, 3, flipped_pixels.data(), width * 3);
    std::cout << "Screenshot saved: " << filename << std::endl;
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
};

// Helper function for random float in [0, 1)
inline float random_float() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

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
        text_color = {20, 20, 20, 255};        // Dark text for light background
        background_color = {200, 200, 200, 180}; // Light gray transparent
        title_color = {200, 50, 50, 255};        // Dark red titles
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
            font = TTF_OpenFont(font_paths[i], 13);  // Smaller font
            if (font) break;
        }

        // Try same fonts for title (larger)
        for (int i = 0; font_paths[i] != nullptr; ++i) {
            title_font = TTF_OpenFont(font_paths[i], 18);  // Smaller title font
            if (title_font) break;
        }

        if (!font || !title_font) {
            std::cerr << "Failed to load fonts, help overlay unavailable" << std::endl;
            return false;
        }

        initialized = true;
        return true;
    }

    void render(SDL_Renderer* renderer, int window_width, int window_height) {
        if (!initialized || !font || !title_font) return;

        // Create smaller semi-transparent background
        SDL_Rect overlay_rect = {
            (window_width - 460) / 2,
            (window_height - 340) / 2,
            460,
            340
        };

        SDL_Surface* surface = SDL_CreateRGBSurface(0, overlay_rect.w, overlay_rect.h, 32, 0, 0, 0, 0);
        if (!surface) return;

        // Fill background
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 200, 200, 200, 180));

        // Render title
        const char* title_text = "HELP";
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {(460 - title_surface->w) / 2, 12, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        // Render controls text (condensed)
        const char* controls_text[] = {
            "Click window to capture mouse for looking around",
            "MOVE: WASD + Arrows | LOOK: Mouse",
            "1-6: Quality | M: Analysis | SPACE: Pause",
            "S: Screenshot | C: Controls Panel | H: Help | ESC: Quit"
        };

        int y_offset = 50;
        for (size_t i = 0; i < sizeof(controls_text) / sizeof(controls_text[0]); ++i) {
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, controls_text[i], text_color);

            if (text_surface) {
                SDL_Rect text_rect = {(460 - text_surface->w) / 2, y_offset, text_surface->w, text_surface->h};
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }
            y_offset += 40;  // More compact spacing
        }

        // Add footer
        const char* footer = "Press H to close";
        SDL_Surface* footer_surface = TTF_RenderText_Blended(font, footer, text_color);
        if (footer_surface) {
            SDL_Rect footer_rect = {(460 - footer_surface->w) / 2, 280, footer_surface->w, footer_surface->h};
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
        int category; // 0: quality, 1: samples, 2: depth, 3: shadows, 4: reflections, 5: resolution, 6: debug
    };
    std::vector<Button> buttons;
    int panel_x, panel_y;

    // Scroll functionality
    int scroll_offset;
    int max_scroll_offset;
    int content_height;
    bool is_scrollable;

public:
    ControlsPanel() : font(nullptr), title_font(nullptr), initialized(false), panel_x(0), panel_y(0),
                      scroll_offset(0), max_scroll_offset(0), content_height(0), is_scrollable(false) {
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

    void render(SDL_Renderer* renderer, int window_width, int window_height,
                int quality_idx, const QualityPreset& preset, double fps, double render_time,
                const char* analysis_mode_name = nullptr, bool enable_shadows = true, bool enable_reflections = true,
                bool enable_progressive = false, bool enable_adaptive = false, bool enable_wavefront = false,
                bool enable_morton = false, bool enable_stratified = false, bool enable_frustum = false
#ifdef GPU_RENDERING
                , RendererType current_renderer = RendererType::CPU
#endif
    ) {
        (void)window_height;  // Only used to calculate aspect ratio
        if (!initialized || !font || !title_font) return;

        // Panel positioned in top-right corner, scales with window size
        int panel_width = std::min(360, window_width - 20);
        int panel_height = std::min(700, window_height - 20);  // Fixed visible height
        panel_x = window_width - panel_width - 10;
        panel_y = 10;
        SDL_Rect overlay_rect = {panel_x, panel_y, panel_width, panel_height};

        // Create a larger surface for scrollable content
        int content_surface_height = 1200;  // Maximum content height
        SDL_Surface* content_surface = SDL_CreateRGBSurface(0, panel_width, content_surface_height, 32, 0, 0, 0, 0);
        if (!content_surface) return;

        // Fill content background
        SDL_FillRect(content_surface, nullptr, SDL_MapRGBA(content_surface->format, 50, 50, 60, 230));

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
        SDL_FillRect(content_surface, &separator, SDL_MapRGBA(content_surface->format, 100, 100, 120, 255));

        int y_offset = 50;
        const int line_height = 28;
        buttons.clear(); // Clear previous buttons

        // Helper lambda to render label-value pairs
        auto render_setting = [&](const char* label, const char* value) {
            SDL_Surface* label_surface = TTF_RenderText_Blended(font, label, text_color);
            if (label_surface) {
                SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
                SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
                SDL_FreeSurface(label_surface);
            }

            SDL_Surface* value_surface = TTF_RenderText_Blended(font, value, value_color);
            if (value_surface) {
                SDL_Rect value_rect = {panel_width - value_surface->w - 15, y_offset, value_surface->w, value_surface->h};
                SDL_BlitSurface(value_surface, nullptr, surface, &value_rect);
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
            SDL_BlitSurface(renderer_surface, nullptr, surface, &renderer_rect);
            SDL_FreeSurface(renderer_surface);

            // Render the actual mode with color
            SDL_Surface* mode_surface = TTF_RenderText_Blended(font, renderer_mode, renderer_color);
            if (mode_surface) {
                SDL_Rect mode_rect = {70, y_offset, mode_surface->w, mode_surface->h};
                SDL_BlitSurface(mode_surface, nullptr, surface, &mode_rect);
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
                button_bg = SDL_MapRGBA(surface->format, 100, 150, 200, 255);
            } else {
                button_bg = SDL_MapRGBA(surface->format, 70, 70, 90, 255);
            }
            SDL_FillRect(surface, &button_rect, button_bg);

            // Draw button border
            SDL_Rect border = {button_rect.x, button_rect.y, button_rect.w, 2};
            SDL_FillRect(surface, &border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
            border = {button_rect.x, button_rect.y + button_rect.h - 2, button_rect.w, 2};
            SDL_FillRect(surface, &border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

            // Draw button text
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, label, text_color);
            if (text_surface) {
                SDL_Rect text_rect = {
                    button_x + (button_width - text_surface->w) / 2,
                    y_offset + (button_height - text_surface->h) / 2,
                    text_surface->w, text_surface->h
                };
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
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
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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

        // Render Features section
        label_surface = TTF_RenderText_Blended(font, "Render Features:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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
        Uint32 shadows_button_bg = SDL_MapRGBA(surface->format,
            enable_shadows ? button_active_color.r : button_color.r,
            enable_shadows ? button_active_color.g : button_color.g,
            enable_shadows ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &shadows_button_rect, shadows_button_bg);

        // Draw shadows button border
        SDL_Rect shadows_border = {shadows_button_rect.x, shadows_button_rect.y, shadows_button_rect.w, 2};
        SDL_FillRect(surface, &shadows_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        shadows_border = {shadows_button_rect.x, shadows_button_rect.y + shadows_button_rect.h - 2, shadows_button_rect.w, 2};
        SDL_FillRect(surface, &shadows_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw shadows button text
        SDL_Surface* shadows_text_surface = TTF_RenderText_Blended(font, shadows_label, text_color);
        if (shadows_text_surface) {
            SDL_Rect shadows_text_rect = {
                shadows_button_rect.x + (shadows_button_width - shadows_text_surface->w) / 2,
                y_offset + (24 - shadows_text_surface->h) / 2,
                shadows_text_surface->w, shadows_text_surface->h
            };
            SDL_BlitSurface(shadows_text_surface, nullptr, surface, &shadows_text_rect);
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
        Uint32 reflections_button_bg = SDL_MapRGBA(surface->format,
            enable_reflections ? button_active_color.r : button_color.r,
            enable_reflections ? button_active_color.g : button_color.g,
            enable_reflections ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &reflections_button_rect, reflections_button_bg);

        // Draw reflections button border
        SDL_Rect reflections_border = {reflections_button_rect.x, reflections_button_rect.y, reflections_button_rect.w, 2};
        SDL_FillRect(surface, &reflections_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        reflections_border = {reflections_button_rect.x, reflections_button_rect.y + reflections_button_rect.h - 2, reflections_button_rect.w, 2};
        SDL_FillRect(surface, &reflections_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw reflections button text
        SDL_Surface* reflections_text_surface = TTF_RenderText_Blended(font, reflections_label, text_color);
        if (reflections_text_surface) {
            SDL_Rect reflections_text_rect = {
                reflections_button_rect.x + (reflections_button_width - reflections_text_surface->w) / 2,
                y_offset + (24 - reflections_text_surface->h) / 2,
                reflections_text_surface->w, reflections_text_surface->h
            };
            SDL_BlitSurface(reflections_text_surface, nullptr, surface, &reflections_text_rect);
            SDL_FreeSurface(reflections_text_surface);
        }

        y_offset += 35;

        // Screenshot button (new row)
        label_surface = TTF_RenderText_Blended(font, "Screenshot:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        const char* screenshot_label = "Save Screenshot";
        int screenshot_button_width = 120;
        SDL_Rect screenshot_button_rect = {15, y_offset, screenshot_button_width, 24};

        // Store button for click detection (category 7 = screenshot)
        buttons.push_back({{screenshot_button_rect.x + panel_x, screenshot_button_rect.y + panel_y, screenshot_button_rect.w, screenshot_button_rect.h},
                          "screenshot", 1, 7});

        // Draw screenshot button background (make it brighter like the "on" toggle state)
        Uint32 screenshot_button_bg = SDL_MapRGBA(surface->format, button_active_color.r, button_active_color.g, button_active_color.b, 255);
        SDL_FillRect(surface, &screenshot_button_rect, screenshot_button_bg);

        // Draw screenshot button border
        SDL_Rect screenshot_border = {screenshot_button_rect.x, screenshot_button_rect.y, screenshot_button_rect.w, 2};
        SDL_FillRect(surface, &screenshot_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        screenshot_border = {screenshot_button_rect.x, screenshot_button_rect.y + screenshot_button_rect.h - 2, screenshot_button_rect.w, 2};
        SDL_FillRect(surface, &screenshot_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw screenshot button text
        SDL_Surface* screenshot_text_surface = TTF_RenderText_Blended(font, screenshot_label, text_color);
        if (screenshot_text_surface) {
            SDL_Rect screenshot_text_rect = {
                screenshot_button_rect.x + (screenshot_button_width - screenshot_text_surface->w) / 2,
                y_offset + (24 - screenshot_text_surface->h) / 2,
                screenshot_text_surface->w, screenshot_text_surface->h
            };
            SDL_BlitSurface(screenshot_text_surface, nullptr, surface, &screenshot_text_rect);
            SDL_FreeSurface(screenshot_text_surface);
        }

        y_offset += 35;

        // Debug features section
        label_surface = TTF_RenderText_Blended(font, "Debug Features:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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
            Uint32 button_bg = SDL_MapRGBA(surface->format,
                is_active ? button_active_color.r : button_color.r,
                is_active ? button_active_color.g : button_color.g,
                is_active ? button_active_color.b : button_color.b,
                255);
            SDL_FillRect(surface, &button_rect, button_bg);

            // Draw button border
            SDL_Rect border = {button_rect.x, button_rect.y, button_rect.w, 2};
            SDL_FillRect(surface, &border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
            border = {button_rect.x, button_rect.y + button_rect.h - 2, button_rect.w, 2};
            SDL_FillRect(surface, &border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

            // Draw button text
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, analysis_modes[i], text_color);
            if (text_surface) {
                SDL_Rect text_rect = {
                    button_rect.x + (button_width - text_surface->w) / 2,
                    y_offset + (button_height - text_surface->h) / 2,
                    text_surface->w, text_surface->h
                };
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }
        }

        // Advanced Rendering Features section
        y_offset += 35;
        label_surface = TTF_RenderText_Blended(font, "Advanced Rendering:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
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
        Uint32 progressive_button_bg = SDL_MapRGBA(surface->format,
            enable_progressive ? button_active_color.r : button_color.r,
            enable_progressive ? button_active_color.g : button_color.g,
            enable_progressive ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &progressive_button_rect, progressive_button_bg);

        // Draw progressive button border
        SDL_Rect progressive_border = {progressive_button_rect.x, progressive_button_rect.y, progressive_button_rect.w, 2};
        SDL_FillRect(surface, &progressive_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        progressive_border = {progressive_button_rect.x, progressive_button_rect.y + progressive_button_rect.h - 2, progressive_button_rect.w, 2};
        SDL_FillRect(surface, &progressive_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw progressive button text
        SDL_Surface* progressive_text_surface = TTF_RenderText_Blended(font, progressive_label, text_color);
        if (progressive_text_surface) {
            SDL_Rect progressive_text_rect = {
                progressive_button_rect.x + (progressive_button_width - progressive_text_surface->w) / 2,
                y_offset + (24 - progressive_text_surface->h) / 2,
                progressive_text_surface->w, progressive_text_surface->h
            };
            SDL_BlitSurface(progressive_text_surface, nullptr, surface, &progressive_text_rect);
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
        Uint32 adaptive_button_bg = SDL_MapRGBA(surface->format,
            enable_adaptive ? button_active_color.r : button_color.r,
            enable_adaptive ? button_active_color.g : button_color.g,
            enable_adaptive ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &adaptive_button_rect, adaptive_button_bg);

        // Draw adaptive button border
        SDL_Rect adaptive_border = {adaptive_button_rect.x, adaptive_button_rect.y, adaptive_button_rect.w, 2};
        SDL_FillRect(surface, &adaptive_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        adaptive_border = {adaptive_button_rect.x, adaptive_button_rect.y + adaptive_button_rect.h - 2, adaptive_button_rect.w, 2};
        SDL_FillRect(surface, &adaptive_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw adaptive button text
        SDL_Surface* adaptive_text_surface = TTF_RenderText_Blended(font, adaptive_label, text_color);
        if (adaptive_text_surface) {
            SDL_Rect adaptive_text_rect = {
                adaptive_button_rect.x + (adaptive_button_width - adaptive_text_surface->w) / 2,
                y_offset + (24 - adaptive_text_surface->h) / 2,
                adaptive_text_surface->w, adaptive_text_surface->h
            };
            SDL_BlitSurface(adaptive_text_surface, nullptr, surface, &adaptive_text_rect);
            SDL_FreeSurface(adaptive_text_surface);
        }

        y_offset += 35;

        // Wavefront rendering toggle button (new row)
        const char* wavefront_label = enable_wavefront ? "Wavefront: ON" : "Wavefront: OFF";
        int wavefront_button_width = 120;
        SDL_Rect wavefront_button_rect = {15, y_offset, wavefront_button_width, 24};

        // Store button for click detection (category 10 = wavefront)
        buttons.push_back({{wavefront_button_rect.x + panel_x, wavefront_button_rect.y + panel_y, wavefront_button_rect.w, wavefront_button_rect.h},
                          "wavefront", enable_wavefront ? 1 : 0, 10});

        // Draw wavefront button background
        Uint32 wavefront_button_bg = SDL_MapRGBA(surface->format,
            enable_wavefront ? button_active_color.r : button_color.r,
            enable_wavefront ? button_active_color.g : button_color.g,
            enable_wavefront ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &wavefront_button_rect, wavefront_button_bg);

        // Draw wavefront button border
        SDL_Rect wavefront_border = {wavefront_button_rect.x, wavefront_button_rect.y, wavefront_button_rect.w, 2};
        SDL_FillRect(surface, &wavefront_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        wavefront_border = {wavefront_button_rect.x, wavefront_button_rect.y + wavefront_button_rect.h - 2, wavefront_button_rect.w, 2};
        SDL_FillRect(surface, &wavefront_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw wavefront button text
        SDL_Surface* wavefront_text_surface = TTF_RenderText_Blended(font, wavefront_label, text_color);
        if (wavefront_text_surface) {
            SDL_Rect wavefront_text_rect = {
                wavefront_button_rect.x + (wavefront_button_width - wavefront_text_surface->w) / 2,
                y_offset + (24 - wavefront_text_surface->h) / 2,
                wavefront_text_surface->w, wavefront_text_surface->h
            };
            SDL_BlitSurface(wavefront_text_surface, nullptr, surface, &wavefront_text_rect);
            SDL_FreeSurface(wavefront_text_surface);
        }

        y_offset += 35;

        // Phase 2 Optimizations section
        label_surface = TTF_RenderText_Blended(font, "Phase 2 Optimizations:", title_color);
        if (label_surface) {
            SDL_Rect label_rect = {15, y_offset, label_surface->w, label_surface->h};
            SDL_BlitSurface(label_surface, nullptr, surface, &label_rect);
            SDL_FreeSurface(label_surface);
        }
        y_offset += 22;

        // Morton Z-curve toggle button
        const char* morton_label = enable_morton ? "Morton: ON" : "Morton: OFF";
        int morton_button_width = 120;
        SDL_Rect morton_button_rect = {15, y_offset, morton_button_width, 24};

        // Store button for click detection (category 11 = morton)
        buttons.push_back({{morton_button_rect.x + panel_x, morton_button_rect.y + panel_y, morton_button_rect.w, morton_button_rect.h},
                          "morton", enable_morton ? 1 : 0, 11});

        // Draw morton button background
        Uint32 morton_button_bg = SDL_MapRGBA(surface->format,
            enable_morton ? button_active_color.r : button_color.r,
            enable_morton ? button_active_color.g : button_color.g,
            enable_morton ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &morton_button_rect, morton_button_bg);

        // Draw morton button border
        SDL_Rect morton_border = {morton_button_rect.x, morton_button_rect.y, morton_button_rect.w, 2};
        SDL_FillRect(surface, &morton_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        morton_border = {morton_button_rect.x, morton_button_rect.y + morton_button_rect.h - 2, morton_button_rect.w, 2};
        SDL_FillRect(surface, &morton_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw morton button text
        SDL_Surface* morton_text_surface = TTF_RenderText_Blended(font, morton_label, text_color);
        if (morton_text_surface) {
            SDL_Rect morton_text_rect = {
                morton_button_rect.x + (morton_button_width - morton_text_surface->w) / 2,
                y_offset + (24 - morton_text_surface->h) / 2,
                morton_text_surface->w, morton_text_surface->h
            };
            SDL_BlitSurface(morton_text_surface, nullptr, surface, &morton_text_rect);
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
        Uint32 stratified_button_bg = SDL_MapRGBA(surface->format,
            enable_stratified ? button_active_color.r : button_color.r,
            enable_stratified ? button_active_color.g : button_color.g,
            enable_stratified ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &stratified_button_rect, stratified_button_bg);

        // Draw stratified button border
        SDL_Rect stratified_border = {stratified_button_rect.x, stratified_button_rect.y, stratified_button_rect.w, 2};
        SDL_FillRect(surface, &stratified_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        stratified_border = {stratified_button_rect.x, stratified_button_rect.y + stratified_button_rect.h - 2, stratified_button_rect.w, 2};
        SDL_FillRect(surface, &stratified_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw stratified button text
        SDL_Surface* stratified_text_surface = TTF_RenderText_Blended(font, stratified_label, text_color);
        if (stratified_text_surface) {
            SDL_Rect stratified_text_rect = {
                stratified_button_rect.x + (stratified_button_width - stratified_text_surface->w) / 2,
                y_offset + (24 - stratified_text_surface->h) / 2,
                stratified_text_surface->w, stratified_text_surface->h
            };
            SDL_BlitSurface(stratified_text_surface, nullptr, surface, &stratified_text_rect);
            SDL_FreeSurface(stratified_text_surface);
        }

        y_offset += 35;

        // Frustum culling toggle button (new row)
        const char* frustum_label = enable_frustum ? "Frustum: ON" : "Frustum: OFF";
        int frustum_button_width = 120;
        SDL_Rect frustum_button_rect = {15, y_offset, frustum_button_width, 24};

        // Store button for click detection (category 13 = frustum)
        buttons.push_back({{frustum_button_rect.x + panel_x, frustum_button_rect.y + panel_y, frustum_button_rect.w, frustum_button_rect.h},
                          "frustum", enable_frustum ? 1 : 0, 13});

        // Draw frustum button background
        Uint32 frustum_button_bg = SDL_MapRGBA(surface->format,
            enable_frustum ? button_active_color.r : button_color.r,
            enable_frustum ? button_active_color.g : button_color.g,
            enable_frustum ? button_active_color.b : button_color.b,
            255);
        SDL_FillRect(surface, &frustum_button_rect, frustum_button_bg);

        // Draw frustum button border
        SDL_Rect frustum_border = {frustum_button_rect.x, frustum_button_rect.y, frustum_button_rect.w, 2};
        SDL_FillRect(surface, &frustum_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));
        frustum_border = {frustum_button_rect.x, frustum_button_rect.y + frustum_button_rect.h - 2, frustum_button_rect.w, 2};
        SDL_FillRect(surface, &frustum_border, SDL_MapRGBA(surface->format, 120, 120, 140, 255));

        // Draw frustum button text
        SDL_Surface* frustum_text_surface = TTF_RenderText_Blended(font, frustum_label, text_color);
        if (frustum_text_surface) {
            SDL_Rect frustum_text_rect = {
                frustum_button_rect.x + (frustum_button_width - frustum_text_surface->w) / 2,
                y_offset + (24 - frustum_text_surface->h) / 2,
                frustum_text_surface->w, frustum_text_surface->h
            };
            SDL_BlitSurface(frustum_text_surface, nullptr, surface, &frustum_text_rect);
            SDL_FreeSurface(frustum_text_surface);
        }

        // Convert surface to texture

        // Convert surface to texture
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_RenderCopy(renderer, texture, nullptr, &overlay_rect);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
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
        bool wavefront_changed;
        bool morton_changed;
        bool stratified_changed;
        bool frustum_changed;
        bool button_clicked;

        ClickResult() : quality_changed(false), new_quality(0),
                       samples_changed(false), new_samples(1),
                       depth_changed(false), new_depth(1),
                       resolution_changed(false), new_resolution(0),
                       shadows_changed(false), reflections_changed(false),
                       analysis_mode_changed(false), new_analysis_mode(0),
                       screenshot_requested(false), progressive_changed(false),
                       adaptive_changed(false), wavefront_changed(false),
                       morton_changed(false), stratified_changed(false), frustum_changed(false),
                       button_clicked(false) {}
    };

    ClickResult handle_click(int mouse_x, int mouse_y) {
        ClickResult result;
        for (const auto& button : buttons) {
            if (mouse_x >= button.rect.x && mouse_x < button.rect.x + button.rect.w &&
                mouse_y >= button.rect.y && mouse_y < button.rect.y + button.rect.h) {

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
                        result.screenshot_requested = true;
                        break;
                    case 8: // Progressive toggle
                        result.progressive_changed = true;
                        break;
                    case 9: // Adaptive toggle
                        result.adaptive_changed = true;
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
                }
                break;
            }
        }
        return result;
    }

    ~ControlsPanel() {
        if (font) TTF_CloseFont(font);
        if (title_font) TTF_CloseFont(title_font);
        if (initialized) TTF_Quit();
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

    // Initial quality level
    int current_quality = 0; // Start at "Large Window (Default)" quality
    QualityPreset preset = quality_levels[current_quality];

    // Rendering feature toggles
    bool enable_shadows = true;
    bool enable_reflections = true;
    bool enable_morton = false;
    bool enable_stratified = false;
    bool enable_frustum = false;

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
            preset.width,
            static_cast<int>(preset.width / (16.0f / 9.0f)),
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
    #else
        SDL_Window* window = SDL_CreateWindow(
            window_title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            preset.width,
            static_cast<int>(preset.width / (16.0f / 9.0f)),
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
    std::cout << "CPU Renderer initialized (OpenMP with " << omp_get_max_threads() << " threads)" << std::endl;

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

    // Initialize render analysis system
    RenderAnalysis analysis;
    int analysis_height = preset.width * 9 / 16;
    analysis.resize(preset.width, analysis_height);
    std::cout << "Analysis system initialized (press M to cycle modes)\n";

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
    std::cout << "  R             - Toggle CPU/GPU renderer\n";
    std::cout << "  Space         - Pause rendering\n";
    std::cout << "  S             - Save screenshot (screenshots/screenshot_*.png)\n";
    std::cout << "  ESC           - Quit\n";
    std::cout << "\n";
    std::cout << "Quality Levels (affects rendering samples, not window size):\n";
    for (int i = 0; i < NUM_QUALITY_LEVELS; i++) {
        std::cout << "  " << (i + 1) << ". " << quality_levels[i].name << "\n";
    }
    std::cout << "\n";

    while (running) {
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
                    case SDLK_s: {  // Save screenshot
                        system("mkdir -p screenshots");
                        auto now = std::chrono::system_clock::now();
                        auto time = std::chrono::system_clock::to_time_t(now);
                        std::stringstream ss;
                        ss << "screenshots/screenshot_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
                        save_cpu_screenshot(renderer, texture, image_width, image_height, ss.str().c_str());
                        break;
                    }
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
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
                            need_render = true;
                        }
                        break;
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Check if clicking on controls panel
                    if (show_controls) {
                        auto click_result = controls_panel.handle_click(event.button.x, event.button.y);
                        if (click_result.quality_changed) {
                            current_quality = click_result.new_quality;
                            preset = quality_levels[current_quality];
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
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
                            preset.max_depth = click_result.new_depth;
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
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
                            // Handle screenshot request
                            system("mkdir -p screenshots");
                            auto now = std::chrono::system_clock::now();
                            auto time = std::chrono::system_clock::to_time_t(now);
                            std::stringstream ss;
                            ss << "screenshots/screenshot_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
                            save_cpu_screenshot(renderer, texture, image_width, image_height, ss.str().c_str());
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
                        } else if (click_result.wavefront_changed) {
                            // Toggle wavefront rendering
                            ray_renderer.enable_wavefront = !ray_renderer.enable_wavefront;
                            std::cout << "Wavefront rendering: " << (ray_renderer.enable_wavefront ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.morton_changed) {
                            // Toggle Morton Z-curve ordering
                            enable_morton = !enable_morton;
                            ray_renderer.enable_morton = enable_morton;
                            std::cout << "Morton Z-curve: " << (enable_morton ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.stratified_changed) {
                            // Toggle stratified sampling
                            enable_stratified = !enable_stratified;
                            std::cout << "Stratified sampling: " << (enable_stratified ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.frustum_changed) {
                            // Toggle frustum culling
                            enable_frustum = !enable_frustum;
                            ray_renderer.enable_frustum = enable_frustum;
                            std::cout << "Frustum culling: " << (enable_frustum ? "ON" : "OFF") << std::endl;
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

                                // Recreate texture with new dimensions (window size doesn't change)
                                if (texture) {
                                    SDL_DestroyTexture(texture);
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
                                std::cout << "Resolution: " << image_width << "x" << image_height << "\n" << std::endl;
                                need_render = true;
                            }
                        } else {
                            // No button clicked, toggle mouse capture
                            SDL_bool captured = SDL_GetRelativeMouseMode();
                            SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                        }
                    } else {
                        // Controls panel not visible, toggle mouse capture
                        SDL_bool captured = SDL_GetRelativeMouseMode();
                        SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                    }
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (SDL_GetRelativeMouseMode()) {
                    camera_controller.rotate(
                        event.motion.xrel * 0.1f,
                        -event.motion.yrel * 0.1f
                    );
                    need_render = true;
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

        // Render frame if needed and not paused
        double render_time = 0.0;  // Declare outside the if block for controls panel
        if (need_render && !paused) {
            // Safety check before rendering
            if (!validate_current_settings(preset)) {
                std::cout << "Render aborted due to unsafe settings. Please adjust quality.\n";
                need_render = false;
                continue;
            }

            auto render_start = std::chrono::high_resolution_clock::now();

            // Get camera from controller
            Camera cam = camera_controller.get_camera();

#ifdef GPU_RENDERING
            // Choose rendering path based on current renderer
            if (current_renderer == RendererType::GPU && gpu_renderer) {
                // GPU rendering path
                std::cout << "GPU rendering..." << std::endl;

                // Resize GPU renderer if needed
                gpu_renderer->resize(image_width, image_height);

                // Render with GPU
                std::vector<std::vector<Color>> gpu_framebuffer;
                gpu_renderer->render(cam, gpu_framebuffer);

                // Convert GPU framebuffer to SDL texture format
                #pragma omp parallel for schedule(dynamic, 4)
                for (int j = image_height - 1; j >= 0; --j) {
                    for (int i = 0; i < image_width; ++i) {
                        Color color = gpu_framebuffer[image_height - 1 - j][i];

                        // Gamma correction and tone mapping
                        color.x = sqrt(color.x);
                        color.y = sqrt(color.y);
                        color.z = sqrt(color.z);

                        // Convert to 0-255 range
                        float r = color.x > 1.0f ? 1.0f : (color.x < 0.0f ? 0.0f : color.x);
                        float g = color.y > 1.0f ? 1.0f : (color.y < 0.0f ? 0.0f : color.y);
                        float b = color.z > 1.0f ? 1.0f : (color.z < 0.0f ? 0.0f : color.z);

                        int ir = static_cast<int>(255.999 * r);
                        int ig = static_cast<int>(255.999 * g);
                        int ib = static_cast<int>(255.999 * b);

                        // Store in framebuffer (SDL texture format: RGB)
                        size_t pixel_idx = ((image_height - 1 - j) * image_width + i) * 3;
                        framebuffer[pixel_idx + 0] = ir;
                        framebuffer[pixel_idx + 1] = ig;
                        framebuffer[pixel_idx + 2] = ib;
                    }
                }

                std::cout << "GPU render complete" << std::endl;
            } else
#endif
            {
                // CPU rendering path with advanced features
                if (ray_renderer.enable_progressive) {
                    // TRUE PROGRESSIVE RENDERING: Start noisy, get progressively better
                    static int progressive_frame_count = 0;
                    static bool progressive_initialized = false;

                    // Reset when camera moves significantly
                    static Point3 last_camera_pos(999, 999, 999);
                    Point3 current_camera_pos = camera_controller.get_position();

                    if (!progressive_initialized ||
                        fabs(last_camera_pos.x - current_camera_pos.x) > 0.1f ||
                        fabs(last_camera_pos.y - current_camera_pos.y) > 0.1f ||
                        fabs(last_camera_pos.z - current_camera_pos.z) > 0.1f) {
                        progressive_frame_count = 0;
                        progressive_initialized = true;
                        last_camera_pos = current_camera_pos;
                    }

                    // Calculate samples for this frame: 1, 2, 4, 8, 16... doubling each time
                    int current_samples = 1 << std::min(10, progressive_frame_count);
                    if (current_samples > preset.samples) current_samples = preset.samples;

                    std::cout << "Progressive frame " << (progressive_frame_count + 1)
                             << " using " << current_samples << " samples (target: " << preset.samples << ")" << std::endl;

                    #pragma omp parallel for schedule(dynamic, 4)
                    for (int j = image_height - 1; j >= 0; --j) {
                        for (int i = 0; i < image_width; ++i) {
                            Color pixel_color(0, 0, 0);

                            // Render samples for this progressive level
                            for (int s = 0; s < current_samples; ++s) {
                                float u = (i + random_float()) / (image_width - 1);
                                float v = (j + random_float()) / (image_height - 1);

                                Ray r = cam.get_ray(u, v);
                                Color sample_color = ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                                pixel_color = pixel_color + sample_color;
                            }

                            // Average samples
                            pixel_color = pixel_color / current_samples;

                            // Gamma correction
                            pixel_color.x = std::sqrt(pixel_color.x);
                            pixel_color.y = std::sqrt(pixel_color.y);
                            pixel_color.z = std::sqrt(pixel_color.z);

                            // Write to framebuffer
                            int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                            framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                            framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                            framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
                        }
                    }

                    progressive_frame_count++;

                    // Keep rendering if we haven't reached target quality yet
                    if (current_samples < preset.samples && progressive_frame_count < 10) {
                        need_render = true; // Trigger next progressive frame
                    } else {
                        std::cout << "Progressive rendering complete!" << std::endl;
                    }

                } else if (ray_renderer.enable_wavefront) {
                    // WAVEFRONT RENDERING: Tiled cache-coherent rendering
                    std::cout << "Wavefront rendering..." << std::endl;

                    // Use wavefront rendering function
                    std::vector<std::vector<Color>> wavefront_framebuffer(image_height, std::vector<Color>(image_width));
                    ray_renderer.render_wavefront(cam, scene, wavefront_framebuffer, image_width, image_height, preset.samples);

                    // Convert to SDL framebuffer format
                    #pragma omp parallel for schedule(dynamic, 4)
                    for (int j = image_height - 1; j >= 0; --j) {
                        for (int i = 0; i < image_width; ++i) {
                            Color pixel_color = wavefront_framebuffer[j][i];

                            // Write to framebuffer
                            int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                            framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(pixel_color.x, 0.0f, 0.999f));
                            framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(pixel_color.y, 0.0f, 0.999f));
                            framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(pixel_color.z, 0.0f, 0.999f));
                        }
                    }

                    std::cout << "Wavefront render complete" << std::endl;

                } else {
                    // STANDARD RENDERING: Original path with optional adaptive sampling
                    std::cout << "CPU rendering..." << std::endl;

                    #pragma omp parallel for schedule(dynamic, 4)
                for (int j = image_height - 1; j >= 0; --j) {
                for (int i = 0; i < image_width; ++i) {
                            Color pixel_color(0, 0, 0);
                            float total_depth = 0.0f;
                            Color pixel_normal(0, 0, 0);
                            Color pixel_albedo(0, 0, 0);
                            int shadow_rays = 0;
                            int total_rays = 0;

                            // Adaptive sampling: use fewer samples for preview-like quality
                            int actual_samples = ray_renderer.enable_adaptive ?
                                std::max(1, preset.samples / 2) : preset.samples; // Use half samples when adaptive is on

                            for (int s = 0; s < actual_samples; ++s) {
                                float u = (i + random_float()) / (image_width - 1);
                                float v = (j + random_float()) / (image_height - 1);

                                Ray r = cam.get_ray(u, v);
                                Color sample_color = ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                                pixel_color = pixel_color + sample_color;
                                total_rays += ray_renderer.max_depth;

                                // Get hit info for analysis (simple approximation)
                                if (!ray_renderer.enable_adaptive) {
                                    HitRecord rec;
                                    if (scene.hit(r, 0.001f, 1000.0f, rec)) {
                                        total_depth += rec.t;
                                        pixel_normal = pixel_normal + Color(rec.normal.x, rec.normal.y, rec.normal.z);
                                        if (rec.mat) {
                                            pixel_albedo = pixel_albedo + rec.mat->albedo;
                                        }

                                        // Count shadow rays for this hit point
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

                                // Record analysis data (only for standard rendering)
                                analysis.record_pixel(i, image_height - 1 - j, shadow_rays, total_rays, total_depth, pixel_normal, pixel_albedo);
                            }

                            // Apply analysis mode if active
                            Color final_color = analysis.get_analysis_color(i, image_height - 1 - j, pixel_color);

                            // Gamma correction
                            final_color.x = std::sqrt(final_color.x);
                            final_color.y = std::sqrt(final_color.y);
                            final_color.z = std::sqrt(final_color.z);

                            // Write to framebuffer
                            int pixel_index = ((image_height - 1 - j) * image_width + i) * 3;
                            framebuffer[pixel_index + 0] = static_cast<unsigned char>(256 * std::clamp(final_color.x, 0.0f, 0.999f));
                            framebuffer[pixel_index + 1] = static_cast<unsigned char>(256 * std::clamp(final_color.y, 0.0f, 0.999f));
                            framebuffer[pixel_index + 2] = static_cast<unsigned char>(256 * std::clamp(final_color.z, 0.0f, 0.999f));
                        }
                        }

                        std::cout << "CPU render complete" << std::endl;
                    }
                }  // End of CPU/GPU rendering paths

            // Update texture
            void* pixels;
            int pitch;
            SDL_LockTexture(texture, NULL, &pixels, &pitch);
            memcpy(pixels, framebuffer.data(), framebuffer.size());
            SDL_UnlockTexture(texture);

            // Calculate render time
            auto render_end = std::chrono::high_resolution_clock::now();
            double render_time = std::chrono::duration<double>(render_end - render_start).count();

            // Calculate FPS
            frame_count++;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_frame_time).count();
            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                frame_count = 0;
                last_frame_time = now;

                // Print status
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                         << " | Render: " << std::setprecision(3) << render_time << "s"
                         << " | Quality: " << preset.name
                         << " | Cam: " << camera_controller.get_position()
                         << "     " << std::flush;
            }

            need_render = false;
        }

        // Copy texture to renderer
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);

        // Render controls panel if active
        if (show_controls) {
            const char* mode_name = analysis.get_mode_name();
#ifdef GPU_RENDERING
            controls_panel.render(renderer, window_width, window_height,
                                 current_quality, preset, fps, render_time, mode_name, enable_shadows, enable_reflections,
                                 ray_renderer.enable_progressive, ray_renderer.enable_adaptive, ray_renderer.enable_wavefront,
                                 current_renderer);
#else
            controls_panel.render(renderer, window_width, window_height,
                                 current_quality, preset, fps, render_time, mode_name, enable_shadows, enable_reflections,
                                 ray_renderer.enable_progressive, ray_renderer.enable_adaptive, ray_renderer.enable_wavefront,
                                 enable_morton, enable_stratified, enable_frustum);
#endif
        }

        // Render help overlay if active
        if (show_help) {
            help_overlay.render(renderer, window_width, window_height);
        }

        SDL_RenderPresent(renderer);

        // Small delay to prevent 100% CPU usage
        SDL_Delay(1);
    }

    // Cleanup
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
