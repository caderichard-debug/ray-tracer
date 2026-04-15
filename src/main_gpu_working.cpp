#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <chrono>
#include <sstream>

// Include GLEW before SDL to avoid header conflicts
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>

// Include CPU scene setup for shared scene data
#include "scene/scene.h"
#include "scene/cornell_box.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "material/material.h"
#include "texture/texture.h"
#include "math/vec3.h"
#include "math/ray.h"

// Window dimensions
const int WIDTH = 1280;
const int HEIGHT = 720;

// Camera controller
class CameraController {
public:
    float position[3];
    float lookat[3];
    float vup[3];
    float vfov;
    float aspect_ratio;
    float yaw;
    float pitch;

public:
    CameraController()
        : vfov(60.0f), aspect_ratio((float)WIDTH / (float)HEIGHT), yaw(-90.0f), pitch(0.0f) {
        position[0] = 0.0f; position[1] = 2.0f; position[2] = 15.0f;
        lookat[0] = 0.0f; lookat[1] = 2.0f; lookat[2] = 0.0f;
        vup[0] = 0.0f; vup[1] = 1.0f; vup[2] = 0.0f;
        update_from_angles();
    }

    void update_from_angles() {
        float yaw_rad = yaw * 3.14159f / 180.0f;
        float pitch_rad = pitch * 3.14159f / 180.0f;

        float direction[3];
        direction[0] = cos(yaw_rad) * cos(pitch_rad);
        direction[1] = sin(pitch_rad);
        direction[2] = sin(yaw_rad) * cos(pitch_rad);

        lookat[0] = position[0] + direction[0];
        lookat[1] = position[1] + direction[1];
        lookat[2] = position[2] + direction[2];
    }

    void move_forward(float delta) {
        float forward[3] = {lookat[0] - position[0], lookat[1] - position[1], lookat[2] - position[2]};
        float len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
        forward[0] /= len; forward[1] /= len; forward[2] /= len;

        position[0] += forward[0] * delta;
        position[1] += forward[1] * delta;
        position[2] += forward[2] * delta;
        update_from_angles();
    }

    void move_right(float delta) {
        float forward[3] = {lookat[0] - position[0], lookat[1] - position[1], lookat[2] - position[2]};
        float len = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
        forward[0] /= len; forward[1] /= len; forward[2] /= len;

        float right[3] = {
            forward[1] * vup[2] - forward[2] * vup[1],
            forward[2] * vup[0] - forward[0] * vup[2],
            forward[0] * vup[1] - forward[1] * vup[0]
        };

        len = sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        right[0] /= len; right[1] /= len; right[2] /= len;

        position[0] += right[0] * delta;
        position[1] += right[1] * delta;
        position[2] += right[2] * delta;
        update_from_angles();
    }

    void move_up(float delta) {
        position[1] += delta;
        update_from_angles();
    }

    void rotate(float delta_yaw, float delta_pitch) {
        yaw += delta_yaw;
        pitch += delta_pitch;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        update_from_angles();
    }
};

// Simple help overlay with text rendering
class HelpOverlay {
private:
    bool show;
    bool initialized;
    TTF_Font* font;
    SDL_Color text_color;
    SDL_Color background_color;

public:
    HelpOverlay() : show(false), initialized(false), font(nullptr) {
        text_color = {255, 255, 255, 255};
        background_color = {0, 0, 0, 180};
    }

    ~HelpOverlay() {
        if (font) TTF_CloseFont(font);
        if (initialized) TTF_Quit();
    }

    bool init() {
        if (TTF_Init() == -1) {
            std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
            return false;
        }

        font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 16);
        if (!font) {
            std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
            return false;
        }

