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
#include "scene/gpu_demo.h"
#include "scene/pbr_showcase.h"
#include "scene/dof_showcase.h"
#include "scene/fx_showcase.h"
#include "primitives/sphere.h"
#include "primitives/triangle.h"
#include "material/material.h"
#include "texture/texture.h"
#include "math/vec3.h"
#include "math/ray.h"

// Feature flags (set via Makefile)
#ifndef ENABLE_PBR
#define ENABLE_PBR 1  // Enable PBR lighting by default
#endif

#ifndef ENABLE_MULTIPLE_LIGHTS
#define ENABLE_MULTIPLE_LIGHTS 1  // Enable multiple light support
#endif

#ifndef ENABLE_TONE_MAPPING
#define ENABLE_TONE_MAPPING 1  // Enable ACES tone mapping
#endif

#ifndef ENABLE_GAMMA_CORRECTION
#define ENABLE_GAMMA_CORRECTION 1  // Enable gamma correction
#endif

#ifndef ENABLE_SOFT_SHADOWS
#define ENABLE_SOFT_SHADOWS 0  // Enable soft shadows (Phase 2)
#endif

#ifndef ENABLE_AMBIENT_OCCLUSION
#define ENABLE_AMBIENT_OCCLUSION 0  // Enable ambient occlusion (Phase 2)
#endif

#ifndef ENABLE_GI
#define ENABLE_GI 0  // Enable global illumination (Phase 3)
#endif

#ifndef GI_SAMPLES
#define GI_SAMPLES 4  // GI hemisphere samples (1-8)
#endif

#ifndef GI_INTENSITY
#define GI_INTENSITY 0.3f  // GI intensity (0.0-1.0)
#endif

// Phase 3.5: Advanced reflection features
#ifndef ENABLE_SSR
#define ENABLE_SSR 0  // Enable screen-space reflections
#endif

#ifndef ENABLE_ENV_MAPPING
#define ENABLE_ENV_MAPPING 0  // Enable environment mapping
#endif

#ifndef ENABLE_GLOSSY_REFLECTIONS
#define ENABLE_GLOSSY_REFLECTIONS 0  // Enable glossy reflections with roughness
#endif

#ifndef SSR_SAMPLES
#define SSR_SAMPLES 16  // SSR ray march samples
#endif

#ifndef SSR_STEP_SIZE
#define SSR_STEP_SIZE 0.01f  // SSR ray march step size
#endif

// Phase 4: Post-processing and visual effects
#ifndef ENABLE_SSAO
#define ENABLE_SSAO 0  // Enable screen-space ambient occlusion
#endif

#ifndef ENABLE_BLOOM
#define ENABLE_BLOOM 0  // Enable bloom/glow effect
#endif

#ifndef ENABLE_VIGNETTE
#define ENABLE_VIGNETTE 0  // Enable cinematic vignette
#endif

#ifndef ENABLE_FILM_GRAIN
#define ENABLE_FILM_GRAIN 0  // Enable film grain effect
#endif

// Phase 5: Chromatic Aberration
#ifndef ENABLE_CHROMATIC_ABERRATION
#define ENABLE_CHROMATIC_ABERRATION 0  // Enable chromatic aberration
#endif

#ifndef CHROMATIC_ABERRATION_STRENGTH
#define CHROMATIC_ABERRATION_STRENGTH 1.0  // Default strength (0.0-2.0)
#endif

// Phase 5: Depth of Field
#ifndef ENABLE_DOF
#define ENABLE_DOF 0  // Enable depth of field
#endif

#ifndef DOF_FOCUS_DISTANCE
#define DOF_FOCUS_DISTANCE 3.0  // Default focus distance
#endif

#ifndef DOF_APERTURE
#define DOF_APERTURE 0.1  // Default aperture size
#endif

// Phase 5: Motion Blur
#ifndef ENABLE_MOTION_BLUR
#define ENABLE_MOTION_BLUR 0  // Enable motion blur
#endif

#ifndef MOTION_BLUR_STRENGTH
#define MOTION_BLUR_STRENGTH 0.5  // Default motion blur strength
#endif

// Phase 5: Adaptive Quality
#ifndef ENABLE_ADAPTIVE_QUALITY
#define ENABLE_ADAPTIVE_QUALITY 0  // Enable adaptive quality
#endif

#ifndef TARGET_FPS
#define TARGET_FPS 60  // Target FPS for adaptive quality
#endif

#ifndef ADAPTIVE_QUALITY_AGGRESSIVENESS
#define ADAPTIVE_QUALITY_AGGRESSIVENESS 0.5  // How aggressively to adjust (0.1-1.0)
#endif

// Phase 5: Lens Flares
#ifndef ENABLE_LENS_FLARES
#define ENABLE_LENS_FLARES 0  // Enable lens flares
#endif

#ifndef LENS_FLARE_INTENSITY
#define LENS_FLARE_INTENSITY 1.0  // Default lens flare intensity
#endif

// Phase 6: Temporal Anti-Aliasing (TAA)
#ifndef ENABLE_TAA
#define ENABLE_TAA 0  // Enable temporal anti-aliasing
#endif

#ifndef TAA_MIX_FACTOR
#define TAA_MIX_FACTOR 0.9  // Default temporal blend factor (0.9 = smooth)
#endif

#ifndef TONE_MAPPING_OPERATOR
#define TONE_MAPPING_OPERATOR 1  // 0=None, 1=ACES, 2=Reinhard, 3=Filmic, 4=Uncharted
#endif

// Scene selection (set via Makefile)
#ifndef SCENE_NAME
#define SCENE_NAME "cornell_box"  // Default scene
#endif

// Window dimensions
// 640x360 keeps frames under ~1s on Intel integrated GPU (decent interactivity).
// Bump to 1280x720 if you have a discrete GPU.
const int WIDTH = 640;
const int HEIGHT = 360;

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
            "  P           - Toggle Phong/PBR lighting",
            "  L           - Cycle light configurations",
            "  H           - Toggle this help",
            "  C           - Toggle controls panel",
            "  ESC         - Quit",
            "",
            "Features:",
            "  - Real-time GPU ray tracing",
            "  - Exact CPU Cornell Box scene",
            "  - Phong & PBR lighting modes",
            "  - Multiple light configurations",
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

    void render(SDL_Window* window, bool reflections_enabled, int lighting_mode, int num_lights) {
        if (!show || !initialized) return;

        int width, height;
        SDL_GetWindowSize(window, &width, &height);

        // Create renderer for this window
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) return;

        // Semi-transparent background
        SDL_Rect bg_rect = {width - 300, 50, 250, 250};
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

        sprintf(settings_text, "Lighting: %s", lighting_mode == 0 ? "Phong" : "PBR");
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

        sprintf(settings_text, "Lights: %d", num_lights);
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
    float gradient_scale;   // For gradient textures
    float gradient_offset;  // For gradient textures
    float roughness;        // PBR roughness (0=mirror, 1=matte)
    float metallic;         // PBR metallic (0=dielectric, 1=metal)
};

struct TriangleData {
    float v0[3], v1[3], v2[3];
    float normal[3];
    float color[3];
    int material;
    float gradient_scale;   // For gradient textures
    float gradient_offset;  // For gradient textures
    float roughness;        // PBR roughness
    float metallic;         // PBR metallic
};

// Fragment shader that reads scene data from uniforms
// Note: Feature flags are injected from C++ macros during compilation
std::string fragment_shader_source =
    std::string(R"(#version 120

)") +
    "#define ENABLE_PBR " + (ENABLE_PBR ? "1" : "0") + "\n" +
    "#define ENABLE_MULTIPLE_LIGHTS " + (ENABLE_MULTIPLE_LIGHTS ? "1" : "0") + "\n" +
    "#define ENABLE_TONE_MAPPING " + (ENABLE_TONE_MAPPING ? "1" : "0") + "\n" +
    "#define ENABLE_GAMMA_CORRECTION " + (ENABLE_GAMMA_CORRECTION ? "1" : "0") + "\n" +
    "#define ENABLE_SOFT_SHADOWS " + (ENABLE_SOFT_SHADOWS ? "1" : "0") + "\n" +
    "#define ENABLE_AMBIENT_OCCLUSION " + (ENABLE_AMBIENT_OCCLUSION ? "1" : "0") + "\n" +
    "#define ENABLE_GI " + (ENABLE_GI ? "1" : "0") + "\n" +
    R"(

uniform vec2 resolution;
uniform vec3 camera_pos;
uniform vec3 camera_lookat;
uniform vec3 camera_vup;
uniform float camera_vfov;

// Scene uniforms
uniform vec3 sphere_centers[32];
uniform float sphere_radii[32];
uniform vec3 sphere_colors[32];
uniform int sphere_materials[32];
uniform float sphere_gradient_scale[32];
uniform float sphere_gradient_offset[32];
uniform float sphere_roughness[32];     // PBR roughness (0=mirror, 1=matte)
uniform float sphere_metallic[32];      // PBR metallic (0=dielectric, 1=metal)
uniform int num_spheres;

uniform vec3 tri_v0[20];
uniform vec3 tri_v1[20];
uniform vec3 tri_v2[20];
uniform vec3 tri_normals[20];
uniform vec3 tri_colors[20];
uniform int tri_materials[20];
uniform float tri_gradient_scale[20];
uniform float tri_gradient_offset[20];
uniform float tri_roughness[20];        // PBR roughness
uniform float tri_metallic[20];         // PBR metallic
uniform int num_triangles;

// Rendering options
uniform bool enable_reflections;
uniform int max_depth;

// PBR lighting options
uniform int lighting_mode;              // 0=Phong, 1=PBR
uniform int num_lights;                 // Number of active lights

// Light uniforms (up to 4 lights)
uniform vec3 light_positions[4];
uniform vec3 light_colors[4];
uniform float light_intensities[4];

// Advanced lighting options
uniform bool enable_soft_shadows;         // Enable soft shadows
uniform int soft_shadow_samples;          // Samples per light (1-4)
uniform float light_radius;              // Radius of area light

// Ambient occlusion options
uniform bool enable_ao;                  // Enable ambient occlusion
uniform int ao_samples;                  // AO samples (1-16)

// Global illumination options
uniform bool enable_gi;                  // Enable global illumination
uniform int gi_samples;                  // GI samples (1-8)
uniform float gi_intensity;              // GI intensity (0.0-1.0)

// Phase 3.5: Advanced reflection options
uniform bool enable_ssr;                 // Enable screen-space reflections
uniform int ssr_samples;                 // SSR ray march samples
uniform float ssr_step_size;             // SSR ray march step size
uniform float ssr_roughness_cutoff;      // Cutoff for glossy reflections (0.0-1.0)
uniform bool enable_env_mapping;         // Enable environment mapping
uniform int env_mip_levels;              // Environment map mip levels

// Phase 4: Post-processing options
uniform bool enable_ssao;                // Enable screen-space ambient occlusion
uniform int ssao_samples;                // SSAO samples (4-32)
uniform float ssao_radius;               // SSAO sample radius
uniform float ssao_intensity;            // SSAO intensity (0.0-2.0)

uniform bool enable_bloom;               // Enable bloom/glow effect
uniform float bloom_threshold;           // Bloom brightness threshold (0.5-2.0)
uniform float bloom_intensity;           // Bloom intensity (0.0-1.0)

uniform bool enable_vignette;            // Enable cinematic vignette
uniform float vignette_intensity;        // Vignette intensity (0.0-1.0)
uniform float vignette_falloff;          // Vignette falloff (0.1-1.0)

uniform bool enable_film_grain;          // Enable film grain effect
uniform float grain_intensity;           // Film grain intensity (0.0-1.0)
uniform float grain_size;                // Film grain size (1.0-10.0)

// Phase 5: Chromatic Aberration
uniform bool enable_chromatic_aberration; // Enable chromatic aberration
uniform float chromatic_aberration_strength; // Chromatic aberration strength (0.0-2.0)

// Phase 5: Depth of Field
uniform bool enable_dof;                 // Enable depth of field
uniform float dof_focus_distance;        // Focus distance (0.1-10.0)
uniform float dof_aperture;              // Aperture size (0.01-0.5)

// Phase 5: Motion Blur
uniform bool enable_motion_blur;         // Enable motion blur
uniform float motion_blur_strength;      // Motion blur intensity (0.0-1.0)
uniform vec2 motion_vector;              // Motion direction vector

// Phase 5: Adaptive Quality
uniform bool enable_adaptive_quality;    // Enable adaptive quality
uniform float quality_scale;             // Quality scale (0.5-1.5)

