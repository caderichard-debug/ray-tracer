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

// Simple file logger
#ifdef USE_GPU_RENDERER
std::ofstream gpu_log;
#endif
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

#ifdef USE_GPU_RENDERER
#include <SDL2/SDL_opengl.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

// Simple GPU ray tracer shader code
const char* gpu_vertex_shader = R"(
#version 330 core
layout(location = 0) in vec2 a_position;
out vec2 v_uv;
void main() {
    v_uv = a_position * 0.5 + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

const char* gpu_fragment_shader = R"(
#version 330 core
in vec2 v_uv;
out vec4 frag_color;

uniform vec2 resolution;
uniform float time;

// Scene data
uniform vec3 sphere_centers[10];
uniform float sphere_radii[10];
uniform vec3 sphere_colors[10];
uniform int sphere_types[10];  // 0 = Lambertian (diffuse), 1 = Metal (reflective)
uniform float sphere_fuzz[10]; // Roughness for metal materials
uniform int sphere_count;
uniform int samples_per_pixel;
uniform int max_depth;

uniform vec3 camera_pos;
uniform vec3 camera_target;
uniform vec3 camera_up;
uniform float vfov;
uniform float aspect_ratio;

// Ray structure
struct Ray {
    vec3 origin;
    vec3 direction;
};

// Hit record
struct HitRecord {
    vec3 p;
    vec3 normal;
    float t;
    int material_id;
    bool front_face;
};

Ray get_ray(vec2 uv) {
    vec3 w = normalize(camera_target - camera_pos);
    vec3 u = normalize(cross(w, camera_up));
    vec3 v = cross(u, w);

    float half_height = tan(radians(vfov) / 2.0);
    float half_width = aspect_ratio * half_height;

    vec3 ray_origin = camera_pos;
    vec3 ray_direction = normalize(
        half_width * 2.0 * (uv.x - 0.5) * u +
        half_height * 2.0 * (uv.y - 0.5) * v + w
    );

    Ray r;
    r.origin = ray_origin;
    r.direction = ray_direction;
    return r;
}

bool hit_sphere(vec3 center, float radius, Ray r, float t_min, float t_max, inout HitRecord rec) {
    vec3 oc = r.origin - center;
    float a = dot(r.direction, r.direction);
    float half_b = dot(oc, r.direction);
    float c = dot(oc, oc) - radius * radius;

    float discriminant = half_b * half_b - a * c;
    if (discriminant < 0.0) {
        return false;
    }

    float sqrt_d = sqrt(discriminant);
    float root = (-half_b - sqrt_d) / a;

    if (root < t_min || root > t_max) {
        root = (-half_b + sqrt_d) / a;
        if (root < t_min || root > t_max) {
            return false;
        }
    }

    rec.t = root;
    rec.p = r.origin + root * r.direction;
    vec3 outward_normal = (rec.p - center) / radius;
    rec.front_face = dot(r.direction, outward_normal) < 0.0;
    rec.normal = rec.front_face ? outward_normal : -outward_normal;
    rec.material_id = 0;

    return true;
}

bool scene_hit(Ray r, float t_min, float t_max, inout HitRecord rec) {
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_so_far = t_max;

    for (int i = 0; i < sphere_count; i++) {
        if (hit_sphere(sphere_centers[i], sphere_radii[i], r, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
            rec.material_id = i;
        }
    }

    return hit_anything;
}

// Random number generation (must be declared before ray_color)
float rand_seed = 0.0;

float random_float() {
    rand_seed = fract(sin(dot(vec2(rand_seed, rand_seed * 0.1), vec2(12.9898, 78.233))) * 43758.5453);
    return rand_seed;
}

vec3 random_in_unit_sphere() {
    vec3 p;
    do {
        p = 2.0 * vec3(random_float(), random_float(), random_float()) - 1.0;
    } while (dot(p, p) >= 1.0);
    return p;
}

vec3 ray_color(Ray r, int depth) {
    HitRecord rec;

    // Background gradient
    if (!scene_hit(r, 0.001, 1000.0, rec)) {
        vec3 unit_direction = normalize(r.direction);
        float t = 0.5 * (unit_direction.y + 1.0);
        return mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
    }

    vec3 albedo = sphere_colors[rec.material_id];
    int mat_type = sphere_types[rec.material_id];
    float fuzz = sphere_fuzz[rec.material_id];

    vec3 color;

    if (mat_type == 1) {
        // Metal: Strong diffuse component for visible fuzz
        vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
        vec3 ambient = vec3(0.15, 0.15, 0.15);
        float diff = max(dot(rec.normal, light_dir), 0.0) * 0.8;  // Strong diffuse
        vec3 view_dir = normalize(-r.direction);
        vec3 reflect_dir = reflect(-light_dir, rec.normal);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);  // Softer highlight

        // Strong base metal color with diffuse (makes fuzz very visible)
        color = ambient * albedo + diff * albedo + spec * vec3(0.3, 0.3, 0.3);

        // Add reflection for metal materials
        if (depth > 1) {
            vec3 reflected_dir = reflect(normalize(r.direction), rec.normal);

            // Add strong fuzz for rough metals
            if (fuzz > 0.0) {
                // Very strong perturbation for visible fuzz effect
                vec3 fuzz_offset = fuzz * 1.2 * normalize(vec3(
                    sin(rec.p.x * 6.0),
                    sin(rec.p.y * 6.0),
                    sin(rec.p.z * 6.0)
                ));
                reflected_dir = normalize(reflected_dir + fuzz_offset);
            }

            Ray reflected_ray;
            reflected_ray.origin = rec.p + 0.1 * rec.normal;
            reflected_ray.direction = normalize(reflected_dir);

            HitRecord ref_rec;
            vec3 ref_color;

            if (scene_hit(reflected_ray, 0.001, 1000.0, ref_rec)) {
                // Hit an object - get its color
                if (ref_rec.material_id != rec.material_id) {
                    vec3 ref_albedo = sphere_colors[ref_rec.material_id];
                    int ref_mat_type = sphere_types[ref_rec.material_id];

                    // Simple shading for reflected object
                    vec3 ref_light_dir = normalize(vec3(1.0, 1.0, 1.0));
                    vec3 ref_ambient = vec3(0.1, 0.1, 0.1);
                    float ref_diff = max(dot(ref_rec.normal, ref_light_dir), 0.0);

                    ref_color = ref_ambient * ref_albedo + ref_diff * ref_albedo;
                } else {
                    // Self-reflection - use background
                    vec3 unit_direction = normalize(reflected_dir);
                    float t = 0.5 * (unit_direction.y + 1.0);
                    ref_color = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
                }
            } else {
                // Didn't hit anything - use background gradient
                vec3 unit_direction = normalize(reflected_dir);
                float t = 0.5 * (unit_direction.y + 1.0);
                ref_color = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
            }

            // Less reflective, more base color (40% reflection, 60% diffuse)
            color = mix(ref_color, color, 0.6);
        }
    } else {
        // Lambertian (diffuse): Full diffuse shading, no reflections
        vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
        vec3 light_color = vec3(1.0, 1.0, 1.0);
        vec3 ambient = vec3(0.1, 0.1, 0.1);
        float diff = max(dot(rec.normal, light_dir), 0.0);

        color = ambient * albedo + diff * light_color * albedo;
    }

    return color;
}

void main() {
    // Initialize random seed based on pixel position
    rand_seed = fract(sin(dot(v_uv, vec2(12.9898, 78.233))) * 43758.5453);

    vec3 color = vec3(0.0);

    // Multi-sample anti-aliasing
    for (int s = 0; s < samples_per_pixel; s++) {
        // Add random jitter for anti-aliasing
        vec2 jitter = vec2(random_float() - 0.5, random_float() - 0.5) / resolution;

        // Generate ray for this sample
        Ray r = get_ray(v_uv + jitter);

        // Ray trace with configured max depth
        color += ray_color(r, max_depth);
    }

    // Average samples
    color = color / float(samples_per_pixel);

    // Gamma correction
    color = sqrt(color);

    frag_color = vec4(color, 1.0);
}
)";
#endif