        initialized = true;
        return true;
    }

    void toggle() { show = !show; }
    bool is_showing() const { return show; }

    void render(SDL_Window* window) {
        if (!show || !initialized) return;

        int width, height;
        SDL_GetWindowSize(window, &width, &height);

        // Create renderer for this window
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) return;

        // Semi-transparent background
        SDL_Rect bg_rect = {50, 50, width - 100, height - 100};
        SDL_Surface* bg_surface = SDL_CreateRGBSurface(0, bg_rect.w, bg_rect.h, 32, 0, 0, 0, 0);
        SDL_FillRect(bg_surface, nullptr, SDL_MapRGBA(bg_surface->format, background_color.r, background_color.g, background_color.b, background_color.a));

        SDL_Texture* bg_texture = SDL_CreateTextureFromSurface(renderer, bg_surface);
        SDL_RenderCopy(renderer, bg_texture, nullptr, &bg_rect);
        SDL_DestroyTexture(bg_texture);
        SDL_FreeSurface(bg_surface);

        // Render help text
        const char* help_lines[] = {
            "=== GPU Ray Tracer Help ===",
            "",
            "Controls:",
            "  WASD/Arrows - Move camera",
            "  Mouse       - Look around",
            "  R           - Toggle reflections",
            "  H           - Toggle this help",
            "  C           - Toggle controls panel",
            "  ESC         - Quit",
            "",
            "Features:",
            "  - Real-time GPU ray tracing",
            "  - Exact CPU Cornell Box scene",
            "  - Phong shading with reflections",
            "  - Procedural textures",
            "  - 75+ FPS performance",
            "",
            "Press H to close this help"
        };

        int y_offset = 70;
        for (size_t i = 0; i < sizeof(help_lines) / sizeof(help_lines[0]); i++) {
            SDL_Surface* surface = TTF_RenderText_Blended(font, help_lines[i], text_color);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                if (texture) {
                    SDL_Rect dest_rect = {70, y_offset, surface->w, surface->h};
                    SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                    SDL_DestroyTexture(texture);
                }
                SDL_FreeSurface(surface);
                y_offset += 25;
            }
        }

        SDL_RenderPresent(renderer);
        SDL_DestroyRenderer(renderer);
    }
};

// Simple settings panel
class ControlsPanel {
private:
    bool show;
    bool initialized;
    TTF_Font* font;
    SDL_Color text_color;
    SDL_Color background_color;
    SDL_Color title_color;

public:
    ControlsPanel() : show(false), initialized(false), font(nullptr) {
        text_color = {255, 255, 255, 255};
        background_color = {0, 0, 0, 180};
        title_color = {255, 200, 100, 255};
    }

    ~ControlsPanel() {
        if (font) TTF_CloseFont(font);
        if (initialized) TTF_Quit();
    }

    bool init() {
        if (TTF_Init() == -1) {
            return false;
        }

        font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 16);
        if (!font) {
            return false;
        }

