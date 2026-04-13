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
#include "renderer/renderer.h"

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
    v_uv.y = 1.0 - v_uv.y;
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

vec3 ray_color(Ray r, int depth) {
    HitRecord rec;
    vec3 color = vec3(1.0, 1.0, 1.0);  // Start with white
    vec3 accum_color = vec3(0.0, 0.0, 0.0);
    vec3 throughput = vec3(1.0, 1.0, 1.0);

    for (int bounce = 0; bounce < depth; bounce++) {
        // Background gradient
        if (!scene_hit(r, 0.001, 1000.0, rec)) {
            vec3 unit_direction = normalize(r.direction);
            float t = 0.5 * (unit_direction.y + 1.0);
            color = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
            accum_color += throughput * color;
            break;
        }

        // Simple lighting
        vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
        vec3 light_color = vec3(1.0, 1.0, 1.0);
        vec3 ambient = vec3(0.1, 0.1, 0.1);

        // Diffuse
        float diff = max(dot(rec.normal, light_dir), 0.0);

        // Specular
        vec3 view_dir = normalize(-r.direction);
        vec3 reflect_dir = reflect(-light_dir, rec.normal);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

        // Get material color
        vec3 albedo = sphere_colors[rec.material_id];

        // Combine lighting
        color = ambient * albedo + diff * light_color * albedo + spec * light_color * 0.5;
        accum_color += throughput * color;

        // Reflection (only for reflective materials)
        if (bounce < depth - 1 && albedo.r > 0.5) {
            vec3 reflected = reflect(normalize(-r.direction), rec.normal);
            r.origin = rec.p + 0.001 * rec.normal;
            r.direction = normalize(reflected);
            throughput *= 0.5 * albedo;  // Attenuate
        } else {
            break;
        }
    }

    return accum_color;
}

// Pseudo-random number generator
float rand_seed = 0.0;
float random_float() {
    rand_seed = fract(sin(dot(vec2(rand_seed, rand_seed * 0.1), vec2(12.9898, 78.233))) * 43758.5453);
    return rand_seed;
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
    {320, 1, 1, "Preview (Ultra Fast)"},
    {640, 1, 3, "Low (Fast)"},
    {800, 4, 3, "Medium"},
    {1280, 16, 5, "High"},
    {1600, 64, 8, "Ultra"},
    {1920, 128, 10, "Maximum Quality"}
};

