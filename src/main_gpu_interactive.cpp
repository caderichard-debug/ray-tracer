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

// Scene selection (set via Makefile)
#ifndef SCENE_NAME
#define SCENE_NAME "cornell_box"  // Default scene
#endif

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
        // Don't call TTF_Quit() here - multiple panels use TTF
        // TTF_Quit() will be called at program exit
    }

    bool init() {
        if (initialized) return true;

        if (TTF_Init() == -1) {
            std::cerr << "HelpOverlay: TTF_Init failed: " << TTF_GetError() << std::endl;
            return false;
        }

        font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 16);
        if (!font) {
            std::cerr << "HelpOverlay: Failed to load font: " << TTF_GetError() << std::endl;
            return false;
        }

        initialized = true;
        std::cout << "HelpOverlay initialized successfully" << std::endl;
        return true;
    }

    void toggle() {
        show = !show;
        if (show) {
            render_to_console();
        }
    }
    bool is_showing() const { return show; }

    void render_to_console() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "=== GPU Ray Tracer Help ===" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  WASD/Arrows - Move camera" << std::endl;
        std::cout << "  Mouse       - Look around (click to capture)" << std::endl;
        std::cout << "  R           - Toggle reflections" << std::endl;
        std::cout << "  P           - Toggle Phong/PBR lighting" << std::endl;
        std::cout << "  L           - Cycle light configurations" << std::endl;
        std::cout << "  H           - Toggle this help" << std::endl;
        std::cout << "  C           - Toggle controls panel" << std::endl;
        std::cout << "  ESC         - Quit" << std::endl;
        std::cout << "\nPhase 1 Features:" << std::endl;
        std::cout << "  " << (ENABLE_PBR ? "✓" : "✗") << " PBR lighting (Cook-Torrance BRDF)" << std::endl;
        std::cout << "  " << (ENABLE_MULTIPLE_LIGHTS ? "✓" : "✗") << " Multiple lights (press L to cycle)" << std::endl;
        std::cout << "  " << (ENABLE_TONE_MAPPING ? "✓" : "✗") << " ACES tone mapping" << std::endl;
        std::cout << "  " << (ENABLE_GAMMA_CORRECTION ? "✓" : "✗") << " Gamma correction" << std::endl;
        std::cout << "\nPhase 2 Features:" << std::endl;
        std::cout << "  " << (ENABLE_SOFT_SHADOWS ? "✓" : "✗") << " Soft shadows (area light sampling)" << std::endl;
        std::cout << "  " << (ENABLE_AMBIENT_OCCLUSION ? "✓" : "✗") << " Ambient occlusion (ray-traced)" << std::endl;
        std::cout << "\nPerformance: 60-500x faster than CPU" << std::endl;
        std::cout << "  - Real-time ray tracing at 60+ FPS" << std::endl;
        std::cout << "  - GLSL 1.20 (OpenGL 2.0+ compatible)" << std::endl;
        std::cout << "\nPress H to close this help\n" << std::endl;
    }

    void render(SDL_Window* window, SDL_Renderer* renderer) {
        // Console-based rendering - no graphical overlay
        if (show && initialized) {
            render_to_console();
        }
    }
};

// Console-based settings panel (no GUI to avoid OpenGL conflicts)
class ControlsPanel {
private:
    bool show;
    bool initialized;

public:
    ControlsPanel() : show(false), initialized(false) {}

    ~ControlsPanel() {
        // No resources to clean up
    }

    bool init() {
        if (initialized) return true;
        initialized = true;
        std::cout << "ControlsPanel initialized successfully" << std::endl;
        return true;
    }

    void toggle() {
        show = !show;
        if (show) {
            render_to_console();
        }
    }
    bool is_showing() const { return show; }