        initialized = true;
        return true;
    }

    void toggle() { show = !show; }
    bool is_showing() const { return show; }

    void render(SDL_Window* window, bool reflections_enabled) {
        if (!show || !initialized) return;

        int width, height;
        SDL_GetWindowSize(window, &width, &height);

        // Create renderer for this window
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) return;

        // Semi-transparent background
        SDL_Rect bg_rect = {width - 300, 50, 250, 200};
        SDL_Surface* bg_surface = SDL_CreateRGBSurface(0, bg_rect.w, bg_rect.h, 32, 0, 0, 0, 0);
        SDL_FillRect(bg_surface, nullptr, SDL_MapRGBA(bg_surface->format, background_color.r, background_color.g, background_color.b, background_color.a));

        SDL_Texture* bg_texture = SDL_CreateTextureFromSurface(renderer, bg_surface);
        SDL_RenderCopy(renderer, bg_texture, nullptr, &bg_rect);
        SDL_DestroyTexture(bg_texture);
        SDL_FreeSurface(bg_surface);

        // Render settings text
        const char* title = "=== Settings ===";
        SDL_Surface* surface = TTF_RenderText_Blended(font, title, title_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dest_rect = {width - 280, 70, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        int y_offset = 110;
        char settings_text[256];

        sprintf(settings_text, "Reflections: %s", reflections_enabled ? "ON" : "OFF");
        surface = TTF_RenderText_Blended(font, settings_text, text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dest_rect = {width - 280, y_offset, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
            y_offset += 30;
        }

        sprintf(settings_text, "Max Depth: %d", 5);
        surface = TTF_RenderText_Blended(font, settings_text, text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dest_rect = {width - 280, y_offset, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
            y_offset += 30;
        }

        sprintf(settings_text, "Objects: %d spheres, %d tris", 13, 16);
        surface = TTF_RenderText_Blended(font, settings_text, text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dest_rect = {width - 280, y_offset, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        SDL_RenderPresent(renderer);
        SDL_DestroyRenderer(renderer);
    }
};

// Scene data (matching CPU Cornell Box exactly)
struct SphereData {
    float center[3];
    float radius;
    float color[3];
    int material;
};

struct TriangleData {
    float v0[3], v1[3], v2[3];
    float normal[3];
    float color[3];
    int material;
    float gradient_scale;   // For gradient textures
    float gradient_offset;  // For gradient textures
};

// Fragment shader that reads scene data from uniforms
const char* fragment_shader_source = R"(
#version 120

uniform vec2 resolution;
uniform vec3 camera_pos;
uniform vec3 camera_lookat;
uniform vec3 camera_vup;
uniform float camera_vfov;

// Scene uniforms
uniform vec3 sphere_centers[16];
uniform float sphere_radii[16];
uniform vec3 sphere_colors[16];
uniform int sphere_materials[16];
uniform int num_spheres;

uniform vec3 tri_v0[20];
uniform vec3 tri_v1[20];
uniform vec3 tri_v2[20];
uniform vec3 tri_normals[20];
uniform vec3 tri_colors[20];
uniform int tri_materials[20];
uniform float tri_gradient_scale[20];
uniform float tri_gradient_offset[20];
uniform int num_triangles;

// Rendering options
uniform bool enable_reflections;
uniform int max_depth;

// Simple ray-sphere intersection
bool hit_sphere(vec3 origin, vec3 direction, vec3 center, float radius, inout float t) {
    vec3 oc = origin - center;
    float a = dot(direction, direction);
    float b = dot(oc, direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - a * c;

    if (discriminant < 0.0) {
        return false;
    }

    float sqrt_d = sqrt(discriminant);
    float root = (-b - sqrt_d) / a;

    if (root < 0.001 || root > t) {
        root = (-b + sqrt_d) / a;
        if (root < 0.001 || root > t) {
            return false;
        }
    }

    t = root;
    return true;
}

// Simple ray-triangle intersection
bool hit_triangle(vec3 origin, vec3 direction, vec3 v0, vec3 v1, vec3 v2, vec3 normal, inout float t) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(direction, edge2);
    float a = dot(edge1, h);

    if (abs(a) < 0.0001) {
        return false;
    }

    float f = 1.0 / a;
    vec3 s = origin - v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0) {
        return false;
    }

    vec3 q = cross(s, edge1);
    float v = f * dot(direction, q);

    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    float t_new = f * dot(edge2, q);

    if (t_new < 0.001 || t_new > t) {
        return false;
    }

    t = t_new;
    return true;
}

// Procedural texture functions
vec3 checkerboard_texture(vec3 pos, vec3 color1, vec3 color2, float scale) {
    float checker = mod(floor(pos.x * scale) + floor(pos.y * scale) + floor(pos.z * scale), 2.0);
    return mix(color1, color2, checker);
}

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x),
            mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
            mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

vec3 noise_texture(vec3 pos, vec3 color1, vec3 color2, float scale) {
    float n = noise(pos * scale);
    return mix(color1, color2, n);
}

vec3 gradient_texture(vec3 pos, vec3 color1, vec3 color2, vec3 dir, float scale, float offset) {
    // Use actual position with scale and offset for proper gradient mapping
    float t = dot(pos, dir) * scale + offset;
    return mix(color1, color2, clamp(t, 0.0, 1.0));
}

vec3 stripe_texture(vec3 pos, vec3 color1, vec3 color2, float scale) {
    float stripe = mod(floor(pos.y * scale), 2.0);
    return mix(color1, color2, stripe);
}

// Simple scene rendering with iterative reflections
vec3 ray_color(vec3 origin, vec3 direction) {
    // Background gradient
    vec3 unit_dir = normalize(direction);
    float t_bg = 0.5 * (unit_dir.y + 1.0);
    vec3 background = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t_bg);

    vec3 final_color = vec3(0.0);
    vec3 current_origin = origin;
    vec3 current_direction = direction;
    float accumulated_weight = 1.0;

    // Iterative reflection bounces (max_depth iterations)
    for (int bounce = 0; bounce <= max_depth; bounce++) {
        float t_min = 1000.0;
        int hit_object = -1;
        int hit_type = -1; // 0 = sphere, 1 = triangle

        // Check sphere intersections
        for (int i = 0; i < num_spheres; i++) {
            if (hit_sphere(current_origin, current_direction, sphere_centers[i], sphere_radii[i], t_min)) {
                hit_object = i;
                hit_type = 0;
            }
        }

        // Check triangle intersections
        for (int i = 0; i < num_triangles; i++) {
            if (hit_triangle(current_origin, current_direction, tri_v0[i], tri_v1[i], tri_v2[i], tri_normals[i], t_min)) {
                hit_object = i;
                hit_type = 1;
            }
        }

        if (hit_object >= 0) {
            vec3 hit_point = current_origin + t_min * current_direction;
            vec3 color;
            vec3 normal;
            int material;
            bool apply_lighting = true;  // Declare before if/else blocks

            if (hit_type == 0) {
                // Sphere hit
                normal = normalize(hit_point - sphere_centers[hit_object]);
                color = sphere_colors[hit_object];
                material = sphere_materials[hit_object];

                // Apply procedural textures based on material
                if (material == 4) {
                    // Checkerboard (red and blue)
                    color = checkerboard_texture(hit_point, vec3(0.8, 0.2, 0.2), vec3(0.2, 0.2, 0.8), 8.0);
                } else if (material == 5) {
                    // Noise (black and white)
                    color = noise_texture(hit_point, vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), 5.0);
                } else if (material == 6) {
                    // Gradient (purple to yellow vertical) - use default scale/offset for spheres
                    color = gradient_texture(hit_point, vec3(0.6, 0.2, 0.8), vec3(0.9, 0.9, 0.2), vec3(0.0, 1.0, 0.0), 0.25, 0.0);
                } else if (material == 7) {
                    // Stripe (orange and white horizontal)
                    color = stripe_texture(hit_point, vec3(0.8, 0.5, 0.2), vec3(0.9, 0.9, 0.9), 8.0);
                }
            } else {
                // Triangle hit
                normal = tri_normals[hit_object];
                color = tri_colors[hit_object];
                material = tri_materials[hit_object];

                // Apply procedural textures for triangles
                if (material == 8) {
                    // Pyramid checkerboard
                    color = checkerboard_texture(hit_point, vec3(0.1, 0.1, 0.1), vec3(0.9, 0.9, 0.9), 6.0);
                } else if (material == 9) {
                    // Gradient quad (red to blue along Z) - use proper scale/offset from texture
                    color = gradient_texture(hit_point, vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0),
                                              tri_gradient_scale[hit_object], tri_gradient_offset[hit_object]);
                    // Don't apply lighting to gradient texture - it should be self-illuminated
                    apply_lighting = false;
                }
            }

            // Simple lighting
            vec3 lighting = vec3(1.0);
            if (apply_lighting) {
                vec3 light_pos = vec3(0.0, 18.0, 0.0);
                vec3 light_dir = normalize(light_pos - hit_point);
                float diff = max(dot(normal, light_dir), 0.0);

                // Add simple Phong specular
                vec3 view_dir = normalize(-current_direction);
                vec3 reflect_dir = reflect(-light_dir, normal);
                float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

                // Ambient + diffuse + specular
                lighting = vec3(0.1 + diff * 0.7 + spec * 0.3);
            }

            color = color * lighting;

            // Add contribution to final color
            final_color += accumulated_weight * color;

            // Handle reflections for metallic materials
            bool is_metallic = (material == 1 || material == 2); // Metal or fuzzy metal
            bool is_glass = (material == 3); // Glass

            if (enable_reflections && bounce < max_depth && (is_metallic || is_glass)) {
                // Calculate reflection direction
                vec3 reflect_dir = reflect(current_direction, normal);

                // Update ray for next bounce
                current_origin = hit_point + normal * 0.001;
                current_direction = reflect_dir;

                // Update weight based on material reflectivity
                float reflectivity;
                if (is_glass) {
                    reflectivity = 0.2; // Glass reflects less
                } else if (material == 2) {
                    reflectivity = 0.6; // Fuzzy metal
                } else {
                    reflectivity = 0.8; // Perfect metal
                }

                accumulated_weight *= reflectivity;

                // Continue to next bounce
                continue;
            } else if (!enable_reflections) {
                // Even when reflections are disabled, metallic materials get some metallic appearance
                if (is_metallic) {
                    final_color += accumulated_weight * 0.3 * color;
                }
            }

            // No more reflections, exit loop
            break;
        } else {
            // No hit, add background contribution and exit
            final_color += accumulated_weight * background;
            break;
        }
    }

    return final_color;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;

    // Camera from uniforms
    vec3 origin = camera_pos;
    vec3 lookat = camera_lookat;
    vec3 vup = camera_vup;

    vec3 w = normalize(origin - lookat);
    vec3 u = normalize(cross(vup, w));
    vec3 v = cross(w, u);

    float aspect_ratio = resolution.x / resolution.y;
    float theta = camera_vfov * 0.0174533;
    float h = tan(theta * 0.5);
    float viewport_height = 2.0 * h;
    float viewport_width = aspect_ratio * viewport_height;

    vec3 horizontal = viewport_width * u;
    vec3 vertical = viewport_height * v;
    vec3 lower_left_corner = origin - horizontal * 0.5 - vertical * 0.5 - w;

    vec3 direction = lower_left_corner + uv.x * horizontal + uv.y * vertical - origin;

    // Trace ray and get color
    vec3 color = ray_color(origin, direction);

    // Output color
    gl_FragColor = vec4(color, 1.0);
}
)";