const int NUM_QUALITY_LEVELS = 6;

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
        : position(0, 1, 3), lookat(0, 0, -1), vup(0, 1, 0),
          vfov(60), aspect_ratio(16.0f / 9.0f), aperture(0.0f),
          dist_to_focus(3.0f), yaw(-180.0f), pitch(0.0f) {
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
            "1-3: Quality | H: Help | SPACE: Pause",
            "Click: Capture mouse | ESC: Quit"
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

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Initial quality level
    int current_quality = 2; // Start at "Low" quality
    QualityPreset preset = quality_levels[current_quality];

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
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
#else
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
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

    // Setup scene
    Scene scene;
    scene.ambient_light = Color(0.1f, 0.1f, 0.1f);

    // Materials
    auto material_red = std::make_shared<Lambertian>(Color(0.65f, 0.05f, 0.05f));
    auto material_green = std::make_shared<Lambertian>(Color(0.12f, 0.45f, 0.15f));
    auto material_gray = std::make_shared<Lambertian>(Color(0.73f, 0.73f, 0.73f));
    auto material_blue = std::make_shared<Lambertian>(Color(0.1f, 0.2f, 0.7f));
    auto material_yellow = std::make_shared<Lambertian>(Color(0.8f, 0.7f, 0.1f));
    auto material_metal = std::make_shared<Metal>(Color(0.8f, 0.8f, 0.8f), 0.0);
    auto material_metal_fuzz = std::make_shared<Metal>(Color(0.7f, 0.6f, 0.5f), 0.3);
    auto material_gold = std::make_shared<Metal>(Color(1.0f, 0.77f, 0.35f), 0.1);

    // Cornell box walls
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, -5.5), 5.0, material_green));
    scene.add_object(std::make_shared<Sphere>(Point3(0, -5.5, 0), 5.0, material_gray));
    scene.add_object(std::make_shared<Sphere>(Point3(0, 5.5, 0), 5.0, material_gray));
    scene.add_object(std::make_shared<Sphere>(Point3(-5.5, 0, 0), 5.0, material_red));
    scene.add_object(std::make_shared<Sphere>(Point3(5.5, 0, 0), 5.0, material_green));

    // Objects
    scene.add_object(std::make_shared<Sphere>(Point3(0, 0, 0), 0.5, material_gold));
    scene.add_object(std::make_shared<Sphere>(Point3(-1.2, 0.2, -0.5), 0.35, material_metal_fuzz));
    scene.add_object(std::make_shared<Sphere>(Point3(1.2, -0.1, -0.8), 0.4, material_blue));
    scene.add_object(std::make_shared<Sphere>(Point3(0, -0.5, 0.5), 0.25, material_red));
    scene.add_object(std::make_shared<Sphere>(Point3(-0.6, -0.4, 0.8), 0.2, material_yellow));

    // Triangles - pyramid
    Point3 pyramid_top(0.0f, 0.9f, -1.8f);
    Point3 pyramid_base1(-0.6f, -0.3f, -2.3f);
    Point3 pyramid_base2(0.6f, -0.3f, -2.3f);
    Point3 pyramid_base3(0.0f, -0.3f, -1.3f);

    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base1, pyramid_base2, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base2, pyramid_base3, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_top, pyramid_base3, pyramid_base1, material_green));
    scene.add_object(std::make_shared<Triangle>(pyramid_base1, pyramid_base3, pyramid_base2, material_gray));

    // Lighting
    scene.add_light(Light(Point3(0, 4.9, 0), Color(1.0f, 1.0f, 1.0f)));

    // Camera controller
    CameraController camera_controller;

    // Framebuffer
    std::vector<unsigned char> framebuffer(image_width * image_height * 3);

    // Initialize renderer
#ifdef USE_GPU_RENDERER
    std::cout << "GPU Renderer initialized (OpenGL 3.3)" << std::endl;