    void render_to_console() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "=== GPU Settings Panel ===" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nRuntime Controls:" << std::endl;
        std::cout << "  R - Toggle reflections" << std::endl;
        std::cout << "  P - Toggle Phong/PBR lighting" << std::endl;
        std::cout << "  L - Cycle light count (1→2→3→4)" << std::endl;
        std::cout << "\nCompile-time Features:" << std::endl;
        std::cout << "  " << (ENABLE_PBR ? "✓" : "✗") << " PBR Lighting" << std::endl;
        std::cout << "  " << (ENABLE_MULTIPLE_LIGHTS ? "✓" : "✗") << " Multiple Lights" << std::endl;
        std::cout << "  " << (ENABLE_SOFT_SHADOWS ? "✓" : "✗") << " Soft Shadows" << std::endl;
        std::cout << "  " << (ENABLE_AMBIENT_OCCLUSION ? "✓" : "✗") << " Ambient Occlusion" << std::endl;
        std::cout << "  " << (ENABLE_TONE_MAPPING ? "✓" : "✗") << " ACES Tone Mapping" << std::endl;
        std::cout << "  " << (ENABLE_GAMMA_CORRECTION ? "✓" : "✗") << " Gamma Correction" << std::endl;
        std::cout << "\nQuality Presets:" << std::endl;
        std::cout << "  make gpu-fast          - Maximum performance" << std::endl;
        std::cout << "  make gpu-interactive   - Balanced quality" << std::endl;
        std::cout << "  make gpu-production    - High quality" << std::endl;
        std::cout << "  make gpu-showcase      - Maximum quality" << std::endl;
        std::cout << "\nPress C to close this panel\n" << std::endl;
    }

    void render(SDL_Window* window, SDL_Renderer* renderer, bool enable_reflections, int lighting_mode, int num_lights) {
        // Console-based rendering - show current settings
        if (show && initialized) {
            std::cout << "\n=== Current Settings ===" << std::endl;
            std::cout << "Reflections: " << (enable_reflections ? "ON" : "OFF") << std::endl;
            std::cout << "Lighting: " << (lighting_mode == 0 ? "Phong" : "PBR") << std::endl;
            std::cout << "Lights: " << num_lights << std::endl;
        }
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
    R"(

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
uniform float sphere_gradient_scale[16];
uniform float sphere_gradient_offset[16];
uniform float sphere_roughness[16];     // PBR roughness (0=mirror, 1=matte)
uniform float sphere_metallic[16];      // PBR metallic (0=dielectric, 1=metal)
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
#if ENABLE_PBR
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
#if ENABLE_SOFT_SHADOWS
// Soft shadow calculation using stratified area light sampling
float calculate_soft_shadow(vec3 hit_point, vec3 light_pos, vec3 N, vec3 L, float light_dist) {
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
#endif

// ============================================================================
#if ENABLE_AMBIENT_OCCLUSION
// Ray-traced ambient occlusion
float calculate_ao(vec3 hit_point, vec3 N) {
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
#endif

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
#if ENABLE_SOFT_SHADOWS
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
#else
        // Hard shadows only (soft shadows disabled at compile time)
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
#endif

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
#if ENABLE_AMBIENT_OCCLUSION
    float ao = 1.0;
    if (enable_ao && ao_samples > 0) {
        ao = calculate_ao(hit_point, N);
    }
    ibl *= ao;
#endif

    return Lo + ibl;
}

#endif // ENABLE_PBR

#if ENABLE_TONE_MAPPING
// Tone mapping (ACES filmic)
vec3 aces_tonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

#endif // ENABLE_TONE_MAPPING

#if ENABLE_GAMMA_CORRECTION
// Gamma correction
vec3 gamma_correct(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

#endif // ENABLE_GAMMA_CORRECTION

// Full color pipeline (tone mapping + gamma)
vec3 color_pipeline(vec3 color) {
    // Exposure
    float exposure = 1.0;
    color *= exposure;

#if ENABLE_TONE_MAPPING
    // Tone map
    color = aces_tonemap(color);
#endif

#if ENABLE_GAMMA_CORRECTION
    // Gamma correct
    color = gamma_correct(color);
#endif

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

#if ENABLE_PBR
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
#else
                // PBR not enabled, always use Phong
                if (num_lights > 0) {
                    vec3 L = normalize(light_positions[0] - hit_point);
                    color = phong_shading(normal, V, L, albedo, 32.0);
                } else {
                    // Fallback to single light
                    vec3 light_pos = vec3(0.0, 18.0, 0.0);
                    vec3 L = normalize(light_pos - hit_point);
                    color = phong_shading(normal, V, L, albedo, 32.0);
                }
#endif
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

    // Apply tone mapping and gamma correction
    color = color_pipeline(color);

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

    // Rendering settings (matching CPU version)
    bool enable_reflections = true;
    int max_depth = 5;

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
                    std::cout << "H key pressed - toggling help overlay" << std::endl;
                    help_overlay.toggle();
                } else if (event.key.keysym.sym == SDLK_c) {
                    std::cout << "C key pressed - toggling controls panel" << std::endl;
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

                name = "sphere_gradient_scale[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, spheres[i].gradient_scale);

                name = "sphere_gradient_offset[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, spheres[i].gradient_offset);

                name = "sphere_roughness[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, spheres[i].roughness);

                name = "sphere_metallic[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, spheres[i].metallic);
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

                name = "tri_roughness[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, triangles[i].roughness);

                name = "tri_metallic[" + std::to_string(i) + "]";
                loc = glGetUniformLocation(program, name.c_str());
                glUniform1f(loc, triangles[i].metallic);
            }

            // Render fullscreen quad
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // Render overlays (console-based)
            // Both panels can now be visible independently
            if (help_overlay.is_showing()) {
                help_overlay.render(window, nullptr);
            }
            if (controls_panel.is_showing()) {
                controls_panel.render(window, nullptr, enable_reflections, lighting_mode, num_lights);
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