// Phase 5: Lens Flares
uniform bool enable_lens_flares;         // Enable lens flares
uniform float lens_flare_intensity;      // Lens flare intensity (0.0-2.0)
uniform vec2 light_position;             // Main light position on screen

// Phase 6: Temporal Anti-Aliasing (TAA)
uniform bool enable_taa;                 // Enable temporal anti-aliasing
uniform sampler2D history_buffer;        // Previous frame color buffer
uniform vec2 camera_velocity;            // Camera motion velocity
uniform float taa_mix_factor;           // Temporal blend factor (0.0-1.0)

uniform int tone_mapping_op;             // Tone mapping operator (0=None, 1=ACES, 2=Reinhard, 3=Filmic, 4=Uncharted)
uniform float exposure;                  // Exposure compensation (0.1-2.0)
uniform float contrast;                  // Contrast (0.8-1.2)
uniform float saturation;                // Saturation (0.8-1.2)

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

// ============================================================================
// COOK-TORRANCE BRDF FUNCTIONS (Physically Based Rendering)
// ============================================================================

// Constants
const float PI = 3.14159265359;

// Microfacet distribution function (GGX/Trowbridge-Reitz)
// D: How many microfacets are aligned to reflect light toward viewer
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return nom / denom;
}

// Geometry function (Smith method)
// G: Accounts for microfacets shadowing each other
float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);

    return ggx1 * ggx2;
}

// Fresnel equation (Schlick approximation)
// F: How much light reflects vs refracts (grazing angles become mirrors)
vec3 F_Schlick(float cosTheta, vec3 F0) {
    float pow5 = pow(1.0 - cosTheta, 5.0);
    return F0 + (1.0 - F0) * pow5;
}

// Full Cook-Torrance BRDF
// Returns: ratio of reflected radiance to incident irradiance
vec3 cook_torrance_brdf(vec3 N, vec3 V, vec3 L, vec3 H,
                        vec3 albedo, float roughness, float metallic) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // Base reflectivity (0.04 for most dielectrics)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Specular component (Cook-Torrance)
    float D = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3 F = F_Schlick(HdotV, F0);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotL * NdotV + 0.0001;
    vec3 specular = numerator / denominator;

    // Diffuse component (Lambertian)
    // Metals have no diffuse, dielectrics have full diffuse
    vec3 kS = F;  // Specular reflection
    vec3 kD = vec3(1.0) - kS;  // Diffuse reflection
    kD *= 1.0 - metallic;  // Metals have no diffuse component

    vec3 diffuse = kD * albedo / PI;

    return diffuse + specular;
}

// Simple Phong shading (for comparison/compatibility)
vec3 phong_shading(vec3 N, vec3 V, vec3 L, vec3 albedo, float specular_power) {
    vec3 light_dir = normalize(L);
    float diff = max(dot(N, light_dir), 0.0);

    vec3 view_dir = normalize(V);
    vec3 reflect_dir = reflect(-light_dir, N);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), specular_power);

    return albedo * (0.1 + diff * 0.7 + spec * 0.3);
}

// ============================================================================
// Soft shadow calculation using stratified area light sampling
float calculate_soft_shadow(vec3 hit_point, vec3 light_pos, vec3 N, vec3 L, float light_dist) {
    if (!enable_soft_shadows || soft_shadow_samples <= 1) {
        // Hard shadow fallback
        return 1.0;  // Will be computed inline
    }
    float shadow = 0.0;

    // Create orthonormal basis around light direction
    vec3 light_dir = normalize(light_pos - hit_point);
    vec3 light_tangent = normalize(cross(light_dir, vec3(0, 1, 0)));
    if (abs(light_tangent.x) < 0.01) {
        light_tangent = normalize(cross(light_dir, vec3(1, 0, 0)));
    }
    vec3 light_bitangent = cross(light_dir, light_tangent);

    // Stratified samples on light surface
    for (int i = 0; i < soft_shadow_samples; i++) {
        for (int j = 0; j < soft_shadow_samples; j++) {
            // Stratified sample positions
            float u = (float(i) + 0.5) / float(soft_shadow_samples);
            float v = (float(j) + 0.5) / float(soft_shadow_samples);
            u = u * 2.0 - 1.0;
            v = v * 2.0 - 1.0;

            // Point on area light
            vec3 sample_point = light_pos +
                light_tangent * u * light_radius +
                light_bitangent * v * light_radius;

            // Shadow ray to sample point
            vec3 sample_dir = normalize(sample_point - hit_point);
            float sample_dist = length(sample_point - hit_point);

            // Check for occluders
            vec3 shadow_origin = hit_point + N * 0.001;
            float t_shadow = sample_dist;
            bool occluded = false;

            // Check spheres
            for (int k = 0; k < num_spheres; k++) {
                if (hit_sphere(shadow_origin, sample_dir, sphere_centers[k], sphere_radii[k], t_shadow)) {
                    if (t_shadow < sample_dist) {
                        occluded = true;
                        break;
                    }
                }
            }

            // Check triangles
            if (!occluded) {
                for (int k = 0; k < num_triangles; k++) {
                    if (hit_triangle(shadow_origin, sample_dir, tri_v0[k], tri_v1[k], tri_v2[k],
                                   tri_normals[k], t_shadow)) {
                        if (t_shadow < sample_dist) {
                            occluded = true;
                            break;
                        }
                    }
                }
            }

            shadow += occluded ? 0.0 : 1.0;
        }
    }

    return shadow / float(soft_shadow_samples * soft_shadow_samples);
}

// ============================================================================
// Ray-traced ambient occlusion
float calculate_ao(vec3 hit_point, vec3 N) {
    if (!enable_ao || ao_samples <= 0) return 1.0;

    float ao = 0.0;

    for (int i = 0; i < ao_samples; i++) {
        // Random direction in hemisphere around normal
        float theta = acos(1.0 - 2.0 * float(i) / float(ao_samples) + 1.0 / float(ao_samples));
        float phi = 3.14159 * (1.0 + sqrt(5.0)) * float(i);

        float sin_theta = sin(theta);
        float cos_theta = cos(theta);
        float sin_phi = sin(phi);
        float cos_phi = cos(phi);

        vec3 sample_dir = vec3(
            sin_theta * cos_phi,
            sin_theta * sin_phi,
            cos_theta
        );

        // Align with normal
        vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
        vec3 tangent = normalize(cross(up, N));
        vec3 bitangent = cross(N, tangent);
        sample_dir = tangent * sample_dir.x + bitangent * sample_dir.y + N * sample_dir.z;

        // Check for nearby occluders
        vec3 sample_origin = hit_point + N * 0.001;
        float t_max = 0.5;  // AO radius
        bool occluded = false;

        // Check spheres
        for (int j = 0; j < num_spheres; j++) {
            float t = t_max;
            if (hit_sphere(sample_origin, sample_dir, sphere_centers[j], sphere_radii[j], t)) {
                if (t < t_max) {
                    occluded = true;
                    break;
                }
            }
        }

        // Check triangles
        if (!occluded) {
            for (int j = 0; j < num_triangles; j++) {
                float t = t_max;
                if (hit_triangle(sample_origin, sample_dir, tri_v0[j], tri_v1[j], tri_v2[j],
                               tri_normals[j], t)) {
                    if (t < t_max) {
                        occluded = true;
                        break;
                    }
                }
            }
        }

        ao += occluded ? 1.0 : 0.0;
    }

    return 1.0 - (ao / ao_samples);
}

// Global Illumination - Indirect lighting via hemisphere sampling
vec3 calculate_gi(vec3 hit_point, vec3 N, vec3 albedo) {
    if (!enable_gi || gi_samples <= 0) return vec3(0.0);

    vec3 gi_color = vec3(0.0);

    // Build orthonormal basis around normal
    vec3 tangent = normalize(abs(N.x) > abs(N.y) ? vec3(-N.z, 0.0, N.x) : vec3(0.0, -N.z, N.y));
    vec3 bitangent = cross(N, tangent);

    for (int i = 0; i < gi_samples; i++) {
        // Stratified hemisphere sampling
        float u = (float(i) + 0.5) / float(gi_samples);
        float v = fract(sin(float(i) * 12.9898) * 43758.5453);  // Random-ish

        // Uniform hemisphere sampling
        float cos_theta = 1.0 - u;
        float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
        float phi = 2.0 * 3.14159 * v;

        // Direction in hemisphere space
        vec3 sample_dir;
        sample_dir.x = cos(phi) * sin_theta;
        sample_dir.y = sin(phi) * sin_theta;
        sample_dir.z = cos_theta;

        // Transform to world space
        sample_dir = tangent * sample_dir.x + bitangent * sample_dir.y + N * sample_dir.z;

        // Cast indirect ray
        vec3 sample_origin = hit_point + N * 0.001;
        float t_min = 50.0;  // GI search radius
        bool hit = false;
        vec3 hit_color = vec3(0.0);

        // Check spheres
        for (int j = 0; j < num_spheres; j++) {
            if (hit_sphere(sample_origin, sample_dir, sphere_centers[j], sphere_radii[j], t_min)) {
                hit = true;
                hit_color = sphere_colors[j];
                break;
            }
        }

        // Check triangles
        if (!hit) {
            for (int j = 0; j < num_triangles; j++) {
                if (hit_triangle(sample_origin, sample_dir, tri_v0[j], tri_v1[j], tri_v2[j],
                               tri_normals[j], t_min)) {
                    hit = true;
                    hit_color = tri_colors[j];
                    break;
                }
            }
        }

        // Accumulate indirect lighting
        if (hit) {
            gi_color += hit_color * albedo;
        } else {
            // No hit, use environment approximation
            gi_color += mix(vec3(0.1), vec3(0.5, 0.7, 1.0), max(sample_dir.y, 0.0)) * albedo;
        }
    }

    return gi_color * (gi_intensity / float(gi_samples));
}

// Screen-space reflections using ray traced scene data
vec3 calculate_ssr(vec3 hit_point, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic) {
    if (!enable_ssr || ssr_samples <= 0) return vec3(0.0);

    vec3 ssr_color = vec3(0.0);

    // Only do SSR for reflective surfaces
    float reflectivity = mix(0.04, 1.0, metallic);
    if (reflectivity < 0.1) return ssr_color;

    // Calculate reflection direction
    vec3 R = reflect(-V, N);

    // Roughness-based cutoff (skip very rough surfaces)
    if (roughness > ssr_roughness_cutoff) return ssr_color;

    // Ray march in reflection direction
    vec3 ray_origin = hit_point + N * 0.01;  // Start slightly above surface
    vec3 ray_dir = R;
    float ray_step = ssr_step_size;

    vec3 accumulated_color = vec3(0.0);
    float total_weight = 0.0;

    for (int i = 0; i < ssr_samples; i++) {
        vec3 sample_point = ray_origin + ray_dir * (float(i) * ray_step);

        // Find closest intersection
        float t_min = 100.0;
        bool hit = false;
        vec3 hit_color = vec3(0.0);
        vec3 hit_normal = vec3(0.0);

        // Check spheres
        for (int j = 0; j < num_spheres; j++) {
            if (hit_sphere(ray_origin, ray_dir, sphere_centers[j], sphere_radii[j], t_min)) {
                vec3 hit_pt = ray_origin + ray_dir * t_min;
                vec3 normal = normalize(hit_pt - sphere_centers[j]);

                // Check if facing the reflection direction
                if (dot(ray_dir, normal) < 0.0) {
                    hit = true;
                    hit_color = sphere_colors[j];
                    hit_normal = normal;
                    break;
                }
            }
        }

        // Check triangles
        if (!hit) {
            for (int j = 0; j < num_triangles; j++) {
                if (hit_triangle(ray_origin, ray_dir, tri_v0[j], tri_v1[j], tri_v2[j],
                               tri_normals[j], t_min)) {
                    // Check if facing the reflection direction
                    if (dot(ray_dir, tri_normals[j]) < 0.0) {
                        hit = true;
                        hit_color = tri_colors[j];
                        hit_normal = tri_normals[j];
                        break;
                    }
                }
            }
        }

        if (hit) {
            // Calculate fresnel for this hit
            float NdotV = max(dot(hit_normal, -ray_dir), 0.0);
            vec3 F0 = mix(vec3(0.04), hit_color, metallic);
            vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

            // Weight by distance and fresnel
            float weight = exp(-float(i) * 0.1) * dot(F, vec3(1.0));
            accumulated_color += hit_color * F * weight;
            total_weight += weight;

            // First hit contributes most
            if (i == 0) break;
        }
    }

    if (total_weight > 0.0) {
        ssr_color = accumulated_color / total_weight;
    }

    // Apply roughness blur
    float roughness_factor = 1.0 - (roughness / ssr_roughness_cutoff);
    ssr_color *= roughness_factor * reflectivity;

    return ssr_color;
}