#else
    Renderer ray_renderer(preset.max_depth);
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
    std::cout << "  Left Click    - Capture/release mouse\n";
    std::cout << "  1-6           - Change quality level\n";
    std::cout << "  H             - Toggle help overlay\n";
    std::cout << "  Space         - Pause rendering\n";
    std::cout << "  S             - Save screenshot (gpu_render_*.png in current dir)\n";
    std::cout << "  O             - Save output (renders/cornell_box_*.png, like CPU mode)\n";
    std::cout << "  ESC           - Quit (auto-saves to renders/)\n";
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
                    case SDLK_SPACE:
                        paused = !paused;
                        std::cout << (paused ? "Paused" : "Resumed") << std::endl;
                        break;
                    case SDLK_s: {  // Save screenshot
                        // Generate filename with timestamp
                        auto now = std::chrono::system_clock::now();
                        auto time = std::chrono::system_clock::to_time_t(now);
                        std::stringstream ss;
                        ss << "gpu_render_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
#ifdef USE_GPU_RENDERER
                        save_gpu_framebuffer(image_width, image_height, ss.str().c_str());
#else
                        std::cout << "Save only available in GPU mode" << std::endl;
#endif
                        break;
                    }
                    case SDLK_o: {  // Save output like CPU batch mode
                        system("mkdir -p renders");
                        auto now = std::chrono::system_clock::now();
                        auto time = std::chrono::system_clock::to_time_t(now);
                        std::stringstream ss;
                        ss << "renders/cornell_box_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
#ifdef USE_GPU_RENDERER
                        save_gpu_framebuffer(image_width, image_height, ss.str().c_str());
                        std::cout << "Output saved: " << ss.str() << std::endl;
#else
                        std::cout << "Save only available in GPU mode" << std::endl;
#endif
                        break;
                    }
                    case SDLK_1: case SDLK_2: case SDLK_3:
                    case SDLK_4: case SDLK_5: case SDLK_6: {
                        int new_quality = event.key.keysym.sym - SDLK_1;
                        if (new_quality != current_quality) {
                            current_quality = new_quality;
                            preset = quality_levels[current_quality];

#ifdef USE_GPU_RENDERER
                            // GPU renderer doesn't need re-initialization for quality changes
                            std::cout << "Quality: " << preset.name << " (" << preset.samples
                                     << " samples, depth " << preset.max_depth << ")" << std::endl;
#else
                            // Update CPU renderer (keep window size, only change quality)
                            ray_renderer = Renderer(preset.max_depth);
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
                    SDL_bool captured = SDL_GetRelativeMouseMode();
                    SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (SDL_GetRelativeMouseMode()) {
                    camera_controller.rotate(
                        event.motion.xrel * 0.1f,
                        event.motion.yrel * 0.1f  // Inverted vertical look
                    );
                    need_render = true;
                }
            }
        }

        // Handle continuous keyboard input
        const Uint8* keystates = SDL_GetKeyboardState(NULL);
        float move_speed = 0.1f;

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
        if (need_render && !paused) {
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

                for (const auto& obj : scene.objects) {
                    // Try to cast to sphere
                    auto sphere = std::dynamic_pointer_cast<Sphere>(obj);
                    if (sphere) {
                        sphere_centers_vec.push_back(sphere->center);
                        sphere_radii_vec.push_back(sphere->radius);
                        sphere_colors_vec.push_back(sphere->mat->albedo);
                    }
                }

                gpu_sphere_count = sphere_centers_vec.size();
                std::cout << "Uploaded " << gpu_sphere_count << " spheres to GPU" << std::endl;

                // Copy vector data to arrays (THIS WAS MISSING!)
                for (size_t i = 0; i < sphere_centers_vec.size() && i < 10; i++) {
                    gpu_sphere_centers[i] = sphere_centers_vec[i];
                    gpu_sphere_radii[i] = sphere_radii_vec[i];
                    gpu_sphere_colors[i] = sphere_colors_vec[i];
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
                auto glGetUniformLocation = (GLint (*)(GLuint, const GLchar *))SDL_GL_GetProcAddress("glGetUniformLocation");
                auto glUniform1f = (void (*)(GLint, GLfloat))SDL_GL_GetProcAddress("glUniform1f");
                auto glUniform2f = (void (*)(GLint, GLfloat, GLfloat))SDL_GL_GetProcAddress("glUniform2f");
                auto glUniform3f = (void (*)(GLint, GLfloat, GLfloat, GLfloat))SDL_GL_GetProcAddress("glUniform3f");
                auto glUniform3fv = (void (*)(GLint, GLsizei, const GLfloat *))SDL_GL_GetProcAddress("glUniform3fv");
                auto glUniform1fv = (void (*)(GLint, GLsizei, const GLfloat *))SDL_GL_GetProcAddress("glUniform1fv");
                auto glUniform1i = (void (*)(GLint, GLint))SDL_GL_GetProcAddress("glUniform1i");
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

                    for (int s = 0; s < preset.samples; ++s) {
                        float u = (i + random_float()) / (image_width - 1);
                        float v = (j + random_float()) / (image_height - 1);

                        Ray r = cam.get_ray(u, v);
                        pixel_color = pixel_color + ray_renderer.ray_color(r, scene, ray_renderer.max_depth);
                    }

                    float scale = 1.0f / preset.samples;
                    pixel_color = pixel_color * scale;

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

        // Render help overlay if active
        if (show_help) {
            int window_width, window_height;
            SDL_GetWindowSize(window, &window_width, &window_height);
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

#ifdef USE_GPU_RENDERER
    // Auto-save final render to renders/ directory (like CPU batch mode)
    system("mkdir -p renders");
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "renders/cornell_box_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".png";
    save_gpu_framebuffer(image_width, image_height, ss.str().c_str());
    std::cout << "Auto-saved final render: " << ss.str() << std::endl;
#endif

    std::cout << "\n\n=== Exiting ===\n";
    std::cout << "Final Stats:\n";
    std::cout << "  Resolution: " << image_width << "x" << image_height << "\n";
    std::cout << "  Samples: " << preset.samples << "\n";
    std::cout << "  Max Depth: " << preset.max_depth << "\n";

    return 0;
}