// Quality presets (resolution, samples, max_depth)
struct QualityPreset {
    int width;
    int samples;
    int max_depth;
    const char* name;
};

QualityPreset quality_levels[] = {
    {1280, 1, 3, "Preview (Ultra Fast)"},  // 4x bigger window
    {640, 1, 3, "Low (Fast)"},
    {800, 4, 3, "Medium"},
    {1280, 16, 5, "High"},
    {1600, 32, 5, "Ultra"},
    {1920, 64, 5, "Maximum Quality"}
};

const int NUM_QUALITY_LEVELS = 6;

// Safety limits
const int MAX_WIDTH = 1920;
const int MAX_SAMPLES = 64;
const int MAX_DEPTH = 5;

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
        std::cout << "\n⚠️  DANGER: Current settings are UNSAFE!\n";
        std::cout << "  Resolution: " << preset.width << "x" << (preset.width * 9 / 16) << "\n";
        std::cout << "  Samples: " << preset.samples << " (must be < 8)\n";
        std::cout << "  This combination may crash the application.\n";
        std::cout << "  Please reduce samples or resolution.\n\n";
        return false;
    }
    return true;
}

#ifdef USE_GPU_RENDERER
// Save GPU framebuffer to PNG file
void save_gpu_framebuffer(int width, int height, const char* filename) {
    // Allocate buffer for pixel data
    std::vector<unsigned char> pixels(width * height * 3);

    // Read pixels from framebuffer (bottom-left origin)
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Flip vertically (OpenGL has bottom-left origin, PNG has top-left)
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
    std::cout << "Saved framebuffer to " << filename << std::endl;
}
#endif

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
            (window_width - 400) / 2,
            (window_height - 320) / 2,
            400,
            320
        };

        SDL_Surface* surface = SDL_CreateRGBSurface(0, overlay_rect.w, overlay_rect.h, 32, 0, 0, 0, 0);
        if (!surface) return;

        // Fill background
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 200, 200, 200, 180));

        // Render title
        const char* title_text = "CONTROLS";
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {(400 - title_surface->w) / 2, 12, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        // Render controls text (condensed)
        const char* controls_text[] = {
            "MOVE: WASD + Arrows | LOOK: Mouse",
            "1-6: Quality | M: Analysis | SPACE: Pause",
            "C: Controls | H: Help | ESC: Quit"
        };

        int y_offset = 50;
        for (size_t i = 0; i < sizeof(controls_text) / sizeof(controls_text[0]); ++i) {
            SDL_Surface* text_surface = TTF_RenderText_Blended(font, controls_text[i], text_color);

            if (text_surface) {
                SDL_Rect text_rect = {(400 - text_surface->w) / 2, y_offset, text_surface->w, text_surface->h};
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }
            y_offset += 40;  // More compact spacing
        }

        // Add footer
        const char* footer = "Press H to close";
        SDL_Surface* footer_surface = TTF_RenderText_Blended(font, footer, text_color);
        if (footer_surface) {
            SDL_Rect footer_rect = {(400 - footer_surface->w) / 2, 280, footer_surface->w, footer_surface->h};
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

public:
    ControlsPanel() : font(nullptr), title_font(nullptr), initialized(false), panel_x(0), panel_y(0) {
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
                const char* analysis_mode_name = nullptr, bool enable_shadows = true, bool enable_reflections = true) {
        (void)window_height;  // Only used to calculate aspect ratio
        if (!initialized || !font || !title_font) return;

        // Panel positioned in top-right corner, scales with window size
        int panel_width = std::min(360, window_width - 20);
        int panel_height = std::min(455, window_height - 20);
        panel_x = window_width - panel_width - 10;
        panel_y = 10;
        SDL_Rect overlay_rect = {panel_x, panel_y, panel_width, panel_height};

        SDL_Surface* surface = SDL_CreateRGBSurface(0, overlay_rect.w, overlay_rect.h, 32, 0, 0, 0, 0);
        if (!surface) return;

        // Fill background with rounded appearance
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 50, 50, 60, 230));

        // Render title
        const char* title_text = "INTERACTIVE CONTROLS";
        SDL_Surface* title_surface = TTF_RenderText_Blended(title_font, title_text, title_color);
        if (title_surface) {
            SDL_Rect title_rect = {15, 10, title_surface->w, title_surface->h};
            SDL_BlitSurface(title_surface, nullptr, surface, &title_rect);
            SDL_FreeSurface(title_surface);
        }

        // Draw separator line
        SDL_Rect separator = {10, 35, panel_width - 20, 1};
        SDL_FillRect(surface, &separator, SDL_MapRGBA(surface->format, 100, 100, 120, 255));

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

        // Extended button renderer with custom text and colors
        auto render_button_ext = [&](const char* prefix, const char* label, int category, bool is_active, SDL_Color custom_color) {
            int button_width = 60;
            int button_height = 24;
            int button_spacing = 8;

            static int button_x_ext = 15;
            static int buttons_in_row_ext = 0;

            if (buttons.empty() || buttons.back().category != category) {
                button_x_ext = 15;
                buttons_in_row_ext = 0;
            }

            // Split into label text and button
            SDL_Surface* prefix_surface = TTF_RenderText_Blended(font, prefix, text_color);
            if (prefix_surface) {
                SDL_Rect prefix_rect = {button_x_ext, y_offset, prefix_surface->w, prefix_surface->h};
                SDL_BlitSurface(prefix_surface, nullptr, surface, &prefix_rect);
                SDL_FreeSurface(prefix_surface);
                button_x_ext += prefix_surface->w + 5;
            }

            SDL_Rect button_rect = {button_x_ext, y_offset, button_width, button_height};

            // Store button for click detection
            buttons.push_back({{button_rect.x + panel_x, button_rect.y + panel_y, button_rect.w, button_rect.h},
                              label, is_active ? 1 : 0, category});

            // Draw button background with custom color
            Uint32 button_bg = SDL_MapRGBA(surface->format, custom_color.r, custom_color.g, custom_color.b, custom_color.a);
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
                    button_x_ext + (button_width - text_surface->w) / 2,
                    y_offset + (button_height - text_surface->h) / 2,
                    text_surface->w, text_surface->h
                };
                SDL_BlitSurface(text_surface, nullptr, surface, &text_rect);
                SDL_FreeSurface(text_surface);
            }

            button_x_ext += button_width + button_spacing;
            buttons_in_row_ext++;

            if (buttons_in_row_ext >= 2) {
                button_x_ext = 15;
                buttons_in_row_ext = 0;
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
        const char* resolution_desc[] = {
            "Fast",
            "Good",
            "High",
            "Ultra",
            "Maximum"
        };

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
        int button_width = 85;
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
        bool button_clicked;

        ClickResult() : quality_changed(false), new_quality(0),
                       samples_changed(false), new_samples(1),
                       depth_changed(false), new_depth(1),
                       resolution_changed(false), new_resolution(0),
                       shadows_changed(false), reflections_changed(false),
                       analysis_mode_changed(false), new_analysis_mode(0),
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
    int current_quality = 1; // Start at "Low (Fast)" quality
    QualityPreset preset = quality_levels[current_quality];

    // Rendering feature toggles
    bool enable_shadows = true;
    bool enable_reflections = true;

    // Setup OpenGL for GPU rendering
#ifdef USE_GPU_RENDERER
    // Use OpenGL 3.3 for maximum compatibility
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif

    // Create window
    const char* window_title =
#ifdef USE_GPU_RENDERER
        "Real-time Ray Tracer - GPU (OpenGL Compute Shaders)";
#else
        "Real-time Ray Tracer - CPU (OpenMP)";
#endif

    SDL_Window* window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        preset.width,
        static_cast<int>(preset.width / (16.0f / 9.0f)),
#ifdef USE_GPU_RENDERER
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED
#else
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
#endif
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

#ifdef USE_GPU_RENDERER
    // Show the window explicitly
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "OpenGL 3.3 context created successfully" << std::endl;
    std::cout << "GPU mode: Showing animated test pattern" << std::endl;

#ifdef USE_GPU_RENDERER
    gpu_log.open("gpu_rendering.log");
    gpu_log << "GPU Ray Tracer Logging Started" << std::endl;
    gpu_log << "OpenGL 3.3 context created" << std::endl;
#endif
#endif

    // Create renderer and texture (CPU mode only)
    int image_width = preset.width;
    int image_height = static_cast<int>(preset.width / (16.0f / 9.0f));

#ifndef USE_GPU_RENDERER
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
#else
    // GPU mode uses OpenGL rendering directly
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    (void)renderer; // Unused in GPU mode
    (void)texture;  // Unused in GPU mode
#endif

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
#ifdef USE_GPU_RENDERER
    std::cout << "GPU Renderer initialized (OpenGL 3.3)" << std::endl;
#else
    Renderer ray_renderer(preset.max_depth);
    ray_renderer.enable_shadows = enable_shadows;
    ray_renderer.enable_reflections = enable_reflections;
    std::cout << "CPU Renderer initialized (OpenMP with " << omp_get_max_threads() << " threads)" << std::endl;
#endif

    // Main loop
    bool running = true;
    bool paused = false;
    bool need_render = true;
    bool show_help = false;

#ifdef USE_GPU_RENDERER
    std::cout << "Starting main loop with GPU rendering..." << std::endl;
    std::cout << "Window should be visible now" << std::endl;
#endif

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

#ifdef USE_GPU_RENDERER
    // GPU scene data (will be populated during initialization)
    int gpu_sphere_count = 0;
    Vec3 gpu_sphere_centers[10];
    float gpu_sphere_radii[10];
    Vec3 gpu_sphere_colors[10];
    int gpu_sphere_types[10];  // 0 = Lambertian, 1 = Metal
    float gpu_sphere_fuzz[10]; // Roughness for metals

    // Force window to front and make sure it's visible
    SDL_RaiseWindow(window);
    SDL_SetWindowInputFocus(window);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
#endif

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
    std::cout << "  H             - Toggle help overlay\n";
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
#ifdef USE_GPU_RENDERER
        // Pump events even if we don't process them
        SDL_PumpEvents();
#endif

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
#ifdef USE_GPU_RENDERER
                        save_gpu_framebuffer(image_width, image_height, ss.str().c_str());
                        std::cout << "Screenshot saved: " << ss.str() << std::endl;
#else
                        std::cout << "Screenshot only available in GPU mode" << std::endl;
#endif
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

                                std::cout << "\n⚠️  WARNING: High quality setting!\n";
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

#ifdef USE_GPU_RENDERER
                            // GPU renderer doesn't need re-initialization for quality changes
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
#else
                            // Update CPU renderer (keep window size, only change quality)
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
#endif
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
#ifdef USE_GPU_RENDERER
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
#else
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
#endif
                            need_render = true;
                        } else if (click_result.samples_changed) {
                            // Safety check for samples
                            int new_samples = click_result.new_samples;
                            if (!is_samples_safe(new_samples, preset.width)) {
                                int height = preset.width * 9 / 16;
                                int pixels = preset.width * height;
                                int rays = pixels * new_samples;
                                std::cout << "\n⚠️  UNSAFE SAMPLES SETTING!\n";
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
#ifdef USE_GPU_RENDERER
                            std::cout << "Depth: " << preset.max_depth << std::endl;
#else
                            ray_renderer = Renderer(preset.max_depth);
                            ray_renderer.enable_shadows = enable_shadows;
                            ray_renderer.enable_reflections = enable_reflections;
                            std::cout << "Depth: " << preset.max_depth << std::endl;
#endif
                            need_render = true;
                        } else if (click_result.shadows_changed) {
                            enable_shadows = !enable_shadows;
#ifndef USE_GPU_RENDERER
                            ray_renderer.enable_shadows = enable_shadows;
#endif
                            std::cout << "Shadows: " << (enable_shadows ? "ON" : "OFF") << std::endl;
                            need_render = true;
                        } else if (click_result.reflections_changed) {
                            enable_reflections = !enable_reflections;
#ifndef USE_GPU_RENDERER
                            ray_renderer.enable_reflections = enable_reflections;
#endif
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
                        } else if (click_result.resolution_changed) {
                            int new_width = click_result.new_resolution;
                            if (!is_samples_safe(preset.samples, new_width)) {
                                std::cout << "⚠️  Unsafe resolution for current samples. Please reduce samples first.\n";
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

#ifndef USE_GPU_RENDERER
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
#else
                                // GPU mode uses OpenGL rendering directly
                                (void)renderer; // Unused in GPU mode
                                (void)texture;  // Unused in GPU mode
#endif
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
#ifdef USE_GPU_RENDERER
                    // GPU: Inverted vertical look
                    camera_controller.rotate(
                        event.motion.xrel * 0.1f,
                        -event.motion.yrel * 0.1f
                    );
#else
                    // CPU: Standard vertical look
                    camera_controller.rotate(
                        event.motion.xrel * 0.1f,
                        -event.motion.yrel * 0.1f
                    );
#endif
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
#ifdef USE_GPU_RENDERER
            camera_controller.move_up(-move_speed * 0.5f);  // Inverted and slower for GPU
#else
            camera_controller.move_up(move_speed);
#endif
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_DOWN]) {
#ifdef USE_GPU_RENDERER
            camera_controller.move_up(move_speed * 0.5f);  // Inverted and slower for GPU
#else
            camera_controller.move_up(-move_speed);
#endif
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

            // Get camera from controller (needed by both GPU and CPU paths)
            Camera cam = camera_controller.get_camera();

#ifdef USE_GPU_RENDERER
            // Simple GPU rendering - just show test pattern for now
            static bool gpu_initialized = false;
            static GLuint gpu_program = 0;
            static GLuint gpu_vao = 0, gpu_vbo = 0;

            if (!gpu_initialized) {
                // Extract scene data for GPU
                std::vector<Vec3> sphere_centers_vec;
                std::vector<float> sphere_radii_vec;
                std::vector<Vec3> sphere_colors_vec;
                std::vector<int> sphere_types_vec;  // 0 = Lambertian, 1 = Metal
                std::vector<float> sphere_fuzz_vec; // Roughness for metals

                for (const auto& obj : scene.objects) {
                    // Try to cast to sphere
                    auto sphere = std::dynamic_pointer_cast<Sphere>(obj);
                    if (sphere) {
                        sphere_centers_vec.push_back(sphere->center);
                        sphere_radii_vec.push_back(sphere->radius);
                        sphere_colors_vec.push_back(sphere->mat->albedo);

                        // Extract material type
                        auto lambertian = std::dynamic_pointer_cast<Lambertian>(sphere->mat);
                        auto metal = std::dynamic_pointer_cast<Metal>(sphere->mat);

                        if (metal) {
                            sphere_types_vec.push_back(1);  // Metal
                            sphere_fuzz_vec.push_back(metal->fuzz);
                        } else {
                            sphere_types_vec.push_back(0);  // Lambertian (default)
                            sphere_fuzz_vec.push_back(0.0f);
                        }
                    }
                }

                gpu_sphere_count = sphere_centers_vec.size();
                std::cout << "Uploaded " << gpu_sphere_count << " spheres to GPU" << std::endl;

                // Copy vector data to arrays (THIS WAS MISSING!)
                for (size_t i = 0; i < sphere_centers_vec.size() && i < 10; i++) {
                    gpu_sphere_centers[i] = sphere_centers_vec[i];
                    gpu_sphere_radii[i] = sphere_radii_vec[i];
                    gpu_sphere_colors[i] = sphere_colors_vec[i];
                    gpu_sphere_types[i] = sphere_types_vec[i];
                    gpu_sphere_fuzz[i] = sphere_fuzz_vec[i];
                }

                // Load OpenGL functions using SDL
                auto glCreateShader = (GLuint (*)(GLenum))SDL_GL_GetProcAddress("glCreateShader");
                auto glShaderSource = (void (*)(GLuint, GLsizei, const GLchar *const *, const GLint *))SDL_GL_GetProcAddress("glShaderSource");
                auto glCompileShader = (void (*)(GLuint))SDL_GL_GetProcAddress("glCompileShader");
                auto glCreateProgram = (GLuint (*)())SDL_GL_GetProcAddress("glCreateProgram");
                auto glAttachShader = (void (*)(GLuint, GLuint))SDL_GL_GetProcAddress("glAttachShader");
                auto glLinkProgram = (void (*)(GLuint))SDL_GL_GetProcAddress("glLinkProgram");
                auto glDeleteShader = (void (*)(GLuint))SDL_GL_GetProcAddress("glDeleteShader");
                auto glUseProgram = (void (*)(GLuint))SDL_GL_GetProcAddress("glUseProgram");
                auto glDrawArrays = (void (*)(GLenum, GLint, GLsizei))SDL_GL_GetProcAddress("glDrawArrays");
                auto glViewport = (void (*)(GLint, GLint, GLsizei, GLsizei))SDL_GL_GetProcAddress("glViewport");
                auto glClearColor = (void (*)(GLfloat, GLfloat, GLfloat, GLfloat))SDL_GL_GetProcAddress("glClearColor");
                auto glClear = (void (*)(GLbitfield))SDL_GL_GetProcAddress("glClear");
                auto glGenVertexArrays = (void (*)(GLsizei, GLuint *))SDL_GL_GetProcAddress("glGenVertexArrays");
                auto glBindVertexArray = (void (*)(GLuint))SDL_GL_GetProcAddress("glBindVertexArray");
                auto glGenBuffers = (void (*)(GLsizei, GLuint *))SDL_GL_GetProcAddress("glGenBuffers");
                auto glBindBuffer = (void (*)(GLenum, GLuint))SDL_GL_GetProcAddress("glBindBuffer");
                auto glBufferData = (void (*)(GLenum, GLsizei, const void *, GLenum))SDL_GL_GetProcAddress("glBufferData");
                auto glVertexAttribPointer = (void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))SDL_GL_GetProcAddress("glVertexAttribPointer");
                auto glEnableVertexAttribArray = (void (*)(GLuint))SDL_GL_GetProcAddress("glEnableVertexAttribArray");
                auto glGetShaderiv = (void (*)(GLuint, GLenum, GLint *))SDL_GL_GetProcAddress("glGetShaderiv");
                auto glGetShaderInfoLog = (void (*)(GLuint, GLsizei, GLsizei *, char *))SDL_GL_GetProcAddress("glGetShaderInfoLog");
                auto glGetProgramiv = (void (*)(GLuint, GLenum, GLint *))SDL_GL_GetProcAddress("glGetProgramiv");
                auto glGetProgramInfoLog = (void (*)(GLuint, GLsizei, GLsizei *, char *))SDL_GL_GetProcAddress("glGetProgramInfoLog");

                if (!glCreateShader || !glCreateProgram || !glUseProgram) {
                    std::cerr << "Failed to load OpenGL functions" << std::endl;
                    continue;
                }

                // Compile vertex shader
                GLuint vs = glCreateShader(GL_VERTEX_SHADER);
                glShaderSource(vs, 1, &gpu_vertex_shader, nullptr);
                glCompileShader(vs);

                // Check vertex shader compilation
                GLint vs_success;
                glGetShaderiv(vs, GL_COMPILE_STATUS, &vs_success);
                if (!vs_success) {
                    char info_log[512];
                    glGetShaderInfoLog(vs, 512, nullptr, info_log);
                    std::cerr << "Vertex shader compilation failed: " << info_log << std::endl;
                    continue;
                }

                // Compile fragment shader
                GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
                glShaderSource(fs, 1, &gpu_fragment_shader, nullptr);
                glCompileShader(fs);

                // Check fragment shader compilation
                GLint fs_success;
                glGetShaderiv(fs, GL_COMPILE_STATUS, &fs_success);
                if (!fs_success) {
                    char info_log[512];
                    glGetShaderInfoLog(fs, 512, nullptr, info_log);
                    std::cerr << "Fragment shader compilation failed: " << info_log << std::endl;
                    continue;
                }

                gpu_program = glCreateProgram();
                glAttachShader(gpu_program, vs);
                glAttachShader(gpu_program, fs);
                glLinkProgram(gpu_program);

#ifdef USE_GPU_RENDERER
                gpu_log << "Shader program created, id=" << gpu_program << std::endl;
#endif

                // Check link status
                GLint link_success;
                glGetProgramiv(gpu_program, GL_LINK_STATUS, &link_success);
                if (!link_success) {
                    char info_log[512];
                    glGetProgramInfoLog(gpu_program, 512, nullptr, info_log);
                    std::cerr << "Shader program linking failed: " << info_log << std::endl;
#ifdef USE_GPU_RENDERER
                    gpu_log << "Shader program linking failed: " << info_log << std::endl;
#endif
                    continue;
                }

#ifdef USE_GPU_RENDERER
                gpu_log << "Shader program linked successfully" << std::endl;
#endif

                glDeleteShader(vs);
                glDeleteShader(fs);

                // Create quad
                float quad[] = {-1, -1, 3, -1, -1, 3};
                glGenVertexArrays(1, &gpu_vao);
                glGenBuffers(1, &gpu_vbo);
                glBindVertexArray(gpu_vao);
                glBindBuffer(GL_ARRAY_BUFFER, gpu_vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                glEnableVertexAttribArray(0);
                glBindVertexArray(0);

                gpu_initialized = true;
                std::cout << "GPU renderer initialized" << std::endl;
            }

            // Load functions again for rendering
            auto glUseProgram = (void (*)(GLuint))SDL_GL_GetProcAddress("glUseProgram");
            auto glGetUniformLocation = (GLint (*)(GLuint, const GLchar *))SDL_GL_GetProcAddress("glGetUniformLocation");
            auto glUniform2f = (void (*)(GLint, GLfloat, GLfloat))SDL_GL_GetProcAddress("glUniform2f");
            auto glUniform1f = (void (*)(GLint, GLfloat))SDL_GL_GetProcAddress("glUniform1f");
            auto glUniform1i = (void (*)(GLint, GLint))SDL_GL_GetProcAddress("glUniform1i");
            auto glUniform3f = (void (*)(GLint, GLfloat, GLfloat, GLfloat))SDL_GL_GetProcAddress("glUniform3f");
            auto glUniform3fv = (void (*)(GLint, GLsizei, const GLfloat *))SDL_GL_GetProcAddress("glUniform3fv");
            auto glUniform1fv = (void (*)(GLint, GLsizei, const GLfloat *))SDL_GL_GetProcAddress("glUniform1fv");
            auto glDrawArrays = (void (*)(GLenum, GLint, GLsizei))SDL_GL_GetProcAddress("glDrawArrays");
            auto glViewport = (void (*)(GLint, GLint, GLsizei, GLsizei))SDL_GL_GetProcAddress("glViewport");
            auto glClearColor = (void (*)(GLfloat, GLfloat, GLfloat, GLfloat))SDL_GL_GetProcAddress("glClearColor");
            auto glClear = (void (*)(GLbitfield))SDL_GL_GetProcAddress("glClear");
            auto glBindVertexArray = (void (*)(GLuint))SDL_GL_GetProcAddress("glBindVertexArray");

            // Render
            glViewport(0, 0, image_width, image_height);
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(gpu_program);

#ifdef USE_GPU_RENDERER
            gpu_log << "Setting camera uniforms" << std::endl;
#endif

            // Set camera uniforms
            GLint cam_pos_loc = glGetUniformLocation(gpu_program, "camera_pos");
            GLint cam_target_loc = glGetUniformLocation(gpu_program, "camera_target");
            GLint cam_up_loc = glGetUniformLocation(gpu_program, "camera_up");
            GLint vfov_loc = glGetUniformLocation(gpu_program, "vfov");
            GLint aspect_loc = glGetUniformLocation(gpu_program, "aspect_ratio");

#ifdef USE_GPU_RENDERER
            gpu_log << "  camera_pos loc: " << cam_pos_loc << std::endl;
            gpu_log << "  camera_target loc: " << cam_target_loc << std::endl;
            gpu_log << "  camera_up loc: " << cam_up_loc << std::endl;
            gpu_log << "  vfov loc: " << vfov_loc << std::endl;
            gpu_log << "  aspect_ratio loc: " << aspect_loc << std::endl;
#endif

            glUniform3f(cam_pos_loc, cam.lookfrom.x, cam.lookfrom.y, cam.lookfrom.z);
            glUniform3f(cam_target_loc, cam.lookat.x, cam.lookat.y, cam.lookat.z);
            glUniform3f(cam_up_loc, cam.vup.x, cam.vup.y, cam.vup.z);
            glUniform1f(vfov_loc, cam.vfov);
            glUniform1f(aspect_loc, cam.aspect_ratio);
            glUniform2f(glGetUniformLocation(gpu_program, "resolution"), (float)image_width, (float)image_height);

            // Set sphere uniforms
            glUniform1i(glGetUniformLocation(gpu_program, "sphere_count"), gpu_sphere_count);
            glUniform1i(glGetUniformLocation(gpu_program, "samples_per_pixel"), preset.samples);
            glUniform1i(glGetUniformLocation(gpu_program, "max_depth"), preset.max_depth);

#ifdef USE_GPU_RENDERER
            gpu_log << "Quality: " << preset.name << " (samples=" << preset.samples
                     << ", depth=" << preset.max_depth << ")" << std::endl;
            gpu_log << "Setting " << gpu_sphere_count << " spheres" << std::endl;
            gpu_log << "Camera: " << cam.lookfrom.x << ", " << cam.lookfrom.y << ", " << cam.lookfrom.z << std::endl;

            // Upload sphere data (this is inefficient but works for demo)
            for (int i = 0; i < gpu_sphere_count; i++) {
                std::string loc = "sphere_centers[" + std::to_string(i) + "]";
                GLint loc_id = glGetUniformLocation(gpu_program, loc.c_str());
                gpu_log << "Sphere " << i << " center: (" << gpu_sphere_centers[i].x << ", " << gpu_sphere_centers[i].y << ", " << gpu_sphere_centers[i].z << ") radius: " << gpu_sphere_radii[i] << std::endl;
                if (loc_id >= 0) {
                    glUniform3f(loc_id,
                               gpu_sphere_centers[i].x, gpu_sphere_centers[i].y, gpu_sphere_centers[i].z);
                } else {
                    gpu_log << "  Warning: uniform " << loc << " not found (id=" << loc_id << ")" << std::endl;
                }

                loc = "sphere_radii[" + std::to_string(i) + "]";
                loc_id = glGetUniformLocation(gpu_program, loc.c_str());
                if (loc_id >= 0) {
                    glUniform1f(loc_id, gpu_sphere_radii[i]);
                }

                loc = "sphere_colors[" + std::to_string(i) + "]";
                loc_id = glGetUniformLocation(gpu_program, loc.c_str());
                if (loc_id >= 0) {
                    glUniform3f(loc_id,
                               gpu_sphere_colors[i].x, gpu_sphere_colors[i].y, gpu_sphere_colors[i].z);
                }

                // Set material type and fuzz
                loc = "sphere_types[" + std::to_string(i) + "]";
                loc_id = glGetUniformLocation(gpu_program, loc.c_str());
                if (loc_id >= 0) {
                    glUniform1i(loc_id, gpu_sphere_types[i]);
                }

                loc = "sphere_fuzz[" + std::to_string(i) + "]";
                loc_id = glGetUniformLocation(gpu_program, loc.c_str());
                if (loc_id >= 0) {
                    glUniform1f(loc_id, gpu_sphere_fuzz[i]);
                }
            }

            gpu_log << "Drawing..." << std::endl;
            gpu_log.flush();
#endif

            glBindVertexArray(gpu_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            SDL_GL_SwapWindow(window);

            // Calculate render time
            auto render_end_gpu = std::chrono::high_resolution_clock::now();
            double render_time_gpu = std::chrono::duration<double>(render_end_gpu - render_start).count();

            frame_count++;
            auto now_gpu = std::chrono::high_resolution_clock::now();
            double elapsed_gpu = std::chrono::duration<double>(now_gpu - last_frame_time).count();
            if (elapsed_gpu >= 1.0) {
                fps = frame_count / elapsed_gpu;
                frame_count = 0;
                last_frame_time = now_gpu;

                // Print status
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                         << " | Render: " << std::setprecision(3) << render_time_gpu << "s"
                         << " | Quality: " << preset.name
                         << " | GPU TEST MODE"
                         << "     " << std::flush;
            }

            need_render = false;
            continue;
#else
            // CPU rendering path
            #pragma omp parallel for schedule(dynamic, 4)
            for (int j = image_height - 1; j >= 0; --j) {
                for (int i = 0; i < image_width; ++i) {
                    Color pixel_color(0, 0, 0);
                    float total_depth = 0.0f;
                    Color pixel_normal(0, 0, 0);
                    Color pixel_albedo(0, 0, 0);
                    int shadow_rays = 0;
                    int total_rays = 0;

                    for (int s = 0; s < preset.samples; ++s) {
                        float u = (i + random_float()) / (image_width - 1);
                        float v = (j + random_float()) / (image_height - 1);

                        Ray r = cam.get_ray(u, v);
                        Color sample_color = ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                        pixel_color = pixel_color + sample_color;
                        total_rays += ray_renderer.max_depth;

                        // Get hit info for analysis (simple approximation)
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

                    float scale = 1.0f / preset.samples;
                    pixel_color = pixel_color * scale;
                    pixel_normal = pixel_normal * scale;
                    pixel_albedo = pixel_albedo * scale;
                    total_depth *= scale;

                    // Record analysis data
                    analysis.record_pixel(i, image_height - 1 - j, shadow_rays, total_rays, total_depth, pixel_normal, pixel_albedo);

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

            // Update texture
            void* pixels;
            int pitch;
            SDL_LockTexture(texture, NULL, &pixels, &pitch);
            memcpy(pixels, framebuffer.data(), framebuffer.size());
            SDL_UnlockTexture(texture);
#endif

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

        // Copy texture to renderer (CPU mode only)
#ifndef USE_GPU_RENDERER
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);

        // Render controls panel if active
        if (show_controls) {
            const char* mode_name = analysis.get_mode_name();
            controls_panel.render(renderer, window_width, window_height,
                                 current_quality, preset, fps, render_time, mode_name, enable_shadows, enable_reflections);
        }

        // Render help overlay if active
        if (show_help) {
            help_overlay.render(renderer, window_width, window_height);
        }

        SDL_RenderPresent(renderer);
#else
        // GPU mode - render a test frame initially to ensure window is visible
        static bool first_frame = true;
        if (first_frame) {
            std::cout << "Rendering first frame to ensure window is visible..." << std::endl;
            first_frame = false;
        }
#endif

        // Small delay to prevent 100% CPU usage
        SDL_Delay(1);
    }

    // Cleanup
#ifdef USE_GPU_RENDERER
    // GPU cleanup - OpenGL context will be destroyed by SDL
#else
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "\n\n=== Exiting ===\n";
    std::cout << "Final Stats:\n";
    std::cout << "  Resolution: " << image_width << "x" << image_height << "\n";
    std::cout << "  Samples: " << preset.samples << "\n";
    std::cout << "  Max Depth: " << preset.max_depth << "\n";

    return 0;
}