// Environment mapping for realistic sky lighting
vec3 calculate_env_mapping(vec3 hit_point, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic) {
    if (!enable_env_mapping) return vec3(0.0);

    // Calculate reflection direction
    vec3 R = reflect(-V, N);

    // Sample procedural environment based on reflection direction
    vec3 env_color;

    // Sky gradient (horizon to zenith)
    float sky_factor = max(R.y, 0.0);
    vec3 sky_color = mix(vec3(0.8, 0.9, 1.0), vec3(0.4, 0.6, 0.9), sky_factor);

    // Ground color
    vec3 ground_color = vec3(0.15, 0.12, 0.10);

    // Blend based on direction
    float horizon_blend = smoothstep(-0.1, 0.1, R.y);
    env_color = mix(ground_color, sky_color, horizon_blend);

    // Sun (bright spot in sky)
    vec3 sun_dir = normalize(vec3(0.5, 0.8, -0.3));
    float sun_dot = max(dot(R, sun_dir), 0.0);
    vec3 sun_color = vec3(1.0, 0.95, 0.8) * 5.0;
    vec3 sun_disk = pow(sun_dot, 64.0) * sun_color;
    env_color += sun_disk;

    // Fresnel effect for grazing angles
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // Roughness blur approximation
    float roughness_blur = roughness * 0.5;
    env_color = mix(env_color, vec3(dot(env_color, vec3(1.0/3.0))), roughness_blur);

    return env_color * F * 0.3;  // Scale intensity
}

// ============================================================================

// Calculate PBR lighting for a point
vec3 calculate_pbr_lighting(vec3 hit_point, vec3 N, vec3 V,
                            vec3 albedo, float roughness, float metallic) {
    vec3 Lo = vec3(0.0);  // Total outgoing radiance

    // Accumulate contribution from each light
    for (int i = 0; i < 4; i++) {
        if (i >= num_lights) break;

        vec3 light_pos = light_positions[i];
        vec3 light_color = light_colors[i];
        float light_intensity = light_intensities[i];

        vec3 L = normalize(light_pos - hit_point);
        vec3 H = normalize(V + L);
        float light_dist = length(light_pos - hit_point);

        // Shadow calculation (soft or hard)
        float shadow = 1.0;
        if (enable_soft_shadows && soft_shadow_samples > 1) {
            shadow = calculate_soft_shadow(hit_point, light_pos, N, L, light_dist);
        } else {
            // Hard shadow (single sample)
            vec3 shadow_origin = hit_point + N * 0.001;
            float t_shadow = light_dist;
            bool in_shadow = false;

            for (int j = 0; j < num_spheres; j++) {
                if (hit_sphere(shadow_origin, L, sphere_centers[j], sphere_radii[j], t_shadow)) {
                    if (t_shadow < light_dist) {
                        in_shadow = true;
                        break;
                    }
                }
            }

            if (!in_shadow) {
                for (int j = 0; j < num_triangles; j++) {
                    if (hit_triangle(shadow_origin, L, tri_v0[j], tri_v1[j], tri_v2[j],
                                   tri_normals[j], t_shadow)) {
                        if (t_shadow < light_dist) {
                            in_shadow = true;
                            break;
                        }
                    }
                }
            }

            shadow = in_shadow ? 0.0 : 1.0;
        }

        // Cook-Torrance BRDF
        vec3 brdf = cook_torrance_brdf(N, V, L, H, albedo, roughness, metallic);

        // Light attenuation (quadratic falloff)
        float distance = length(light_pos - hit_point);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

        // Add light contribution
        float NdotL = max(dot(N, L), 0.0);
        Lo += brdf * light_color * light_intensity * attenuation * NdotL * shadow;
    }

    // Add Image-Based Lighting approximation (ambient)
    vec3 R = reflect(-V, N);

    // Sample environment gradient
    float env_dot = max(R.y, 0.0);
    vec3 env_color = mix(vec3(0.1), vec3(0.5, 0.7, 1.0), env_dot);

    // Roughness-based blur approximation
    float roughness2 = roughness * roughness;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F_Schlick(max(dot(N, V), 0.0), F0);

    // Specular IBL
    vec3 specular_ibl = env_color * (roughness2 * F) * 0.3;

    // Diffuse IBL (irradiance approximation)
    float irradiance = max(N.y, 0.0) * 0.5 + 0.5;
    vec3 diffuse_ibl = albedo * irradiance * 0.15;

    vec3 ibl = diffuse_ibl + specular_ibl;

    // Apply ambient occlusion if enabled
    float ao = 1.0;
    if (enable_ao && ao_samples > 0) {
        ao = calculate_ao(hit_point, N);
    }
    ibl *= ao;

    return Lo + ibl;
}

// Gamma correction
vec3 gamma_correct(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

// ========== PHASE 4: POST-PROCESSING FUNCTIONS ==========

// ACES tone mapping operator
vec3 aces_tonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard tone mapping operator
vec3 reinhard_tonemap(vec3 color) {
    vec3 result = color / (color + vec3(1.0));
    return result;
}

// Filmic tone mapping operator (optimized Uncharted 2)
vec3 filmic_tonemap(vec3 color) {
    vec3 x = max(vec3(0.0), color - 0.004);
    vec3 result = (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
    return result / (result / vec3(11.2) + vec3(1.0));  // White balance
}

// Uncharted 2 tone mapping operator
vec3 uncharted2_tonemap(vec3 color) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;

    vec3 result = ((color * (A * color + C) * B + D * E) / (color * (A * color + B) + D * F)) - E / F;

    vec3 white = ((vec3(W) * (A * vec3(W) + C) * B + D * E) / (vec3(W) * (A * vec3(W) + B) + D * F)) - E / F;

    return result / white;
}

// Screen-Space Ambient Occlusion (simplified, depth-based approximation)
float calculate_ssao(vec3 pos, vec3 normal, vec2 uv) {
    if (!enable_ssao || ssao_samples <= 0) return 1.0;

    float occlusion = 0.0;
    int max_samples = 32;
    int actual_samples = ssao_samples;
    if (actual_samples > max_samples) actual_samples = max_samples;

    for (int i = 0; i < actual_samples; i++) {
        // Simple hemisphere sampling
        float fi = float(i);
        float ftotal = float(actual_samples);

        // Create sample direction
        float angle1 = fi * 6.28318 / ftotal;
        float angle2 = fi * 3.14159 / ftotal;

        vec3 sample_dir_local = normalize(vec3(
            cos(angle1) * sin(angle2),
            sin(angle1) * sin(angle2),
            cos(angle2)
        ));

        sample_dir_local = normalize(sample_dir_local + normal * 0.5);
        vec3 sample_pos_local = pos + sample_dir_local * ssao_radius;

        // Check occlusion
        float t_min_local = ssao_radius;
        bool hit_local = false;

        for (int j = 0; j < num_spheres; j++) {
            if (hit_sphere(sample_pos_local, -sample_dir_local, sphere_centers[j], sphere_radii[j], t_min_local)) {
                hit_local = true;
                break;
            }
        }

        if (hit_local) {
            occlusion += 1.0;
        }
    }

    float ftotal = float(actual_samples);
    occlusion /= ftotal;
    return 1.0 - occlusion * ssao_intensity;
}

// Apply tone mapping based on selected operator
vec3 apply_tone_mapping(vec3 color) {
    // Apply exposure compensation
    color *= exposure;

    // Apply selected tone mapping operator
    vec3 result;
    if (tone_mapping_op == 0) {
        // None
        result = color;
    } else if (tone_mapping_op == 1) {
        // ACES
        result = aces_tonemap(color);
    } else if (tone_mapping_op == 2) {
        // Reinhard
        result = reinhard_tonemap(color);
    } else if (tone_mapping_op == 3) {
        // Filmic
        result = filmic_tonemap(color);
    } else if (tone_mapping_op == 4) {
        // Uncharted 2
        result = uncharted2_tonemap(color);
    } else {
        // Default to ACES
        result = aces_tonemap(color);
    }
    return result;
}

// Cinematic vignette effect
vec3 apply_vignette(vec3 color, vec2 uv) {
    if (!enable_vignette) return color;

    vec2 center = vec2(0.5, 0.5);
    float dist = distance(uv, center);
    float vignette = smoothstep(vignette_falloff, 0.0, dist);

    return color * mix(1.0 - vignette_intensity, 1.0, vignette);
}

// Film grain effect
vec3 apply_film_grain(vec3 color, vec2 uv) {
    if (!enable_film_grain) return color;

    float grain = fract(sin(dot(uv * grain_size, vec2(12.9898, 78.233))) * 43758.5453);
    grain = grain * 2.0 - 1.0;  // Remap to [-1, 1]

    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
    vec3 grain_color = vec3(grain * grain_intensity * (0.5 + luminance * 0.5));

    return color + grain_color;
}

// Bloom/glow effect (simplified single-pass approximation)
vec3 apply_bloom(vec3 color) {
    if (!enable_bloom) return color;

    // Calculate luminance
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));

    // Threshold for bloom
    if (luminance > bloom_threshold) {
        float excess = luminance - bloom_threshold;
        vec3 bloom_color = color * (excess / luminance) * bloom_intensity;
        return color + bloom_color;
    }

    return color;
}

// Color grading (lift/gamma/gain approximation)
vec3 apply_color_grading(vec3 color) {
    // Contrast adjustment
    color = (color - vec3(0.5)) * contrast + vec3(0.5);

    // Saturation adjustment
    float gray = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(gray), color, saturation);

    return clamp(color, vec3(0.0), vec3(1.0));
}

// DOF and motion blur are now handled in main() via multi-ray sampling.
// These stubs are kept for API compatibility.
vec3 apply_depth_of_field(vec3 color, vec2 uv) { return color; }
vec3 apply_motion_blur(vec3 color, vec2 uv) { return color; }

// Phase 5: Lens Flares (dramatic light effects)
vec3 apply_lens_flares(vec3 color, vec2 uv) {
    if (!enable_lens_flares || lens_flare_intensity <= 0.0) {
        return color;  // No lens flares
    }

    // Calculate direction from light position
    vec2 delta = uv - light_position;
    float dist = length(delta);
    vec2 light_dir = normalize(delta);

    // Multiple flare types
    vec3 flare_color = vec3(0.0);

    // 1. Main halo (bright circle around light)
    float halo = 1.0 / (dist * 2.0 + 0.1);
    halo = clamp(halo, 0.0, 1.0);
    flare_color += vec3(1.0, 0.9, 0.7) * halo * 0.3;

    // 2. Streak flare (horizontal/vertical streaks)
    float streak = abs(light_dir.x) + abs(light_dir.y);
    streak = pow(1.0 - streak, 8.0);
    flare_color += vec3(1.0, 0.8, 0.6) * streak * 0.2;

    // 3. Ghost flares (reflected circles)
    for (int i = 1; i <= 3; i++) {
        float ghost_dist = dist - float(i) * 0.15;
        float ghost = exp(-ghost_dist * ghost_dist * 50.0);
        vec2 ghost_pos = uv - light_dir * float(i) * 0.15;
        flare_color += vec3(0.8, 0.6, 0.4) * ghost * 0.15 / float(i);
    }

    // Apply intensity and blend
    return color + flare_color * lens_flare_intensity * 0.5;
}

// TAA is now handled in main() using the history_buffer texture sampler.
// This stub is kept for API compatibility.
vec3 apply_taa(vec3 color, vec2 uv) { return color; }

// Full post-processing pipeline.
// DOF, motion blur, chromatic aberration, and TAA are applied in main() before
// this function, since they require re-tracing rays or reading the history texture.
vec3 apply_post_processing(vec3 color, vec2 uv) {
    // Bloom (brightens highlights)
    color = apply_bloom(color);

    // Tone mapping + exposure
    color = apply_tone_mapping(color);

    // Color grading (contrast, saturation)
    color = apply_color_grading(color);

    // Gamma correction
    color = gamma_correct(color);

    // Lens flares (additive, after tone mapping so they don't blow out)
    color = apply_lens_flares(color, uv);

    // Vignette
    color = apply_vignette(color, uv);

    // Film grain
    color = apply_film_grain(color, uv);

    return clamp(color, vec3(0.0), vec3(1.0));
}