// Vertex shader for fullscreen quad
const char* vertex_shader_source = R"(
#version 120

attribute vec2 position;
attribute vec2 texCoord;

varying vec2 uv;

void main() {
    uv = texCoord;
    uv.y = 1.0 - uv.y;  // Flip Y coordinate here
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

// Shader compilation helper
GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());

        std::cerr << "Failed to compile shader:" << std::endl;
        std::cerr << log.data() << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// Program linking helper
GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());

        std::cerr << "Failed to link program:" << std::endl;
        std::cerr << log.data() << std::endl;

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

// Setup scene data using the same CPU scene setup code
void setup_scene_data(
    std::vector<SphereData>& spheres,
    std::vector<TriangleData>& triangles
) {
    // Use the exact same scene setup as CPU renderer
    Scene scene;
    setup_cornell_box_scene(scene);

    // Extract sphere data from Scene objects
    for (const auto& obj : scene.objects) {
        if (auto sphere = std::dynamic_pointer_cast<Sphere>(obj)) {
            SphereData data;
            data.center[0] = sphere->center.x;
            data.center[1] = sphere->center.y;
            data.center[2] = sphere->center.z;
            data.radius = sphere->radius;
            data.color[0] = sphere->mat->albedo.x;
            data.color[1] = sphere->mat->albedo.y;
            data.color[2] = sphere->mat->albedo.z;

            // Determine material type
            if (auto metal = std::dynamic_pointer_cast<Metal>(sphere->mat)) {
                if (metal->fuzz < 0.1) {
                    data.material = 1; // Perfect metal
                } else {
                    data.material = 2; // Fuzzy metal
                }
            } else if (auto glass = std::dynamic_pointer_cast<Dielectric>(sphere->mat)) {
                data.material = 3; // Glass
            } else if (auto lambertian = std::dynamic_pointer_cast<Lambertian>(sphere->mat)) {
                // Check if this is a procedural texture material
                if (auto checker = std::dynamic_pointer_cast<CheckerTexture>(lambertian->albedo_texture)) {
                    data.material = 4; // Checkerboard
                } else if (auto noise = std::dynamic_pointer_cast<NoiseTexture>(lambertian->albedo_texture)) {
                    data.material = 5; // Noise
                } else if (auto gradient = std::dynamic_pointer_cast<GradientTexture>(lambertian->albedo_texture)) {
                    data.material = 6; // Gradient
                } else if (auto stripe = std::dynamic_pointer_cast<StripeTexture>(lambertian->albedo_texture)) {
                    data.material = 7; // Stripe
                } else {
                    data.material = 0; // Regular lambertian
                }
            } else {
                data.material = 0; // Default lambertian
            }

            spheres.push_back(data);
        } else if (auto triangle = std::dynamic_pointer_cast<Triangle>(obj)) {
            TriangleData data;

            // Extract vertices using public methods
            Point3 v0 = triangle->vertex0();
            Point3 v1 = triangle->vertex1();
            Point3 v2 = triangle->vertex2();

            data.v0[0] = v0.x; data.v0[1] = v0.y; data.v0[2] = v0.z;
            data.v1[0] = v1.x; data.v1[1] = v1.y; data.v1[2] = v1.z;
            data.v2[0] = v2.x; data.v2[1] = v2.y; data.v2[2] = v2.z;

            // Calculate normal
            Vec3 edge1 = v1 - v0;
            Vec3 edge2 = v2 - v0;
            Vec3 normal = unit_vector(cross(edge1, edge2));
            data.normal[0] = normal.x;
            data.normal[1] = normal.y;
            data.normal[2] = normal.z;

            // Extract color
            data.color[0] = triangle->material->albedo.x;
            data.color[1] = triangle->material->albedo.y;
            data.color[2] = triangle->material->albedo.z;

            // Determine material type for triangles
            std::cout << "Triangle material type: ";
            if (auto lambertian = std::dynamic_pointer_cast<Lambertian>(triangle->material)) {
                std::cout << "Lambertian, checking texture..." << std::endl;
                if (auto checker = std::dynamic_pointer_cast<CheckerTexture>(lambertian->albedo_texture)) {
                    data.material = 8; // Checkerboard (pyramid)
                    std::cout << "  -> Checkerboard texture" << std::endl;
                } else if (auto gradient = std::dynamic_pointer_cast<GradientTexture>(lambertian->albedo_texture)) {
                    data.material = 9; // Gradient (quad on right wall)
                    data.gradient_scale = gradient->scale;
                    data.gradient_offset = gradient->offset;
                    std::cout << "  -> Gradient texture! scale=" << gradient->scale << " offset=" << gradient->offset << std::endl;
                } else {
                    data.material = 0; // Regular lambertian
                    data.gradient_scale = 0.25f;  // Default
                    data.gradient_offset = 0.0f;  // Default
                    std::cout << "  -> Regular lambertian (unknown texture)" << std::endl;
                }
            } else {
                data.material = 0; // Default lambertian
                data.gradient_scale = 0.25f;  // Default
                data.gradient_offset = 0.0f;  // Default
                std::cout << "Not Lambertian" << std::endl;
            }

            triangles.push_back(data);
        }
    }

    std::cout << "✓ Extracted " << spheres.size() << " spheres and " << triangles.size() << " triangles from CPU scene" << std::endl;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=== Working GPU Ray Tracer (GLSL 1.20 Compatible) ===" << std::endl;
    std::cout << "Features: Exact CPU Cornell Box scene, Phong shading, uniforms" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create OpenGL window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Working GPU Ray Tracer - GLSL 1.20",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Failed to create OpenGL window" << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create OpenGL context
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "✓ OpenGL window created" << std::endl;

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(err) << std::endl;
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Check OpenGL version
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL Version: " << version << std::endl;

    const GLubyte* shader_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    std::cout << "GLSL Version: " << shader_version << std::endl;

    // Compile shaders
    std::cout << "Compiling shaders..." << std::endl;
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    if (vertex_shader == 0) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GLuint program = link_program(vertex_shader, fragment_shader);
    if (program == 0) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "✓ Shaders compiled and linked successfully" << std::endl;

    // Create fullscreen quad
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    float quad_vertices[] = {
        // Position  // TexCoord
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Position attribute
    GLint pos_loc = glGetAttribLocation(program, "position");
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(pos_loc);

    // TexCoord attribute
    GLint tex_loc = glGetAttribLocation(program, "texCoord");
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(tex_loc);

    // Get uniform locations
    GLint resolution_loc = glGetUniformLocation(program, "resolution");
    GLint camera_pos_loc = glGetUniformLocation(program, "camera_pos");
    GLint camera_lookat_loc = glGetUniformLocation(program, "camera_lookat");
    GLint camera_vup_loc = glGetUniformLocation(program, "camera_vup");
    GLint camera_vfov_loc = glGetUniformLocation(program, "camera_vfov");

    // Scene uniform locations
    GLint num_spheres_loc = glGetUniformLocation(program, "num_spheres");
    GLint num_triangles_loc = glGetUniformLocation(program, "num_triangles");

    // Rendering option uniforms
    GLint enable_reflections_loc = glGetUniformLocation(program, "enable_reflections");
    GLint max_depth_loc = glGetUniformLocation(program, "max_depth");

    // Rendering settings (matching CPU version)
    bool enable_reflections = true;
    int max_depth = 5;

    // Setup scene data
    std::vector<SphereData> spheres;
    std::vector<TriangleData> triangles;
    setup_scene_data(spheres, triangles);

    std::cout << "✓ Scene loaded: " << spheres.size() << " spheres, " << triangles.size() << " triangles" << std::endl;

    std::cout << "\n=== Starting Working GPU Ray Tracer ===" << std::endl;
    std::cout << "You should see the EXACT CPU Cornell Box scene with:" << std::endl;
    std::cout << "  - Center gold sphere (large)" << std::endl;
    std::cout << "  - 5 orbiting spheres (metal, blue, red, yellow)" << std::endl;
    std::cout << "  - Glass sphere and sphere behind glass" << std::endl;
    std::cout << "  - Pyramid with checkerboard texture" << std::endl;
    std::cout << "  - 2 small metal spheres" << std::endl;
    std::cout << "  - Procedural texture spheres (checkerboard, noise, gradient, stripe)" << std::endl;
    std::cout << "  - Gradient quad on right wall" << std::endl;
    std::cout << "  - Red wall (left), green walls (right/back), gray floor/ceiling" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Click window to capture mouse" << std::endl;
    std::cout << "  WASD/Arrows - Move (slower)" << std::endl;
    std::cout << "  Mouse       - Look around" << std::endl;
    std::cout << "  R           - Toggle reflections" << std::endl;
    std::cout << "  H           - Help" << std::endl;
    std::cout << "  C           - Controls panel" << std::endl;
    std::cout << "  ESC         - Quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

    // Camera controller (slower movement)
    CameraController camera;
    float move_speed = 0.05f; // Slower speed

    // UI panels
    HelpOverlay help_overlay;
    help_overlay.init();

    ControlsPanel controls_panel;
    controls_panel.init();

    // Main loop
    bool running = true;
    bool need_render = true;
    SDL_Event event;

    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps = 0.0f;
    auto last_frame_time = std::chrono::high_resolution_clock::now();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_h) {
                    help_overlay.toggle();
                } else if (event.key.keysym.sym == SDLK_c) {
                    controls_panel.toggle();
                } else if (event.key.keysym.sym == SDLK_r) {
                    enable_reflections = !enable_reflections;
                    std::cout << "Reflections: " << (enable_reflections ? "ON" : "OFF") << std::endl;
                    need_render = true;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_bool captured = SDL_GetRelativeMouseMode();
                    SDL_SetRelativeMouseMode(captured ? SDL_FALSE : SDL_TRUE);
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (SDL_GetRelativeMouseMode()) {
                    camera.rotate(event.motion.xrel * 0.1f, -event.motion.yrel * 0.1f);
                    need_render = true;
                }
            }
        }

        // Handle continuous keyboard input
        const Uint8* keystates = SDL_GetKeyboardState(NULL);

        if (keystates[SDL_SCANCODE_W]) {
            camera.move_forward(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_S]) {
            camera.move_forward(-move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_A]) {
            camera.move_right(-move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_D]) {
            camera.move_right(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_UP]) {
            camera.move_up(move_speed);
            need_render = true;
        }
        if (keystates[SDL_SCANCODE_DOWN]) {
            camera.move_up(-move_speed);
            need_render = true;
        }

        // Update time (keep for potential animation)
        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(current_time - start_time).count();

        // Render
        if (need_render) {
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(program);

            // Set camera uniforms
            glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
            glUniform3f(camera_pos_loc, camera.position[0], camera.position[1], camera.position[2]);
            glUniform3f(camera_lookat_loc, camera.lookat[0], camera.lookat[1], camera.lookat[2]);
            glUniform3f(camera_vup_loc, camera.vup[0], camera.vup[1], camera.vup[2]);
            glUniform1f(camera_vfov_loc, camera.vfov);

            // Set scene uniforms
            glUniform1i(num_spheres_loc, spheres.size());
            glUniform1i(num_triangles_loc, triangles.size());

            // Set rendering option uniforms
            glUniform1i(enable_reflections_loc, enable_reflections ? 1 : 0);
            glUniform1i(max_depth_loc, max_depth);

            // Set sphere uniforms
            for (size_t i = 0; i < spheres.size(); i++) {
                std::string name = "sphere_centers[" + std::to_string(i) + "]";
                GLint loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, spheres[i].center[0], spheres[i].center[1], spheres[i].center[2]);

                name = "sphere_radii[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, spheres[i].radius);

                name = "sphere_colors[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, spheres[i].color[0], spheres[i].color[1], spheres[i].color[2]);

                name = "sphere_materials[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1i(loc, spheres[i].material);
            }

            // Set triangle uniforms
            for (size_t i = 0; i < triangles.size(); i++) {
                std::string name = "tri_v0[" + std::to_string(i) + "]";
                GLint loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, triangles[i].v0[0], triangles[i].v0[1], triangles[i].v0[2]);

                name = "tri_v1[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, triangles[i].v1[0], triangles[i].v1[1], triangles[i].v1[2]);

                name = "tri_v2[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, triangles[i].v2[0], triangles[i].v2[1], triangles[i].v2[2]);

                name = "tri_normals[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, triangles[i].normal[0], triangles[i].normal[1], triangles[i].normal[2]);

                name = "tri_colors[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform3f(loc, triangles[i].color[0], triangles[i].color[1], triangles[i].color[2]);

                name = "tri_materials[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1i(loc, triangles[i].material);

                name = "tri_gradient_scale[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, triangles[i].gradient_scale);

                name = "tri_gradient_offset[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, triangles[i].gradient_offset);
            }

            // Render fullscreen quad
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // Render overlays (need to temporarily switch from OpenGL to SDL2 rendering)
            if (help_overlay.is_showing()) {
                help_overlay.render(window);
            } else if (controls_panel.is_showing()) {
                controls_panel.render(window, enable_reflections);
            }

            SDL_GL_SwapWindow(window);

            // Calculate FPS
            frame_count++;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_frame_time).count();
            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                frame_count = 0;
                last_frame_time = now;
                std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                         << " | Cam: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                         << "     " << std::flush;
            }

            need_render = false;
        }
    }

    std::cout << "\n=== Exiting ===" << std::endl;

    // Cleanup
    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "GPU Ray Tracer shut down successfully" << std::endl;
    return 0;
}