// Full color pipeline (Phase 4: Advanced post-processing)
vec3 color_pipeline(vec3 color, vec2 uv) {
    // Phase 4: Apply advanced post-processing pipeline
    return apply_post_processing(color, uv);
}

// Legacy color pipeline (for backwards compatibility)
vec3 color_pipeline_legacy(vec3 color) {
    // Exposure
    float exposure = 1.0;
    color *= exposure;

    // Tone map
    color = aces_tonemap(color);

    // Gamma correct
    color = gamma_correct(color);

    return color;
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
            vec3 albedo;
            vec3 normal;
            int material;
            float roughness = 0.5;   // Default roughness
            float metallic = 0.0;     // Default metallic
            bool apply_lighting = true;

            if (hit_type == 0) {
                // Sphere hit
                normal = normalize(hit_point - sphere_centers[hit_object]);
                albedo = sphere_colors[hit_object];
                material = sphere_materials[hit_object];
                roughness = sphere_roughness[hit_object];
                metallic = sphere_metallic[hit_object];

                // Apply procedural textures based on material
                if (material == 4) {
                    // Checkerboard (red and blue)
                    albedo = checkerboard_texture(hit_point, vec3(0.8, 0.2, 0.2), vec3(0.2, 0.2, 0.8), 8.0);
                } else if (material == 5) {
                    // Noise (black and white)
                    albedo = noise_texture(hit_point, vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), 5.0);
                } else if (material == 6) {
                    // Gradient (purple to yellow vertical)
                    float gradient_t = (hit_point.y + 2.3) / 1.6;
                    gradient_t = clamp(gradient_t, 0.0, 1.0);
                    albedo = mix(vec3(0.6, 0.2, 0.8), vec3(0.9, 0.9, 0.2), gradient_t);
                    apply_lighting = false;  // Self-illuminated
                } else if (material == 7) {
                    // Stripe (orange and white horizontal)
                    albedo = stripe_texture(hit_point, vec3(0.8, 0.5, 0.2), vec3(0.9, 0.9, 0.9), 8.0);
                }
            } else {
                // Triangle hit
                normal = tri_normals[hit_object];
                albedo = tri_colors[hit_object];
                material = tri_materials[hit_object];
                roughness = tri_roughness[hit_object];
                metallic = tri_metallic[hit_object];

                // Apply procedural textures for triangles
                if (material == 8) {
                    // Pyramid checkerboard
                    albedo = checkerboard_texture(hit_point, vec3(0.1, 0.1, 0.1), vec3(0.9, 0.9, 0.9), 6.0);
                } else if (material == 9) {
                    // Gradient quad (red to blue along Z)
                    albedo = gradient_texture(hit_point, vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0),
                                             tri_gradient_scale[hit_object], tri_gradient_offset[hit_object]);
                    apply_lighting = false;  // Self-illuminated
                }
            }

            // Calculate lighting (PBR or Phong)
            vec3 color;
            if (apply_lighting) {
                vec3 V = normalize(-current_direction);

                if (lighting_mode == 1) {
                    // PBR lighting
                    color = calculate_pbr_lighting(hit_point, normal, V, albedo, roughness, metallic);
                } else {
                    // Legacy Phong shading (for comparison)
                    if (num_lights > 0) {
                        vec3 L = normalize(light_positions[0] - hit_point);
                        color = phong_shading(normal, V, L, albedo, 32.0);
                    } else {
                        // Fallback to single light
                        vec3 light_pos = vec3(0.0, 18.0, 0.0);
                        vec3 L = normalize(light_pos - hit_point);
                        color = phong_shading(normal, V, L, albedo, 32.0);
                    }
                }

                // Add Global Illumination (indirect lighting)
                if (enable_gi && gi_samples > 0) {
                    vec3 gi_color = calculate_gi(hit_point, normal, albedo);
                    color += gi_color;
                }

                // Add Screen-Space Reflections
                if (enable_ssr && ssr_samples > 0) {
                    vec3 ssr_color = calculate_ssr(hit_point, normal, V, albedo, roughness, metallic);
                    color += ssr_color;
                }

                // Add Environment Mapping
                if (enable_env_mapping) {
                    vec3 env_color = calculate_env_mapping(hit_point, normal, V, albedo, roughness, metallic);
                    color += env_color;
                }
            } else {
                color = albedo;  // Self-illuminated
            }

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

    // Build camera basis from uniforms
    vec3 origin = camera_pos;
    vec3 w = normalize(origin - camera_lookat);
    vec3 u = normalize(cross(camera_vup, w));
    vec3 v = cross(w, u);

    float aspect_ratio = resolution.x / resolution.y;
    float h = tan(camera_vfov * 0.0174533 * 0.5);
    float viewport_height = 2.0 * h;
    float viewport_width  = aspect_ratio * viewport_height;
    vec3 horizontal = viewport_width * u;
    vec3 vertical   = viewport_height * v;
    vec3 lower_left_corner = origin - horizontal * 0.5 - vertical * 0.5 - w;

    // Base ray direction for this pixel
    vec3 direction = lower_left_corner + uv.x * horizontal + uv.y * vertical - origin;

    vec3 color;

    // --- Depth of Field ---
    // Traces 5 rays from different lens positions; all converge at the focus plane.
    if (enable_dof && dof_aperture > 0.001) {
        vec3 focus_point = origin + normalize(direction) * dof_focus_distance;
        float r = dof_aperture;
        vec3 o1 = origin + u * r;
        vec3 o2 = origin - u * r;
        vec3 o3 = origin + v * r;
        vec3 o4 = origin - v * r;
        color  = ray_color(origin, direction);
        color += ray_color(o1, normalize(focus_point - o1));
        color += ray_color(o2, normalize(focus_point - o2));
        color += ray_color(o3, normalize(focus_point - o3));
        color += ray_color(o4, normalize(focus_point - o4));
        color /= 5.0;
    }
    // --- Motion Blur ---
    // Traces 5 rays with camera origin stepped along the motion vector,
    // simulating exposure during camera movement.
    else if (enable_motion_blur && motion_blur_strength > 0.001 && length(motion_vector) > 0.001) {
        float str = motion_blur_strength * 0.25;
        vec3 mv = vec3(motion_vector.x, 0.0, motion_vector.y) * str;
        vec3 c = vec3(0.0);
        vec3 o0 = origin - mv * 0.5;
        vec3 o1 = origin - mv * 0.25;
        vec3 o2 = origin;
        vec3 o3 = origin + mv * 0.25;
        vec3 o4 = origin + mv * 0.5;
        c += ray_color(o0, lower_left_corner + uv.x*horizontal + uv.y*vertical - o0);
        c += ray_color(o1, lower_left_corner + uv.x*horizontal + uv.y*vertical - o1);
        c += ray_color(o2, direction);
        c += ray_color(o3, lower_left_corner + uv.x*horizontal + uv.y*vertical - o3);
        c += ray_color(o4, lower_left_corner + uv.x*horizontal + uv.y*vertical - o4);
        color = c / 5.0;
    }
    else {
        color = ray_color(origin, direction);
    }

    // --- Chromatic Aberration ---
    // Re-traces the red and blue channels with slightly offset ray directions,
    // simulating lens dispersion (different wavelengths bend differently).
    if (enable_chromatic_aberration && chromatic_aberration_strength > 0.001) {
        vec2 uv_offset = uv - vec2(0.5);
        float dist = length(uv_offset);
        if (dist > 0.01) {
            float str = chromatic_aberration_strength * 0.006 * dist;
            vec2 shift = normalize(uv_offset) * str;

            vec2 uv_r = clamp(uv + shift, vec2(0.001), vec2(0.999));
            vec3 dir_r = lower_left_corner + uv_r.x*horizontal + uv_r.y*vertical - origin;
            color.r = ray_color(origin, dir_r).r;

            vec2 uv_b = clamp(uv - shift, vec2(0.001), vec2(0.999));
            vec3 dir_b = lower_left_corner + uv_b.x*horizontal + uv_b.y*vertical - origin;
            color.b = ray_color(origin, dir_b).b;
        }
    }

    // --- Temporal Anti-Aliasing ---
    // Blends the current frame with the history buffer to smooth aliasing over time.
    // The history texture is updated by the CPU after each frame.
    if (enable_taa && taa_mix_factor > 0.0) {
        vec3 history = texture2D(history_buffer, uv).rgb;
        // Clamp history to neighborhood of current to avoid ghosting
        vec3 color_min = color * 0.85;
        vec3 color_max = color * 1.15 + vec3(0.05);
        history = clamp(history, color_min, color_max);
        color = mix(color, history, taa_mix_factor);
    }

    // Post-processing: bloom, tone mapping, color grading, gamma, vignette, grain
    color = color_pipeline(color, uv);

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

// Overload for std::string
GLuint compile_shader(GLenum type, const std::string& source) {
    return compile_shader(type, source.c_str());
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
    std::vector<TriangleData>& triangles,
    const std::string& scene_name = SCENE_NAME
) {
    // Scene selection based on SCENE_NAME macro or runtime parameter
    Scene scene;

    std::cout << "=== Loading Scene: " << scene_name << " ===" << std::endl;

    if (scene_name == "gpu_demo") {
        setup_gpu_demo_scene(scene);
        std::cout << "✓ GPU Demo scene loaded" << std::endl;
    } else if (scene_name == "pbr_showcase") {
        setup_pbr_showcase_scene(scene);
        std::cout << "✓ PBR Showcase scene loaded" << std::endl;
    } else if (scene_name == "dof_showcase") {
        setup_dof_showcase_scene(scene);
        std::cout << "✓ DOF Showcase scene loaded" << std::endl;
    } else if (scene_name == "fx_showcase") {
        setup_fx_showcase_scene(scene);
        std::cout << "✓ FX Showcase scene loaded" << std::endl;
    } else if (scene_name == "cornell_box") {
        setup_cornell_box_scene(scene);
        std::cout << "✓ Cornell Box scene loaded" << std::endl;
    } else {
        std::cout << "Unknown scene: " << scene_name << ", using Cornell Box" << std::endl;
        setup_cornell_box_scene(scene);
    }

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

            // Determine material type and PBR parameters
            std::cout << "Sphere material type: ";
            if (auto metal = std::dynamic_pointer_cast<Metal>(sphere->mat)) {
                if (metal->fuzz < 0.1) {
                    data.material = 1; // Perfect metal
                    data.roughness = 0.1f;   // Polished metal
                    data.metallic = 1.0f;    // Fully metallic
                    std::cout << "Perfect metal" << std::endl;
                } else {
                    data.material = 2; // Fuzzy metal
                    data.roughness = 0.4f;   // Rougher metal
                    data.metallic = 1.0f;    // Fully metallic
                    std::cout << "Fuzzy metal" << std::endl;
                }
                data.gradient_scale = 0.25f;  // Default
                data.gradient_offset = 0.0f;  // Default
            } else if (auto glass = std::dynamic_pointer_cast<Dielectric>(sphere->mat)) {
                data.material = 3; // Glass
                data.roughness = 0.05f;  // Very smooth
                data.metallic = 0.0f;    // Dielectric
                std::cout << "Glass" << std::endl;
                data.gradient_scale = 0.25f;  // Default
                data.gradient_offset = 0.0f;  // Default
            } else if (auto lambertian = std::dynamic_pointer_cast<Lambertian>(sphere->mat)) {
                std::cout << "Lambertian, checking texture..." << std::endl;
                // Check if this is a procedural texture material
                if (auto checker = std::dynamic_pointer_cast<CheckerTexture>(lambertian->albedo_texture)) {
                    data.material = 4; // Checkerboard
                    data.roughness = 0.8f;   // Matte plastic
                    data.metallic = 0.0f;    // Dielectric
                    data.gradient_scale = 0.25f;  // Default
                    data.gradient_offset = 0.0f;  // Default
                    std::cout << "  -> Checkerboard texture" << std::endl;
                } else if (auto noise = std::dynamic_pointer_cast<NoiseTexture>(lambertian->albedo_texture)) {
                    data.material = 5; // Noise
                    data.roughness = 0.9f;   // Very rough
                    data.metallic = 0.0f;    // Dielectric
                    data.gradient_scale = 0.25f;  // Default
                    data.gradient_offset = 0.0f;  // Default
                    std::cout << "  -> Noise texture" << std::endl;
                } else if (auto gradient = std::dynamic_pointer_cast<GradientTexture>(lambertian->albedo_texture)) {
                    data.material = 6; // Gradient
                    data.roughness = 0.5f;   // Medium roughness
                    data.metallic = 0.0f;    // Dielectric
                    data.gradient_scale = gradient->scale;
                    data.gradient_offset = gradient->offset;
                    std::cout << "  -> Gradient texture! scale=" << gradient->scale << " offset=" << gradient->offset << std::endl;
                } else if (auto stripe = std::dynamic_pointer_cast<StripeTexture>(lambertian->albedo_texture)) {
                    data.material = 7; // Stripe
                    data.roughness = 0.7f;   // Matte
                    data.metallic = 0.0f;    // Dielectric
                    data.gradient_scale = 0.25f;  // Default
                    data.gradient_offset = 0.0f;  // Default
                    std::cout << "  -> Stripe texture" << std::endl;
                } else {
                    data.material = 0; // Regular lambertian
                    data.roughness = 0.8f;   // Matte wall/floor
                    data.metallic = 0.0f;    // Dielectric
                    data.gradient_scale = 0.25f;  // Default
                    data.gradient_offset = 0.0f;  // Default
                    std::cout << "  -> Regular lambertian (unknown texture)" << std::endl;
                }
            } else {
                data.material = 0; // Default lambertian
                data.roughness = 0.8f;   // Matte
                data.metallic = 0.0f;    // Dielectric
                data.gradient_scale = 0.25f;  // Default
                data.gradient_offset = 0.0f;  // Default
                std::cout << "Unknown material type" << std::endl;
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
                    data.roughness = 0.6f;   // Medium matte
                    data.metallic = 0.0f;    // Dielectric
                    std::cout << "  -> Checkerboard texture" << std::endl;
                } else if (auto gradient = std::dynamic_pointer_cast<GradientTexture>(lambertian->albedo_texture)) {
                    data.material = 9; // Gradient (quad on right wall)
                    data.gradient_scale = gradient->scale;
                    data.gradient_offset = gradient->offset;
                    data.roughness = 0.5f;   // Medium roughness
                    data.metallic = 0.0f;    // Dielectric
                    std::cout << "  -> Gradient texture! scale=" << gradient->scale << " offset=" << gradient->offset << std::endl;
                } else {
                    data.material = 0; // Regular lambertian
                    data.roughness = 0.8f;   // Matte wall
                    data.metallic = 0.0f;    // Dielectric
                    data.gradient_scale = 0.25f;  // Default
                    data.gradient_offset = 0.0f;  // Default
                    std::cout << "  -> Regular lambertian (unknown texture)" << std::endl;
                }
            } else {
                data.material = 0; // Default lambertian
                data.roughness = 0.8f;   // Matte
                data.metallic = 0.0f;    // Dielectric
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
    // Parse command-line arguments for scene selection
    std::string scene_name = SCENE_NAME;  // Default from Makefile

    if (argc > 1) {
        scene_name = argv[1];
        std::cout << "Scene override from command line: " << scene_name << std::endl;
    }

    std::cout << "=== GPU Ray Tracer (GLSL 1.20 Compatible) ===" << std::endl;
    std::cout << "Build Features:" << std::endl;
    std::cout << "  PBR Lighting: " << (ENABLE_PBR ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  Multiple Lights: " << (ENABLE_MULTIPLE_LIGHTS ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  Tone Mapping: " << (ENABLE_TONE_MAPPING ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  Gamma Correction: " << (ENABLE_GAMMA_CORRECTION ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  Scene: " << scene_name << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create OpenGL window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "GPU Ray Tracer - 640x360 (resize with keyboard 1-4)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
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

    // PBR lighting uniforms
    GLint lighting_mode_loc = glGetUniformLocation(program, "lighting_mode");
    GLint num_lights_loc = glGetUniformLocation(program, "num_lights");
    GLint light_positions_loc[4];
    GLint light_colors_loc[4];
    GLint light_intensities_loc[4];

    for (int i = 0; i < 4; i++) {
        std::string name = "light_positions[" + std::to_string(i) + "]";
        light_positions_loc[i] = glGetUniformLocation(program, name.c_str());

        name = "light_colors[" + std::to_string(i) + "]";
        light_colors_loc[i] = glGetUniformLocation(program, name.c_str());

        name = "light_intensities[" + std::to_string(i) + "]";
        light_intensities_loc[i] = glGetUniformLocation(program, name.c_str());
    }

    // Phase 2: Advanced lighting uniforms
    GLint enable_soft_shadows_loc = glGetUniformLocation(program, "enable_soft_shadows");
    GLint soft_shadow_samples_loc = glGetUniformLocation(program, "soft_shadow_samples");
    GLint light_radius_loc = glGetUniformLocation(program, "light_radius");
    GLint enable_ao_loc = glGetUniformLocation(program, "enable_ao");
    GLint ao_samples_loc = glGetUniformLocation(program, "ao_samples");
    GLint enable_gi_loc = glGetUniformLocation(program, "enable_gi");
    GLint gi_samples_loc = glGetUniformLocation(program, "gi_samples");
    GLint gi_intensity_loc = glGetUniformLocation(program, "gi_intensity");

        // Phase 3.5: Advanced reflection uniform locations
    GLint enable_ssr_loc = glGetUniformLocation(program, "enable_ssr");
    GLint ssr_samples_loc = glGetUniformLocation(program, "ssr_samples");
    GLint ssr_step_size_loc = glGetUniformLocation(program, "ssr_step_size");
    GLint ssr_roughness_cutoff_loc = glGetUniformLocation(program, "ssr_roughness_cutoff");
    GLint enable_env_mapping_loc = glGetUniformLocation(program, "enable_env_mapping");
    GLint env_mip_levels_loc = glGetUniformLocation(program, "env_mip_levels");

    // Phase 4: Post-processing uniform locations
    GLint enable_ssao_loc = glGetUniformLocation(program, "enable_ssao");
    GLint ssao_samples_loc = glGetUniformLocation(program, "ssao_samples");
    GLint ssao_radius_loc = glGetUniformLocation(program, "ssao_radius");
    GLint ssao_intensity_loc = glGetUniformLocation(program, "ssao_intensity");

    GLint enable_bloom_loc = glGetUniformLocation(program, "enable_bloom");
    GLint bloom_threshold_loc = glGetUniformLocation(program, "bloom_threshold");
    GLint bloom_intensity_loc = glGetUniformLocation(program, "bloom_intensity");

    GLint enable_vignette_loc = glGetUniformLocation(program, "enable_vignette");
    GLint vignette_intensity_loc = glGetUniformLocation(program, "vignette_intensity");
    GLint vignette_falloff_loc = glGetUniformLocation(program, "vignette_falloff");

    GLint enable_film_grain_loc = glGetUniformLocation(program, "enable_film_grain");
    GLint grain_intensity_loc = glGetUniformLocation(program, "grain_intensity");
    GLint grain_size_loc = glGetUniformLocation(program, "grain_size");

    // Phase 5: Chromatic Aberration
    GLint enable_chromatic_aberration_loc = glGetUniformLocation(program, "enable_chromatic_aberration");
    GLint chromatic_aberration_strength_loc = glGetUniformLocation(program, "chromatic_aberration_strength");

    // Phase 5: Depth of Field
    GLint enable_dof_loc = glGetUniformLocation(program, "enable_dof");
    GLint dof_focus_distance_loc = glGetUniformLocation(program, "dof_focus_distance");
    GLint dof_aperture_loc = glGetUniformLocation(program, "dof_aperture");

    // Phase 5: Motion Blur
    GLint enable_motion_blur_loc = glGetUniformLocation(program, "enable_motion_blur");
    GLint motion_blur_strength_loc = glGetUniformLocation(program, "motion_blur_strength");
    GLint motion_vector_loc = glGetUniformLocation(program, "motion_vector");

    // Phase 5: Adaptive Quality
    GLint enable_adaptive_quality_loc = glGetUniformLocation(program, "enable_adaptive_quality");
    GLint quality_scale_loc = glGetUniformLocation(program, "quality_scale");

    // Phase 5: Lens Flares
    GLint enable_lens_flares_loc = glGetUniformLocation(program, "enable_lens_flares");
    GLint lens_flare_intensity_loc = glGetUniformLocation(program, "lens_flare_intensity");
    GLint light_position_loc = glGetUniformLocation(program, "light_position");

    // Phase 6: Temporal Anti-Aliasing
    GLint enable_taa_loc = glGetUniformLocation(program, "enable_taa");
    GLint taa_mix_factor_loc = glGetUniformLocation(program, "taa_mix_factor");
    GLint camera_velocity_loc = glGetUniformLocation(program, "camera_velocity");
    GLint history_buffer_loc = glGetUniformLocation(program, "history_buffer");

    // Create history texture for TAA (ping-pong via glCopyTexImage2D)
    GLuint history_texture = 0;
    glGenTextures(1, &history_texture);
    glBindTexture(GL_TEXTURE_2D, history_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    bool taa_history_ready = false;

    GLint tone_mapping_op_loc = glGetUniformLocation(program, "tone_mapping_op");
    GLint exposure_loc = glGetUniformLocation(program, "exposure");
    GLint contrast_loc = glGetUniformLocation(program, "contrast");
    GLint saturation_loc = glGetUniformLocation(program, "saturation");

    // Cache per-element uniform locations once at startup.
    // Calling glGetUniformLocation inside the render loop (per sphere/triangle) is
    // extremely slow on Intel iGPU drivers — it was the main cause of 10s frame times.
    const int MAX_SPHERES = 32;
    const int MAX_TRIS    = 20;
    GLint sp_center_loc[MAX_SPHERES], sp_radius_loc[MAX_SPHERES];
    GLint sp_color_loc[MAX_SPHERES],  sp_mat_loc[MAX_SPHERES];
    GLint sp_gscale_loc[MAX_SPHERES], sp_goffset_loc[MAX_SPHERES];
    GLint sp_rough_loc[MAX_SPHERES],  sp_metal_loc[MAX_SPHERES];
    for (int i = 0; i < MAX_SPHERES; i++) {
        std::string idx = "[" + std::to_string(i) + "]";
        sp_center_loc[i]  = glGetUniformLocation(program, ("sphere_centers"       + idx).c_str());
        sp_radius_loc[i]  = glGetUniformLocation(program, ("sphere_radii"         + idx).c_str());
        sp_color_loc[i]   = glGetUniformLocation(program, ("sphere_colors"        + idx).c_str());
        sp_mat_loc[i]     = glGetUniformLocation(program, ("sphere_materials"     + idx).c_str());
        sp_gscale_loc[i]  = glGetUniformLocation(program, ("sphere_gradient_scale"+ idx).c_str());
        sp_goffset_loc[i] = glGetUniformLocation(program, ("sphere_gradient_offset"+idx).c_str());
        sp_rough_loc[i]   = glGetUniformLocation(program, ("sphere_roughness"     + idx).c_str());
        sp_metal_loc[i]   = glGetUniformLocation(program, ("sphere_metallic"      + idx).c_str());
    }
    GLint tr_v0_loc[MAX_TRIS], tr_v1_loc[MAX_TRIS], tr_v2_loc[MAX_TRIS];
    GLint tr_norm_loc[MAX_TRIS], tr_color_loc[MAX_TRIS], tr_mat_loc[MAX_TRIS];
    GLint tr_gscale_loc[MAX_TRIS], tr_goffset_loc[MAX_TRIS];
    GLint tr_rough_loc[MAX_TRIS], tr_metal_loc[MAX_TRIS];
    for (int i = 0; i < MAX_TRIS; i++) {
        std::string idx = "[" + std::to_string(i) + "]";
        tr_v0_loc[i]      = glGetUniformLocation(program, ("tri_v0"              + idx).c_str());
        tr_v1_loc[i]      = glGetUniformLocation(program, ("tri_v1"              + idx).c_str());
        tr_v2_loc[i]      = glGetUniformLocation(program, ("tri_v2"              + idx).c_str());
        tr_norm_loc[i]    = glGetUniformLocation(program, ("tri_normals"         + idx).c_str());
        tr_color_loc[i]   = glGetUniformLocation(program, ("tri_colors"          + idx).c_str());
        tr_mat_loc[i]     = glGetUniformLocation(program, ("tri_materials"       + idx).c_str());
        tr_gscale_loc[i]  = glGetUniformLocation(program, ("tri_gradient_scale"  + idx).c_str());
        tr_goffset_loc[i] = glGetUniformLocation(program, ("tri_gradient_offset" + idx).c_str());
        tr_rough_loc[i]   = glGetUniformLocation(program, ("tri_roughness"       + idx).c_str());
        tr_metal_loc[i]   = glGetUniformLocation(program, ("tri_metallic"        + idx).c_str());
    }

    // Rendering settings
    bool enable_reflections = true;
    int max_depth = 3;  // Keep low for interactive speed on integrated GPU (each bounce is expensive)

    // PBR lighting settings
    int lighting_mode = 0;  // Start with Phong for compatibility
    int num_lights = 1;     // Start with single light

    // Light setup (3-point lighting)
    float light_positions[4][3] = {
        {0.0f, 18.0f, 0.0f},    // Main light (overhead)
        {-10.0f, 10.0f, 10.0f}, // Fill light
        {15.0f, 5.0f, -5.0f},   // Rim light
        {0.0f, 0.0f, 0.0f}      // Spare
    };

    float light_colors[4][3] = {
        {1.0f, 1.0f, 1.0f},     // White main
        {0.8f, 0.9f, 1.0f},     // Cool fill
        {1.0f, 0.9f, 0.8f},     // Warm rim
        {1.0f, 1.0f, 1.0f}      // White spare
    };

    float light_intensities[4] = {
        1.0f,    // Main light
        0.3f,    // Fill light
        0.5f,    // Rim light
        0.0f     // Spare (off)
    };

    // Phase 2: Advanced lighting settings
    bool enable_soft_shadows = false;  // Disabled by default (performance)
    int soft_shadow_samples = 2;       // 2x2 = 4 samples per light
    float light_radius = 2.0;          // Radius of area light

    bool enable_ao = false;            // Disabled by default (performance)
    int ao_samples = 8;                // AO hemisphere samples

    // Phase 3: Global Illumination settings
    bool enable_gi = ENABLE_GI ? true : false;  // From Makefile
    int gi_samples = GI_SAMPLES;                // From Makefile
    float gi_intensity = GI_INTENSITY;          // From Makefile

        // Phase 3.5: Advanced reflection settings
    bool enable_ssr = ENABLE_SSR ? true : false;           // Screen-space reflections
    int ssr_samples = SSR_SAMPLES;                       // SSR ray samples
    float ssr_step_size = SSR_STEP_SIZE;                 // SSR step size
    float ssr_roughness_cutoff = 0.5f;                     // Roughness cutoff for SSR
    bool enable_env_mapping = ENABLE_ENV_MAPPING ? true : false;  // Environment mapping
    int env_mip_levels = 6;                               // Environment map quality

    // Phase 4: Post-processing settings
    bool enable_ssao = ENABLE_SSAO ? true : false;       // Screen-space ambient occlusion
    int ssao_samples = 16;                               // SSAO samples
    float ssao_radius = 0.5f;                            // SSAO sample radius
    float ssao_intensity = 1.0f;                         // SSAO intensity

    bool enable_bloom = ENABLE_BLOOM ? true : false;     // Bloom/glow effect
    float bloom_threshold = 0.8f;                         // Bloom brightness threshold
    float bloom_intensity = 0.3f;                         // Bloom intensity

    bool enable_vignette = ENABLE_VIGNETTE ? true : false;  // Cinematic vignette
    float vignette_intensity = 0.3f;                      // Vignette intensity
    float vignette_falloff = 0.5f;                        // Vignette falloff

    bool enable_film_grain = ENABLE_FILM_GRAIN ? true : false;  // Film grain effect
    float grain_intensity = 0.1f;                         // Film grain intensity
    float grain_size = 2.0f;                              // Film grain size

    // Phase 5: Chromatic Aberration
    bool enable_chromatic_aberration = ENABLE_CHROMATIC_ABERRATION ? true : false;
    float chromatic_aberration_strength = CHROMATIC_ABERRATION_STRENGTH;  // Chromatic aberration strength

    // Phase 5: Depth of Field
    bool enable_dof = ENABLE_DOF ? true : false;
    float dof_focus_distance = DOF_FOCUS_DISTANCE;  // Focus distance
    float dof_aperture = DOF_APERTURE;              // Aperture size

    // Phase 5: Motion Blur
    bool enable_motion_blur = ENABLE_MOTION_BLUR ? true : false;
    float motion_blur_strength = MOTION_BLUR_STRENGTH;  // Motion blur strength
    float motion_vector_x = 0.0f;  // Current motion vector X
    float motion_vector_y = 0.0f;  // Current motion vector Y
    float previous_mouse_x = 0.0f;  // Previous mouse position X
    float previous_mouse_y = 0.0f;  // Previous mouse position Y

    // Phase 5: Adaptive Quality
    bool enable_adaptive_quality = ENABLE_ADAPTIVE_QUALITY ? true : false;
    float target_fps = TARGET_FPS;  // Target FPS
    float adaptive_aggressiveness = ADAPTIVE_QUALITY_AGGRESSIVENESS;  // Adjustment aggressiveness
    float quality_scale = 1.0f;  // Current quality scale (1.0 = normal)

    // FPS tracking for adaptive quality
    auto last_fps_update = std::chrono::steady_clock::now();
    float fps_smooth = 60.0f;  // Smoothed FPS
    int frame_count = 0;  // Frame counter for FPS calculation

    // Phase 5: Lens Flares
    bool enable_lens_flares = ENABLE_LENS_FLARES ? true : false;
    float lens_flare_intensity = LENS_FLARE_INTENSITY;  // Lens flare intensity
    float light_position_x = 0.5f;  // Light position X (screen space)
    float light_position_y = 0.5f;  // Light position Y (screen space)

    // Phase 6: Temporal Anti-Aliasing
    bool enable_taa = ENABLE_TAA ? true : false;
    float taa_mix_factor = TAA_MIX_FACTOR;  // Temporal blend factor
    float camera_velocity_x = 0.0f;  // Camera motion X
    float camera_velocity_y = 0.0f;  // Camera motion Y

    int tone_mapping_op = TONE_MAPPING_OPERATOR;          // Tone mapping operator
    float exposure = 1.0f;                                // Exposure compensation
    float contrast = 1.0f;                                // Contrast adjustment
    float saturation = 1.0f;                              // Saturation adjustment

    // Setup scene data
    std::vector<SphereData> spheres;
    std::vector<TriangleData> triangles;
    setup_scene_data(spheres, triangles, scene_name);

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
    std::cout << "  P           - Toggle Phong/PBR lighting" << std::endl;
    std::cout << "  L           - Cycle light configurations" << std::endl;
    std::cout << "  G           - Toggle Global Illumination" << std::endl;
    std::cout << "  [ / ]       - Adjust GI samples (1-8)" << std::endl;
    std::cout << "  - / =       - Adjust GI intensity" << std::endl;
    std::cout << "  Shift+S     - Toggle Screen-Space Reflections" << std::endl;
    std::cout << "  E           - Toggle Environment Mapping" << std::endl;
    std::cout << "  , / .       - Adjust SSR samples" << std::endl;
    std::cout << "  /           - Adjust SSR roughness cutoff" << std::endl;
    std::cout << "\nPhase 4 Controls:" << std::endl;
    std::cout << "  O           - Toggle SSAO (Screen-Space Ambient Occlusion)" << std::endl;
    std::cout << "  B           - Toggle Bloom/Glow" << std::endl;
    std::cout << "  V           - Toggle Vignette" << std::endl;
    std::cout << "  N           - Toggle Film Grain" << std::endl;
    std::cout << "  T           - Cycle Tone Mapping (None/ACES/Reinhard/Filmic/Uncharted)" << std::endl;
    std::cout << "  1 / 2       - Decrease/Increase Exposure" << std::endl;
    std::cout << "  3 / 4       - Decrease/Increase Contrast" << std::endl;
    std::cout << "  5 / 6       - Decrease/Increase Saturation" << std::endl;
    std::cout << "  H           - Help" << std::endl;
    std::cout << "  C           - Controls panel" << std::endl;
    std::cout << "  ESC         - Quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

    // Camera controller (slower movement)
    CameraController camera;
    float move_speed = 0.05f; // Slower speed

    // Phase 6: TAA Camera Tracking (after camera creation)
    float previous_camera_yaw = 0.0f;  // Previous camera yaw
    float previous_camera_pitch = 0.0f;  // Previous camera pitch

    // UI panels
    HelpOverlay help_overlay;
    help_overlay.init();

    ControlsPanel controls_panel;
    controls_panel.init();

    // Intel iGPU (and many others) does lazy shader compilation: the first
    // glDrawArrays triggers the actual GPU compilation which can take 5-10 seconds.
    // We run one warmup frame here so the main loop is smooth from frame 1.
    // The window title warns the user while the compile runs.
    SDL_SetWindowTitle(window, "GPU Ray Tracer - Compiling shader (one-time, ~5-10s)...");
    std::cout << "\nWarming up GPU shader (one-time compilation, ~5-10 seconds)..." << std::endl;
    {
        glUseProgram(program);
        glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
        glUniform3f(camera_pos_loc, 0.0f, 2.0f, 15.0f);
        glUniform3f(camera_lookat_loc, 0.0f, 0.0f, 0.0f);
        glUniform3f(camera_vup_loc, 0.0f, 1.0f, 0.0f);
        glUniform1f(camera_vfov_loc, 60.0f);
        glUniform1i(num_spheres_loc, (int)spheres.size());
        glUniform1i(num_triangles_loc, (int)triangles.size());
        glUniform1i(enable_reflections_loc, 0);
        glUniform1i(max_depth_loc, 0);
        glUniform1i(lighting_mode_loc, 0);
        glUniform1i(num_lights_loc, 1);
        glUniform3f(light_positions_loc[0], 0.0f, 18.0f, 0.0f);
        glUniform3f(light_colors_loc[0], 1.0f, 1.0f, 1.0f);
        glUniform1f(light_intensities_loc[0], 1.0f);
        glUniform1i(enable_soft_shadows_loc, 0);
        glUniform1i(soft_shadow_samples_loc, 1);
        glUniform1f(light_radius_loc, 1.0f);
        glUniform1i(enable_ao_loc, 0);
        glUniform1i(ao_samples_loc, 0);
        glUniform1i(enable_gi_loc, 0);
        glUniform1i(gi_samples_loc, 0);
        glUniform1f(gi_intensity_loc, 0.0f);
        glUniform1i(enable_ssr_loc, 0);
        glUniform1i(ssr_samples_loc, 0);
        glUniform1f(ssr_step_size_loc, 0.1f);
        glUniform1f(ssr_roughness_cutoff_loc, 0.5f);
        glUniform1i(enable_env_mapping_loc, 0);
        glUniform1i(env_mip_levels_loc, 0);
        glUniform1i(enable_ssao_loc, 0);
        glUniform1i(ssao_samples_loc, 0);
        glUniform1f(ssao_radius_loc, 0.5f);
        glUniform1f(ssao_intensity_loc, 0.0f);
        glUniform1i(enable_bloom_loc, 0);
        glUniform1f(bloom_threshold_loc, 1.0f);
        glUniform1f(bloom_intensity_loc, 0.0f);
        glUniform1i(enable_vignette_loc, 0);
        glUniform1f(vignette_intensity_loc, 0.0f);
        glUniform1f(vignette_falloff_loc, 0.5f);
        glUniform1i(enable_film_grain_loc, 0);
        glUniform1f(grain_intensity_loc, 0.0f);
        glUniform1f(grain_size_loc, 1.0f);
        glUniform1i(enable_chromatic_aberration_loc, 0);
        glUniform1f(chromatic_aberration_strength_loc, 0.0f);
        glUniform1i(enable_dof_loc, 0);
        glUniform1f(dof_focus_distance_loc, 5.0f);
        glUniform1f(dof_aperture_loc, 0.0f);
        glUniform1i(enable_motion_blur_loc, 0);
        glUniform1f(motion_blur_strength_loc, 0.0f);
        glUniform2f(motion_vector_loc, 0.0f, 0.0f);
        glUniform1i(enable_adaptive_quality_loc, 0);
        glUniform1f(quality_scale_loc, 1.0f);
        glUniform1i(enable_lens_flares_loc, 0);
        glUniform1f(lens_flare_intensity_loc, 0.0f);
        glUniform2f(light_position_loc, 0.8f, 0.9f);
        glUniform1i(enable_taa_loc, 0);
        glUniform1f(taa_mix_factor_loc, 0.0f);
        glUniform2f(camera_velocity_loc, 0.0f, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, history_texture);
        glUniform1i(history_buffer_loc, 0);
        glUniform1i(tone_mapping_op_loc, 0);
        glUniform1f(exposure_loc, 1.0f);
        glUniform1f(contrast_loc, 1.0f);
        glUniform1f(saturation_loc, 1.0f);
        for (size_t i = 0; i < spheres.size() && i < (size_t)MAX_SPHERES; i++) {
            glUniform3f(sp_center_loc[i], spheres[i].center[0], spheres[i].center[1], spheres[i].center[2]);
            glUniform1f(sp_radius_loc[i], spheres[i].radius);
            glUniform3f(sp_color_loc[i],  spheres[i].color[0],  spheres[i].color[1],  spheres[i].color[2]);
            glUniform1i(sp_mat_loc[i],    spheres[i].material);
            glUniform1f(sp_gscale_loc[i], spheres[i].gradient_scale);
            glUniform1f(sp_goffset_loc[i],spheres[i].gradient_offset);
            glUniform1f(sp_rough_loc[i],  spheres[i].roughness);
            glUniform1f(sp_metal_loc[i],  spheres[i].metallic);
        }
        for (size_t i = 0; i < triangles.size() && i < (size_t)MAX_TRIS; i++) {
            glUniform3f(tr_v0_loc[i],   triangles[i].v0[0],    triangles[i].v0[1],    triangles[i].v0[2]);
            glUniform3f(tr_v1_loc[i],   triangles[i].v1[0],    triangles[i].v1[1],    triangles[i].v1[2]);
            glUniform3f(tr_v2_loc[i],   triangles[i].v2[0],    triangles[i].v2[1],    triangles[i].v2[2]);
            glUniform3f(tr_norm_loc[i], triangles[i].normal[0], triangles[i].normal[1], triangles[i].normal[2]);
            glUniform3f(tr_color_loc[i],triangles[i].color[0],  triangles[i].color[1],  triangles[i].color[2]);
            glUniform1i(tr_mat_loc[i],  triangles[i].material);
            glUniform1f(tr_gscale_loc[i], triangles[i].gradient_scale);
            glUniform1f(tr_goffset_loc[i],triangles[i].gradient_offset);
            glUniform1f(tr_rough_loc[i],  triangles[i].roughness);
            glUniform1f(tr_metal_loc[i],  triangles[i].metallic);
        }
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glFinish();  // block until compilation + draw complete
        SDL_GL_SwapWindow(window);
    }
    SDL_SetWindowTitle(window, "GPU Ray Tracer - Ready");
    std::cout << "Shader ready. Starting main loop." << std::endl;

    // Main loop
    bool running = true;
    bool need_render = true;
    SDL_Event event;

    auto start_time = std::chrono::high_resolution_clock::now();
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
                } else if (event.key.keysym.sym == SDLK_p) {
                    lighting_mode = 1 - lighting_mode;  // Toggle between 0 and 1
                    std::cout << "Lighting: " << (lighting_mode == 0 ? "Phong (Legacy)" : "PBR (Physically Based)") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_l) {
                    num_lights = (num_lights % 3) + 1;  // Cycle 1->2->3->1
                    std::cout << "Lights: " << num_lights << (num_lights == 1 ? " (Single)" : num_lights == 2 ? " (2-point)" : " (3-point)") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_g) {
                    enable_gi = !enable_gi;
                    std::cout << "Global Illumination: " << (enable_gi ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                    if (gi_samples < 8) {
                        gi_samples++;
                        std::cout << "GI Samples: " << gi_samples << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
                    if (gi_samples > 1) {
                        gi_samples--;
                        std::cout << "GI Samples: " << gi_samples << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_EQUALS) {
                    gi_intensity = std::min(1.0f, gi_intensity + 0.1f);
                    std::cout << "GI Intensity: " << gi_intensity << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_MINUS) {
                    gi_intensity = std::max(0.0f, gi_intensity - 0.1f);
                    std::cout << "GI Intensity: " << gi_intensity << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_s) {  // S key for SSR (shift+s to avoid screenshot conflict)
                    // Shift+S combo to avoid screenshot
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        enable_ssr = !enable_ssr;
                        std::cout << "Screen-Space Reflections: " << (enable_ssr ? "ON" : "OFF") << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_e) {  // E key for environment mapping
                    enable_env_mapping = !enable_env_mapping;
                    std::cout << "Environment Mapping: " << (enable_env_mapping ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_COMMA) {
                    ssr_samples = std::max(4, ssr_samples - 4);
                    std::cout << "SSR Samples: " << ssr_samples << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_PERIOD) {
                    ssr_samples = std::min(32, ssr_samples + 4);
                    std::cout << "SSR Samples: " << ssr_samples << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_SLASH) {
                    ssr_roughness_cutoff = std::min(1.0f, ssr_roughness_cutoff + 0.1f);
                    std::cout << "SSR Roughness Cutoff: " << ssr_roughness_cutoff << std::endl;
                    need_render = true;
                }
                // Phase 4: Post-processing controls
                else if (event.key.keysym.sym == SDLK_o) {  // O key for SSAO
                    enable_ssao = !enable_ssao;
                    std::cout << "SSAO: " << (enable_ssao ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_b) {  // B key for Bloom
                    enable_bloom = !enable_bloom;
                    std::cout << "Bloom: " << (enable_bloom ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_v) {  // V key for Vignette
                    enable_vignette = !enable_vignette;
                    std::cout << "Vignette: " << (enable_vignette ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_n) {  // N key for Film Grain
                    enable_film_grain = !enable_film_grain;
                    std::cout << "Film Grain: " << (enable_film_grain ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_c) {  // C key for Chromatic Aberration (Phase 5)
                    enable_chromatic_aberration = !enable_chromatic_aberration;
                    std::cout << "Chromatic Aberration: " << (enable_chromatic_aberration ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {  // [ key for CA strength down
                    if (chromatic_aberration_strength > 0.0f) {
                        chromatic_aberration_strength = std::max(0.0f, chromatic_aberration_strength - 0.1f);
                        std::cout << "Chromatic Aberration Strength: " << chromatic_aberration_strength << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {  // ] key for CA strength up
                    if (chromatic_aberration_strength < 2.0f) {
                        chromatic_aberration_strength = std::min(2.0f, chromatic_aberration_strength + 0.1f);
                        std::cout << "Chromatic Aberration Strength: " << chromatic_aberration_strength << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_f) {  // F key for Depth of Field (Phase 5)
                    enable_dof = !enable_dof;
                    std::cout << "Depth of Field: " << (enable_dof ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_MINUS) {  // - key for DOF focus distance down
                    if (dof_focus_distance > 0.5f) {
                        dof_focus_distance = std::max(0.5f, dof_focus_distance - 0.5f);
                        std::cout << "DOF Focus Distance: " << dof_focus_distance << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_EQUALS) {  // = key for DOF focus distance up
                    if (dof_focus_distance < 10.0f) {
                        dof_focus_distance = std::min(10.0f, dof_focus_distance + 0.5f);
                        std::cout << "DOF Focus Distance: " << dof_focus_distance << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_COMMA) {  // , key for DOF aperture down
                    if (dof_aperture > 0.01f) {
                        dof_aperture = std::max(0.01f, dof_aperture - 0.01f);
                        std::cout << "DOF Aperture: " << dof_aperture << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_PERIOD) {  // . key for DOF aperture up
                    if (dof_aperture < 0.5f) {
                        dof_aperture = std::min(0.5f, dof_aperture + 0.01f);
                        std::cout << "DOF Aperture: " << dof_aperture << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_m) {  // M key for Motion Blur (Phase 5)
                    enable_motion_blur = !enable_motion_blur;
                    std::cout << "Motion Blur: " << (enable_motion_blur ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_SEMICOLON) {  // ; key for motion blur strength down
                    if (motion_blur_strength > 0.0f) {
                        motion_blur_strength = std::max(0.0f, motion_blur_strength - 0.1f);
                        std::cout << "Motion Blur Strength: " << motion_blur_strength << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_QUOTE) {  // ' key for motion blur strength up
                    if (motion_blur_strength < 1.0f) {
                        motion_blur_strength = std::min(1.0f, motion_blur_strength + 0.1f);
                        std::cout << "Motion Blur Strength: " << motion_blur_strength << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_a) {  // A key for Adaptive Quality (Phase 5)
                    enable_adaptive_quality = !enable_adaptive_quality;
                    std::cout << "Adaptive Quality: " << (enable_adaptive_quality ? "ON" : "OFF") << std::endl;
                    if (enable_adaptive_quality) {
                        std::cout << "  Target FPS: " << target_fps << std::endl;
                        std::cout << "  Current Quality Scale: " << quality_scale << std::endl;
                    }
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {  // [ key for target FPS down
                    if (target_fps > 30.0f) {
                        target_fps = std::max(30.0f, target_fps - 15.0f);
                        std::cout << "Target FPS: " << target_fps << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {  // ] key for target FPS up
                    if (target_fps < 120.0f) {
                        target_fps = std::min(120.0f, target_fps + 15.0f);
                        std::cout << "Target FPS: " << target_fps << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_MINUS) {  // - key for aggressiveness down
                    if (adaptive_aggressiveness > 0.1f) {
                        adaptive_aggressiveness = std::max(0.1f, adaptive_aggressiveness - 0.1f);
                        std::cout << "Adaptive Aggressiveness: " << adaptive_aggressiveness << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_EQUALS) {  // = key for aggressiveness up
                    if (adaptive_aggressiveness < 1.0f) {
                        adaptive_aggressiveness = std::min(1.0f, adaptive_aggressiveness + 0.1f);
                        std::cout << "Adaptive Aggressiveness: " << adaptive_aggressiveness << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_l) {  // L key for Lens Flares (Phase 5)
                    enable_lens_flares = !enable_lens_flares;
                    std::cout << "Lens Flares: " << (enable_lens_flares ? "ON" : "OFF") << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_MINUS) {  // - key for flare intensity down
                    if (lens_flare_intensity > 0.0f) {
                        lens_flare_intensity = std::max(0.0f, lens_flare_intensity - 0.1f);
                        std::cout << "Lens Flare Intensity: " << lens_flare_intensity << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_EQUALS) {  // = key for flare intensity up
                    if (lens_flare_intensity < 2.0f) {
                        lens_flare_intensity = std::min(2.0f, lens_flare_intensity + 0.1f);
                        std::cout << "Lens Flare Intensity: " << lens_flare_intensity << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_r) {  // R key for TAA (Phase 6)
                    enable_taa = !enable_taa;
                    std::cout << "Temporal Anti-Aliasing: " << (enable_taa ? "ON" : "OFF") << std::endl;
                    if (enable_taa) {
                        std::cout << "  TAA Mix Factor: " << taa_mix_factor << std::endl;
                    }
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {  // [ key for TAA mix down
                    if (taa_mix_factor > 0.0f) {
                        taa_mix_factor = std::max(0.0f, taa_mix_factor - 0.1f);
                        std::cout << "TAA Mix Factor: " << taa_mix_factor << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {  // ] key for TAA mix up
                    if (taa_mix_factor < 1.0f) {
                        taa_mix_factor = std::min(1.0f, taa_mix_factor + 0.1f);
                        std::cout << "TAA Mix Factor: " << taa_mix_factor << std::endl;
                        need_render = true;
                    }
                } else if (event.key.keysym.sym == SDLK_t) {  // T key to cycle tone mapping
                    tone_mapping_op = (tone_mapping_op + 1) % 5;  // Cycle 0-4
                    const char* op_names[] = {"None", "ACES", "Reinhard", "Filmic", "Uncharted 2"};
                    std::cout << "Tone Mapping: " << op_names[tone_mapping_op] << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_1) {  // 1 key for exposure down
                    exposure = std::max(0.1f, exposure - 0.1f);
                    std::cout << "Exposure: " << exposure << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_2) {  // 2 key for exposure up
                    exposure = std::min(2.0f, exposure + 0.1f);
                    std::cout << "Exposure: " << exposure << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_3) {  // 3 key for contrast down
                    contrast = std::max(0.8f, contrast - 0.05f);
                    std::cout << "Contrast: " << contrast << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_4) {  // 4 key for contrast up
                    contrast = std::min(1.2f, contrast + 0.05f);
                    std::cout << "Contrast: " << contrast << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_5) {  // 5 key for saturation down
                    saturation = std::max(0.8f, saturation - 0.05f);
                    std::cout << "Saturation: " << saturation << std::endl;
                    need_render = true;
                } else if (event.key.keysym.sym == SDLK_6) {  // 6 key for saturation up
                    saturation = std::min(1.2f, saturation + 0.05f);
                    std::cout << "Saturation: " << saturation << std::endl;
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

        // Phase 5: Track motion for motion blur
        if (enable_motion_blur && need_render) {
            // Calculate motion based on camera movement (simplified)
            // In a real implementation, would track actual camera position changes
            float motion_decay = 0.8f;  // How fast motion fades
            motion_vector_x *= motion_decay;
            motion_vector_y *= motion_decay;

            // Add motion from WASD keys (simplified)
            if (keystates[SDL_SCANCODE_W]) motion_vector_y += move_speed * 0.1f;
            if (keystates[SDL_SCANCODE_S]) motion_vector_y -= move_speed * 0.1f;
            if (keystates[SDL_SCANCODE_A]) motion_vector_x -= move_speed * 0.1f;
            if (keystates[SDL_SCANCODE_D]) motion_vector_x += move_speed * 0.1f;
        }

        // Phase 6: Track camera velocity for TAA
        if (enable_taa && need_render) {
            // Calculate camera position change (using camera controller position)
            float current_cam_x = camera.position[0];
            float current_cam_y = camera.position[1];
            float current_cam_z = camera.position[2];

            // Simple approximation: use WASD state to estimate motion
            float pos_delta_x = 0.0f;
            float pos_delta_y = 0.0f;

            if (keystates[SDL_SCANCODE_W]) pos_delta_y = move_speed;
            if (keystates[SDL_SCANCODE_S]) pos_delta_y = -move_speed;
            if (keystates[SDL_SCANCODE_A]) pos_delta_x = -move_speed;
            if (keystates[SDL_SCANCODE_D]) pos_delta_x = move_speed;

            // Decay velocity over time
            camera_velocity_x = camera_velocity_x * 0.9f + pos_delta_x * 0.1f;
            camera_velocity_y = camera_velocity_y * 0.9f + pos_delta_y * 0.1f;
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

            // Set PBR lighting uniforms
            glUniform1i(lighting_mode_loc, lighting_mode);
            glUniform1i(num_lights_loc, num_lights);

            for (int i = 0; i < 4; i++) {
                glUniform3f(light_positions_loc[i], light_positions[i][0], light_positions[i][1], light_positions[i][2]);
                glUniform3f(light_colors_loc[i], light_colors[i][0], light_colors[i][1], light_colors[i][2]);
                glUniform1f(light_intensities_loc[i], light_intensities[i]);
            }

            // Set Phase 2 lighting uniforms
            glUniform1i(enable_soft_shadows_loc, enable_soft_shadows ? 1 : 0);
            glUniform1i(soft_shadow_samples_loc, soft_shadow_samples);
            glUniform1f(light_radius_loc, light_radius);
            glUniform1i(enable_ao_loc, enable_ao ? 1 : 0);
            glUniform1i(ao_samples_loc, ao_samples);

            // Set Phase 3 GI uniforms
            glUniform1i(enable_gi_loc, enable_gi ? 1 : 0);
            glUniform1i(gi_samples_loc, gi_samples);
            glUniform1f(gi_intensity_loc, gi_intensity);

            // Set Phase 3.5 advanced reflection uniforms
            glUniform1i(enable_ssr_loc, enable_ssr ? 1 : 0);
            glUniform1i(ssr_samples_loc, ssr_samples);
            glUniform1f(ssr_step_size_loc, ssr_step_size);
            glUniform1f(ssr_roughness_cutoff_loc, ssr_roughness_cutoff);
            glUniform1i(enable_env_mapping_loc, enable_env_mapping ? 1 : 0);
            glUniform1i(env_mip_levels_loc, env_mip_levels);

            // Set Phase 4 post-processing uniforms
            glUniform1i(enable_ssao_loc, enable_ssao ? 1 : 0);
            glUniform1i(ssao_samples_loc, ssao_samples);
            glUniform1f(ssao_radius_loc, ssao_radius);
            glUniform1f(ssao_intensity_loc, ssao_intensity);

            glUniform1i(enable_bloom_loc, enable_bloom ? 1 : 0);
            glUniform1f(bloom_threshold_loc, bloom_threshold);
            glUniform1f(bloom_intensity_loc, bloom_intensity);

            glUniform1i(enable_vignette_loc, enable_vignette ? 1 : 0);
            glUniform1f(vignette_intensity_loc, vignette_intensity);
            glUniform1f(vignette_falloff_loc, vignette_falloff);

            glUniform1i(enable_film_grain_loc, enable_film_grain ? 1 : 0);
            glUniform1f(grain_intensity_loc, grain_intensity);
            glUniform1f(grain_size_loc, grain_size);

            // Phase 5: Chromatic Aberration
            glUniform1i(enable_chromatic_aberration_loc, enable_chromatic_aberration ? 1 : 0);
            glUniform1f(chromatic_aberration_strength_loc, chromatic_aberration_strength);

            // Phase 5: Depth of Field
            glUniform1i(enable_dof_loc, enable_dof ? 1 : 0);
            glUniform1f(dof_focus_distance_loc, dof_focus_distance);
            glUniform1f(dof_aperture_loc, dof_aperture);

            // Phase 5: Motion Blur
            glUniform1i(enable_motion_blur_loc, enable_motion_blur ? 1 : 0);
            glUniform1f(motion_blur_strength_loc, motion_blur_strength);
            glUniform2f(motion_vector_loc, motion_vector_x, motion_vector_y);

            // Phase 5: Adaptive Quality
            glUniform1i(enable_adaptive_quality_loc, enable_adaptive_quality ? 1 : 0);
            glUniform1f(quality_scale_loc, quality_scale);

            // Phase 5: Lens Flares
            glUniform1i(enable_lens_flares_loc, enable_lens_flares ? 1 : 0);
            glUniform1f(lens_flare_intensity_loc, lens_flare_intensity);

            // Calculate light position in screen space (simplified)
            // In a real implementation, would project 3D light position to screen space
            glUniform2f(light_position_loc, light_position_x, light_position_y);

            // Phase 6: Temporal Anti-Aliasing
            glUniform1i(enable_taa_loc, enable_taa ? 1 : 0);
            glUniform1f(taa_mix_factor_loc, (enable_taa && taa_history_ready) ? taa_mix_factor : 0.0f);
            glUniform2f(camera_velocity_loc, camera_velocity_x, camera_velocity_y);
            // Bind history texture to texture unit 0
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, history_texture);
            glUniform1i(history_buffer_loc, 0);

            glUniform1i(tone_mapping_op_loc, tone_mapping_op);
            glUniform1f(exposure_loc, exposure);
            glUniform1f(contrast_loc, contrast);
            glUniform1f(saturation_loc, saturation);

            // Upload sphere uniforms using pre-cached locations
            for (size_t i = 0; i < spheres.size() && i < (size_t)MAX_SPHERES; i++) {
                glUniform3f(sp_center_loc[i],  spheres[i].center[0], spheres[i].center[1], spheres[i].center[2]);
                glUniform1f(sp_radius_loc[i],  spheres[i].radius);
                glUniform3f(sp_color_loc[i],   spheres[i].color[0],  spheres[i].color[1],  spheres[i].color[2]);
                glUniform1i(sp_mat_loc[i],     spheres[i].material);
                glUniform1f(sp_gscale_loc[i],  spheres[i].gradient_scale);
                glUniform1f(sp_goffset_loc[i], spheres[i].gradient_offset);
                glUniform1f(sp_rough_loc[i],   spheres[i].roughness);
                glUniform1f(sp_metal_loc[i],   spheres[i].metallic);
            }

            // Upload triangle uniforms using pre-cached locations
            for (size_t i = 0; i < triangles.size() && i < (size_t)MAX_TRIS; i++) {
                glUniform3f(tr_v0_loc[i],      triangles[i].v0[0],     triangles[i].v0[1],     triangles[i].v0[2]);
                glUniform3f(tr_v1_loc[i],      triangles[i].v1[0],     triangles[i].v1[1],     triangles[i].v1[2]);
                glUniform3f(tr_v2_loc[i],      triangles[i].v2[0],     triangles[i].v2[1],     triangles[i].v2[2]);
                glUniform3f(tr_norm_loc[i],    triangles[i].normal[0],  triangles[i].normal[1],  triangles[i].normal[2]);
                glUniform3f(tr_color_loc[i],   triangles[i].color[0],   triangles[i].color[1],   triangles[i].color[2]);
                glUniform1i(tr_mat_loc[i],     triangles[i].material);
                glUniform1f(tr_gscale_loc[i],  triangles[i].gradient_scale);
                glUniform1f(tr_goffset_loc[i], triangles[i].gradient_offset);
                glUniform1f(tr_rough_loc[i],   triangles[i].roughness);
                glUniform1f(tr_metal_loc[i],   triangles[i].metallic);
            }

            // Render fullscreen quad
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // Render overlays (need to temporarily switch from OpenGL to SDL2 rendering)
            if (help_overlay.is_showing()) {
                help_overlay.render(window);
            } else if (controls_panel.is_showing()) {
                controls_panel.render(window, enable_reflections, lighting_mode, num_lights);
            }

            SDL_GL_SwapWindow(window);

            // Capture this frame into the TAA history texture.
            // glCopyTexImage2D reads from the front buffer after swap.
            if (enable_taa) {
                glBindTexture(GL_TEXTURE_2D, history_texture);
                glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, WIDTH, HEIGHT, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
                taa_history_ready = true;
            }

            // Calculate FPS
            frame_count++;
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_frame_time).count();
            if (elapsed >= 1.0) {
                fps = frame_count / elapsed;
                frame_count = 0;
                last_frame_time = now;

                // Phase 5: Adaptive Quality - Auto-adjust to maintain target FPS
                if (enable_adaptive_quality && frame_count > 5) {  // Wait for stable FPS
                    float fps_diff = fps - target_fps;

                    if (fps_diff < -10.0f) {  // FPS too low, reduce quality
                        quality_scale = std::max(0.5f, quality_scale - adaptive_aggressiveness * 0.1f);
                        std::cout << "\r[Adaptive Quality] " << std::fixed << std::setprecision(2)
                                 << "FPS: " << fps << " (Below target) | Quality: " << quality_scale
                                 << " | Cam: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                                 << "     " << std::flush;

                        // Reduce expensive features
                        if (quality_scale < 0.7f) {
                            if (enable_gi) { enable_gi = false; std::cout << "\n[Adaptive] Disabled GI"; }
                            if (enable_ssr) { enable_ssr = false; std::cout << "\n[Adaptive] Disabled SSR"; }
                        }
                        if (quality_scale < 0.6f) {
                            if (enable_bloom) { enable_bloom = false; std::cout << "\n[Adaptive] Disabled Bloom"; }
                            if (enable_ssao) { enable_ssao = false; std::cout << "\n[Adaptive] Disabled SSAO"; }
                        }
                    } else if (fps_diff > 15.0f) {  // FPS high, increase quality
                        quality_scale = std::min(1.5f, quality_scale + adaptive_aggressiveness * 0.05f);
                        std::cout << "\r[Adaptive Quality] " << std::fixed << std::setprecision(2)
                                 << "FPS: " << fps << " (Above target) | Quality: " << quality_scale
                                 << " | Cam: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                                 << "     " << std::flush;

                        // Enable features back when quality is high
                        if (quality_scale > 0.9f) {
                            if (!enable_gi) { enable_gi = true; std::cout << "\n[Adaptive] Enabled GI"; }
                            if (!enable_ssr) { enable_ssr = true; std::cout << "\n[Adaptive] Enabled SSR"; }
                        }
                        if (quality_scale > 0.8f) {
                            if (!enable_bloom) { enable_bloom = true; std::cout << "\n[Adaptive] Enabled Bloom"; }
                            if (!enable_ssao) { enable_ssao = true; std::cout << "\n[Adaptive] Enabled SSAO"; }
                        }
                    } else {
                        // FPS within target range, show normal display
                        std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                                 << " | Cam: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                                 << "     " << std::flush;
                    }
                } else {
                    // Normal display without adaptive quality
                    std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps
                             << " | Cam: " << camera.position[0] << ", " << camera.position[1] << ", " << camera.position[2]
                             << "     " << std::flush;
                }
            }

            need_render = true;  // Render continuously — the GPU shader is fast once compiled
        }
    }

    std::cout << "\n=== Exiting ===" << std::endl;

    // Cleanup
    glDeleteTextures(1, &history_texture);
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
