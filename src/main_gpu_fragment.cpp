#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

// Include GLEW before SDL to avoid header conflicts
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// Window dimensions
const int WIDTH = 1280;
const int HEIGHT = 720;

// Maximum scene objects
const int MAX_SPHERES = 50;
const int MAX_TRIANGLES = 50;
const int MAX_MATERIALS = 50;
const int MAX_LIGHTS = 10;

// Scene data structures (must match shader)
struct Sphere {
    float center[4];
    float radius;
    int material_id;
    float padding[3];
};

struct Triangle {
    float v0[4];
    float v1[4];
    float v2[4];
    float normal[4];
    int material_id;
    float padding[3];
};

struct Material {
    float albedo[4];
    float fuzz;
    int type;  // 0 = Lambertian, 1 = Metal, 2 = Dielectric
    float padding[2];
    float refraction_index;
};

struct Light {
    float position[4];
    float intensity[4];
};

struct CameraData {
    float position[4];
    float lookat[4];
    float vup[4];
    float vfov;
    float aspect_ratio;
    float padding[2];
};

// Cornell Box scene data
void setup_cornell_box(std::vector<Sphere>& spheres, std::vector<Triangle>& triangles,
                       std::vector<Material>& materials, std::vector<Light>& lights) {
    materials.clear();
    spheres.clear();
    triangles.clear();
    lights.clear();

    // Materials
    Material red_mat = {{0.65f, 0.05f, 0.05f, 1.0f}, 0.0f, 0, {0, 0}, 1.0f};
    Material green_mat = {{0.12f, 0.45f, 0.15f, 1.0f}, 0.0f, 0, {0, 0}, 1.0f};
    Material white_mat = {{0.73f, 0.73f, 0.73f, 1.0f}, 0.0f, 0, {0, 0}, 1.0f};
    Material light_mat = {{15.0f, 15.0f, 15.0f, 1.0f}, 0.0f, 0, {0, 0}, 1.0f};
    Material metal_mat = {{0.95f, 0.95f, 0.95f, 1.0f}, 0.0f, 1, {0, 0}, 1.0f};
    Material glass_mat = {{1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 2, {0, 0}, 1.5f};

    materials.push_back(red_mat);    // 0
    materials.push_back(green_mat);  // 1
    materials.push_back(white_mat);  // 2
    materials.push_back(light_mat);  // 3
    materials.push_back(metal_mat);  // 4
    materials.push_back(glass_mat);  // 5

    // Light
    Light light;
    light.position[0] = 0.0f; light.position[1] = 4.99f; light.position[2] = 0.0f; light.position[3] = 1.0f;
    light.intensity[0] = 1.0f; light.intensity[1] = 0.9f; light.intensity[2] = 0.8f; light.intensity[3] = 1.0f;
    lights.push_back(light);

    // Ceiling light (emissive triangle)
    Triangle ceiling_light;
    ceiling_light.v0[0] = -1.0f; ceiling_light.v0[1] = 4.99f; ceiling_light.v0[2] = -1.0f; ceiling_light.v0[3] = 1.0f;
    ceiling_light.v1[0] = 1.0f; ceiling_light.v1[1] = 4.99f; ceiling_light.v1[2] = -1.0f; ceiling_light.v1[3] = 1.0f;
    ceiling_light.v2[0] = 1.0f; ceiling_light.v2[1] = 4.99f; ceiling_light.v2[2] = 1.0f; ceiling_light.v2[3] = 1.0f;
    ceiling_light.normal[0] = 0.0f; ceiling_light.normal[1] = -1.0f; ceiling_light.normal[2] = 0.0f; ceiling_light.normal[3] = 1.0f;
    ceiling_light.material_id = 3;
    triangles.push_back(ceiling_light);

    // Floor
    Triangle floor1;
    floor1.v0[0] = -5.0f; floor1.v0[1] = 0.0f; floor1.v0[2] = -5.0f; floor1.v0[3] = 1.0f;
    floor1.v1[0] = 5.0f; floor1.v1[1] = 0.0f; floor1.v1[2] = -5.0f; floor1.v1[3] = 1.0f;
    floor1.v2[0] = 5.0f; floor1.v2[1] = 0.0f; floor1.v2[2] = 5.0f; floor1.v2[3] = 1.0f;
    floor1.normal[0] = 0.0f; floor1.normal[1] = 1.0f; floor1.normal[2] = 0.0f; floor1.normal[3] = 1.0f;
    floor1.material_id = 2;
    triangles.push_back(floor1);

    Triangle floor2;
    floor2.v0[0] = -5.0f; floor2.v0[1] = 0.0f; floor2.v0[2] = -5.0f; floor2.v0[3] = 1.0f;
    floor2.v1[0] = 5.0f; floor2.v1[1] = 0.0f; floor2.v1[2] = 5.0f; floor2.v1[3] = 1.0f;
    floor2.v2[0] = -5.0f; floor2.v2[1] = 0.0f; floor2.v2[2] = 5.0f; floor2.v2[3] = 1.0f;
    floor2.normal[0] = 0.0f; floor2.normal[1] = 1.0f; floor2.normal[2] = 0.0f; floor2.normal[3] = 1.0f;
    floor2.material_id = 2;
    triangles.push_back(floor2);

    // Ceiling
    Triangle ceil1;
    ceil1.v0[0] = -5.0f; ceil1.v0[1] = 5.0f; ceil1.v0[2] = -5.0f; ceil1.v0[3] = 1.0f;
    ceil1.v1[0] = 5.0f; ceil1.v1[1] = 5.0f; ceil1.v1[2] = -5.0f; ceil1.v1[3] = 1.0f;
    ceil1.v2[0] = 5.0f; ceil1.v2[1] = 5.0f; ceil1.v2[2] = 5.0f; ceil1.v2[3] = 1.0f;
    ceil1.normal[0] = 0.0f; ceil1.normal[1] = -1.0f; ceil1.normal[2] = 0.0f; ceil1.normal[3] = 1.0f;
    ceil1.material_id = 2;
    triangles.push_back(ceil1);

    Triangle ceil2;
    ceil2.v0[0] = -5.0f; ceil2.v0[1] = 5.0f; ceil2.v0[2] = -5.0f; ceil2.v0[3] = 1.0f;
    ceil2.v1[0] = 5.0f; ceil2.v1[1] = 5.0f; ceil2.v1[2] = 5.0f; ceil2.v1[3] = 1.0f;
    ceil2.v2[0] = -5.0f; ceil2.v2[1] = 5.0f; ceil2.v2[2] = 5.0f; ceil2.v2[3] = 1.0f;
    ceil2.normal[0] = 0.0f; ceil2.normal[1] = -1.0f; ceil2.normal[2] = 0.0f; ceil2.normal[3] = 1.0f;
    ceil2.material_id = 2;
    triangles.push_back(ceil2);

    // Back wall
    Triangle back1;
    back1.v0[0] = -5.0f; back1.v0[1] = 0.0f; back1.v0[2] = -5.0f; back1.v0[3] = 1.0f;
    back1.v1[0] = 5.0f; back1.v1[1] = 0.0f; back1.v1[2] = -5.0f; back1.v1[3] = 1.0f;
    back1.v2[0] = 5.0f; back1.v2[1] = 5.0f; back1.v2[2] = -5.0f; back1.v2[3] = 1.0f;
    back1.normal[0] = 0.0f; back1.normal[1] = 0.0f; back1.normal[2] = 1.0f; back1.normal[3] = 1.0f;
    back1.material_id = 2;
    triangles.push_back(back1);

    Triangle back2;
    back2.v0[0] = -5.0f; back2.v0[1] = 0.0f; back2.v0[2] = -5.0f; back2.v0[3] = 1.0f;
    back2.v1[0] = 5.0f; back2.v1[1] = 5.0f; back2.v1[2] = -5.0f; back2.v1[3] = 1.0f;
    back2.v2[0] = -5.0f; back2.v2[1] = 5.0f; back2.v2[2] = -5.0f; back2.v2[3] = 1.0f;
    back2.normal[0] = 0.0f; back2.normal[1] = 0.0f; back2.normal[2] = 1.0f; back2.normal[3] = 1.0f;
    back2.material_id = 2;
    triangles.push_back(back2);

    // Left wall (red)
    Triangle left1;
    left1.v0[0] = -5.0f; left1.v0[1] = 0.0f; left1.v0[2] = -5.0f; left1.v0[3] = 1.0f;
    left1.v1[0] = -5.0f; left1.v1[1] = 0.0f; left1.v1[2] = 5.0f; left1.v1[3] = 1.0f;
    left1.v2[0] = -5.0f; left1.v2[1] = 5.0f; left1.v2[2] = 5.0f; left1.v2[3] = 1.0f;
    left1.normal[0] = 1.0f; left1.normal[1] = 0.0f; left1.normal[2] = 0.0f; left1.normal[3] = 1.0f;
    left1.material_id = 0;
    triangles.push_back(left1);

    Triangle left2;
    left2.v0[0] = -5.0f; left2.v0[1] = 0.0f; left2.v0[2] = -5.0f; left2.v0[3] = 1.0f;
    left2.v1[0] = -5.0f; left2.v1[1] = 5.0f; left2.v1[2] = 5.0f; left2.v1[3] = 1.0f;
    left2.v2[0] = -5.0f; left2.v2[1] = 5.0f; left2.v2[2] = -5.0f; left2.v2[3] = 1.0f;
    left2.normal[0] = 1.0f; left2.normal[1] = 0.0f; left2.normal[2] = 0.0f; left2.normal[3] = 1.0f;
    left2.material_id = 0;
    triangles.push_back(left2);

    // Right wall (green)
    Triangle right1;
    right1.v0[0] = 5.0f; right1.v0[1] = 0.0f; right1.v0[2] = -5.0f; right1.v0[3] = 1.0f;
    right1.v1[0] = 5.0f; right1.v1[1] = 0.0f; right1.v1[2] = 5.0f; right1.v1[3] = 1.0f;
    right1.v2[0] = 5.0f; right1.v2[1] = 5.0f; right1.v2[2] = 5.0f; right1.v2[3] = 1.0f;
    right1.normal[0] = -1.0f; right1.normal[1] = 0.0f; right1.normal[2] = 0.0f; right1.normal[3] = 1.0f;
    right1.material_id = 1;
    triangles.push_back(right1);

    Triangle right2;
    right2.v0[0] = 5.0f; right2.v0[1] = 0.0f; right2.v0[2] = -5.0f; right2.v0[3] = 1.0f;
    right2.v1[0] = 5.0f; right2.v1[1] = 5.0f; right2.v1[2] = 5.0f; right2.v1[3] = 1.0f;
    right2.v2[0] = 5.0f; right2.v2[1] = 5.0f; right2.v2[2] = -5.0f; right2.v2[3] = 1.0f;
    right2.normal[0] = -1.0f; right2.normal[1] = 0.0f; right2.normal[2] = 0.0f; right2.normal[3] = 1.0f;
    right2.material_id = 1;
    triangles.push_back(right2);

    // Metal sphere
    Sphere metal_sphere;
    metal_sphere.center[0] = -1.5f; metal_sphere.center[1] = 1.0f; metal_sphere.center[2] = 0.0f; metal_sphere.center[3] = 1.0f;
    metal_sphere.radius = 1.0f;
    metal_sphere.material_id = 4;
    metal_sphere.padding[0] = metal_sphere.padding[1] = metal_sphere.padding[2] = 0.0f;
    spheres.push_back(metal_sphere);

    // Glass sphere
    Sphere glass_sphere;
    glass_sphere.center[0] = 1.5f; glass_sphere.center[1] = 1.0f; glass_sphere.center[2] = 0.0f; glass_sphere.center[3] = 1.0f;
    glass_sphere.radius = 1.0f;
    glass_sphere.material_id = 5;
    glass_sphere.padding[0] = glass_sphere.padding[1] = glass_sphere.padding[2] = 0.0f;
    spheres.push_back(glass_sphere);
}

// Fragment shader with full CPU renderer features
const char* fragment_shader_source = R"(
#version 120

// Scene data structures
struct Sphere {
    vec4 center;
    float radius;
    int material_id;
    vec3 padding;
};

struct Triangle {
    vec4 v0, v1, v2;
    vec4 normal;
    int material_id;
    vec3 padding;
};

struct Material {
    vec4 albedo;
    float fuzz;
    int type;  // 0 = Lambertian, 1 = Metal, 2 = Dielectric
    vec2 padding;
    float refraction_index;
};

struct Light {
    vec4 position;
    vec4 intensity;
};

struct Camera {
    vec4 position;
    vec4 lookat;
    vec4 vup;
    float vfov;
    float aspect_ratio;
    vec2 padding;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct HitRecord {
    int hit;  // 0 = false, 1 = true
    float t;
    vec3 point;
    vec3 normal;
    int material_id;
    int front_face;  // 0 = false, 1 = true
};

// Uniforms
uniform vec2 resolution;
uniform float time;
uniform int max_depth;
uniform int samples_per_pixel;
uniform int enable_shadows;
uniform int enable_reflections;

// Scene arrays
uniform Sphere spheres[50];
uniform Triangle triangles[50];
uniform Material materials[50];
uniform Light lights[10];
uniform Camera camera;

uniform int num_spheres;
uniform int num_triangles;
uniform int num_materials;
uniform int num_lights;

// Random number generation (simplified for GLSL 1.20 compatibility)
int seed;

float rand_float() {
    // Simple linear congruential generator
    seed = seed * 1103515245 + 12345;
    // Convert to float in [0,1)
    return mod(abs(float(seed)), 65536.0) / 65536.0;
}

vec3 rand_vec3() {
    return vec3(rand_float(), rand_float(), rand_float());
}

vec3 random_in_unit_sphere() {
    while (true) {
        vec3 p = 2.0 * rand_vec3() - 1.0;
        float len = dot(p, p);
        if (len < 1.0) {
            return p;
        }
    }
}

vec3 random_unit_vector() {
    return normalize(random_in_unit_sphere());
}

// Ray-sphere intersection
bool hit_sphere(Sphere sphere, Ray ray, float t_min, float t_max, inout HitRecord rec) {
    vec3 oc = ray.origin - sphere.center.xyz;
    float a = dot(ray.direction, ray.direction);
    float b = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - a * c;

    if (discriminant < 0.0) {
        return false;
    }

    float sqrt_d = sqrt(discriminant);
    float root = (-b - sqrt_d) / a;

    bool root_in_range1 = root < t_min || root > t_max;
    if (root_in_range1) {
        root = (-b + sqrt_d) / a;
        bool root_in_range2 = root < t_min || root > t_max;
        if (root_in_range2) {
            return false;
        }
    }

    rec.hit = 1;
    rec.t = root;
    rec.point = ray.origin + root * ray.direction;
    vec3 outward_normal = (rec.point - sphere.center.xyz) / sphere.radius;
    rec.front_face = dot(ray.direction, outward_normal) < 0.0 ? 1 : 0;
    rec.normal = rec.front_face == 1 ? outward_normal : -outward_normal;
    rec.material_id = sphere.material_id;
    return true;
}

// Ray-triangle intersection (Möller–Trumbore)
bool hit_triangle(Triangle tri, Ray ray, float t_min, float t_max, inout HitRecord rec) {
    vec3 edge1 = tri.v1.xyz - tri.v0.xyz;
    vec3 edge2 = tri.v2.xyz - tri.v0.xyz;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);

    bool small_a = abs(a) < 0.0001;
    if (small_a) {
        return false;
    }

    float f = 1.0 / a;
    vec3 s = ray.origin - tri.v0.xyz;
    float u = f * dot(s, h);

    bool u_in_range = u < 0.0 || u > 1.0;
    if (u_in_range) {
        return false;
    }

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);

    bool v_in_range = v < 0.0 || u + v > 1.0;
    if (v_in_range) {
        return false;
    }

    float t = f * dot(edge2, q);

    bool t_in_range = t < t_min || t > t_max;
    if (t_in_range) {
        return false;
    }

    rec.hit = 1;
    rec.t = t;
    rec.point = ray.origin + t * ray.direction;
    rec.front_face = dot(ray.direction, tri.normal.xyz) < 0.0 ? 1 : 0;
    rec.normal = rec.front_face == 1 ? tri.normal.xyz : -tri.normal.xyz;
    rec.material_id = tri.material_id;
    return true;
}

// Scene intersection
bool hit_scene(Ray ray, float t_min, float t_max, inout HitRecord rec) {
    HitRecord temp_rec;
    temp_rec.hit = 0;
    bool hit_anything = false;
    float closest_so_far = t_max;

    for (int i = 0; i < num_spheres; i++) {
        if (hit_sphere(spheres[i], ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    for (int i = 0; i < num_triangles; i++) {
        if (hit_triangle(triangles[i], ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

// Shadow check
bool is_shadowed(vec3 point, vec3 light_pos, float light_dist) {
    Ray shadow_ray;
    shadow_ray.origin = point;
    shadow_ray.direction = normalize(light_pos - point);

    HitRecord shadow_rec;
    float t_min = 0.001;
    float t_max = light_dist - 0.001;

    for (int i = 0; i < num_spheres; i++) {
        if (hit_sphere(spheres[i], shadow_ray, t_min, t_max, shadow_rec)) {
            return true;
        }
    }

    for (int i = 0; i < num_triangles; i++) {
        if (hit_triangle(triangles[i], shadow_ray, t_min, t_max, shadow_rec)) {
            return true;
        }
    }

    return false;
}

// Phong shading
vec3 compute_phong_shading(HitRecord rec) {
    Material mat = materials[rec.material_id];
    vec3 color = vec3(0.1) * mat.albedo.rgb; // Ambient

    for (int i = 0; i < num_lights; i++) {
        vec3 light_dir = lights[i].position.xyz - rec.point;
        float light_distance = length(light_dir);
        vec3 light_dir_norm = normalize(light_dir);

        // Early culling if surface faces away from light
        float dot_product = dot(rec.normal, light_dir_norm);
        bool facing_away = dot_product <= 0.0;
        if (facing_away) {
            continue;
        }

        // Shadow ray
        bool in_shadow = false;
        if (enable_shadows) {
            in_shadow = is_shadowed(rec.point, lights[i].position.xyz, light_distance);
        }

        if (!in_shadow) {
            // Diffuse component
            vec3 diffuse = dot_product * lights[i].intensity.rgb * mat.albedo.rgb;

            // Specular component (Phong)
            vec3 view_dir = normalize(-light_dir_norm);
            vec3 reflect_dir = reflect(-light_dir_norm, rec.normal);
            float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
            vec3 specular = spec * lights[i].intensity.rgb;

            color += diffuse + specular;
        }
    }

    return color;
}

// Schlick's approximation for Fresnel effect
float reflectance(float cosine, float ref_idx) {
    float r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5.0);
}

// Scatter functions
bool scatter_lambertian(HitRecord rec, inout Ray scattered, inout vec3 attenuation) {
    vec3 scatter_direction = rec.normal + random_unit_vector();

    bool is_degenerate = abs(scatter_direction.x) < 1e-8 && abs(scatter_direction.y) < 1e-8 && abs(scatter_direction.z) < 1e-8;
    if (is_degenerate) {
        scatter_direction = rec.normal;
    }

    scattered.origin = rec.point;
    scattered.direction = normalize(scatter_direction);
    attenuation = materials[rec.material_id].albedo.rgb;
    return true;
}

bool scatter_metal(HitRecord rec, vec3 reflected, inout Ray scattered, inout vec3 attenuation) {
    Material mat = materials[rec.material_id];
    vec3 scattered_dir = reflected + mat.fuzz * random_in_unit_sphere();
    scattered.origin = rec.point;
    scattered.direction = normalize(scattered_dir);
    attenuation = mat.albedo.rgb;
    return dot(scattered.direction, rec.normal) > 0.0;
}

bool scatter_dielectric(HitRecord rec, vec3 ray_dir, inout Ray scattered, inout vec3 attenuation) {
    Material mat = materials[rec.material_id];
    attenuation = vec3(1.0);
    float refraction_ratio = rec.front_face == 1 ? (1.0 / mat.refraction_index) : mat.refraction_index;

    vec3 unit_direction = normalize(ray_dir);
    float cos_theta = min(dot(-unit_direction, rec.normal), 1.0);
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    bool cannot_refract = refraction_ratio * sin_theta > 1.0;
    vec3 direction;

    bool should_reflect = cannot_refract || (reflectance(cos_theta, refraction_ratio) > rand_float());
    if (should_reflect) {
        direction = reflect(unit_direction, rec.normal);
    } else {
        direction = refract(unit_direction, rec.normal, refraction_ratio);
    }

    scattered.origin = rec.point;
    scattered.direction = direction;
    return true;
}

// Main ray color function
vec3 ray_color(Ray ray, int depth) {
    Ray current_ray = ray;
    vec3 current_attenuation = vec3(1.0);
    vec3 accumulated_color = vec3(0.0);

    for (int i = 0; i < depth; i++) {
        HitRecord rec;
        if (hit_scene(current_ray, 0.001, 1000.0, rec)) {
            // Calculate shading
            vec3 color = compute_phong_shading(rec);
            accumulated_color += current_attenuation * color;

            // Handle reflections/refractions
            if (enable_reflections) {
                Material mat = materials[rec.material_id];
                Ray scattered;
                vec3 attenuation;

                bool scattered_success = false;
                if (mat.type == 0) { // Lambertian
                    scattered_success = scatter_lambertian(rec, scattered, attenuation);
                } else if (mat.type == 1) { // Metal
                    vec3 reflected = reflect(current_ray.direction, rec.normal);
                    scattered_success = scatter_metal(rec, reflected, scattered, attenuation);
                } else if (mat.type == 2) { // Dielectric
                    scattered_success = scatter_dielectric(rec, current_ray.direction, scattered, attenuation);
                }

                if (scattered_success) {
                    current_ray = scattered;
                    current_attenuation *= attenuation;
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            // Background gradient
            vec3 unit_direction = normalize(current_ray.direction);
            float t = 0.5 * (unit_direction.y + 1.0);
            vec3 background = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            accumulated_color += current_attenuation * background;
            break;
        }
    }

    return accumulated_color;
}

// Get camera ray
Ray get_camera_ray(vec2 uv) {
    float theta = radians(camera.vfov);
    float h = tan(theta / 2.0);
    float viewport_height = 2.0 * h;
    float viewport_width = camera.aspect_ratio * viewport_height;

    vec3 w = normalize(camera.position.xyz - camera.lookat.xyz);
    vec3 u = normalize(cross(camera.vup.xyz, w));
    vec3 v = cross(w, u);

    vec3 origin = camera.position.xyz;
    vec3 horizontal = viewport_width * u;
    vec3 vertical = viewport_height * v;
    vec3 lower_left_corner = origin - horizontal / 2.0 - vertical / 2.0 - w;

    Ray ray;
    ray.origin = origin;
    ray.direction = lower_left_corner + uv.x * horizontal + uv.y * vertical - origin;
    return ray;
}

void main() {
    // Initialize random seed
    seed = int(gl_FragCoord.y * resolution.x + gl_FragCoord.x);

    vec2 uv = gl_FragCoord.xy / resolution;
    uv.y = 1.0 - uv.y;

    vec3 color = vec3(0.0);

    // Multi-sample anti-aliasing
    for (int s = 0; s < samples_per_pixel; s++) {
        vec2 offset = vec2(0.0);
        if (samples_per_pixel > 1) {
            offset = rand_vec3().xy / resolution;
        }

        Ray ray = get_camera_ray(uv + offset);
        color += ray_color(ray, max_depth);
    }

    // Average samples
    color /= float(samples_per_pixel);

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    gl_FragColor = vec4(color, 1.0);
}
)";

// Vertex shader for fullscreen quad
const char* vertex_shader_source = R"(
#version 120

attribute vec2 position;
varying vec2 uv;

void main() {
    uv = position * 0.5 + 0.5;
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

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=== GPU Ray Tracer (Fragment Shaders) ===" << std::endl;
    std::cout << "Full CPU renderer feature parity" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create OpenGL window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "GPU Ray Tracer - Fragment Shaders (Full Features)",
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

    std::cout << "✓ Shaders compiled and linked" << std::endl;

    // Setup scene
    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Material> materials;
    std::vector<Light> lights;
    setup_cornell_box(spheres, triangles, materials, lights);

    std::cout << "✓ Cornell Box scene created" << std::endl;
    std::cout << "  Spheres: " << spheres.size() << std::endl;
    std::cout << "  Triangles: " << triangles.size() << std::endl;
    std::cout << "  Materials: " << materials.size() << std::endl;
    std::cout << "  Lights: " << lights.size() << std::endl;

    // Create fullscreen quad
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    float quad_vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    GLint pos_loc = glGetAttribLocation(program, "position");
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(pos_loc);

    // Get uniform locations
    GLint resolution_loc = glGetUniformLocation(program, "resolution");
    GLint time_loc = glGetUniformLocation(program, "time");
    GLint max_depth_loc = glGetUniformLocation(program, "max_depth");
    GLint samples_loc = glGetUniformLocation(program, "samples_per_pixel");
    GLint shadows_loc = glGetUniformLocation(program, "enable_shadows");
    GLint reflections_loc = glGetUniformLocation(program, "enable_reflections");

    // Scene uniform locations
    GLint num_spheres_loc = glGetUniformLocation(program, "num_spheres");
    GLint num_triangles_loc = glGetUniformLocation(program, "num_triangles");
    GLint num_materials_loc = glGetUniformLocation(program, "num_materials");
    GLint num_lights_loc = glGetUniformLocation(program, "num_lights");

    // Camera setup
    CameraData camera;
    camera.position[0] = 0.0f; camera.position[1] = 2.0f; camera.position[2] = 15.0f; camera.position[3] = 1.0f;
    camera.lookat[0] = 0.0f; camera.lookat[1] = 2.0f; camera.lookat[2] = 0.0f; camera.lookat[3] = 1.0f;
    camera.vup[0] = 0.0f; camera.vup[1] = 1.0f; camera.vup[2] = 0.0f; camera.vup[3] = 1.0f;
    camera.vfov = 60.0f;
    camera.aspect_ratio = (float)WIDTH / (float)HEIGHT;
    camera.padding[0] = camera.padding[1] = 0.0f;

    // Set scene uniforms
    glUseProgram(program);
    glUniform2f(resolution_loc, (float)WIDTH, (float)HEIGHT);
    glUniform1i(max_depth_loc, 5);
    glUniform1i(samples_loc, 4);  // Start with 4 samples for performance
    glUniform1i(shadows_loc, 1);  // Enable shadows
    glUniform1i(reflections_loc, 1);  // Enable reflections

    glUniform1i(num_spheres_loc, spheres.size());
    glUniform1i(num_triangles_loc, triangles.size());
    glUniform1i(num_materials_loc, materials.size());
    glUniform1i(num_lights_loc, lights.size());

    // Upload scene data
    glUniform4fv(glGetUniformLocation(program, "spheres"), spheres.size() * 4, (GLfloat*)spheres.data());
    glUniform4fv(glGetUniformLocation(program, "triangles"), triangles.size() * 4, (GLfloat*)triangles.data());
    glUniform4fv(glGetUniformLocation(program, "materials"), materials.size() * 4, (GLfloat*)materials.data());
    glUniform4fv(glGetUniformLocation(program, "lights"), lights.size() * 2, (GLfloat*)lights.data());

    // Upload camera
    glUniform4fv(glGetUniformLocation(program, "camera"), 3, (GLfloat*)&camera);

    std::cout << "\n=== Starting GPU Ray Tracer ===" << std::endl;
    std::cout << "Features: Phong shading, shadows, reflections, triangles, dielectric materials" << std::endl;
    std::cout << "Controls: ESC to quit" << std::endl;
    std::cout << "==============================\n" << std::endl;

    // Main loop
    bool running = true;
    SDL_Event event;

    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float time = 0.0f;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        // Update time
        auto current_time = std::chrono::high_resolution_clock::now();
        time = std::chrono::duration<float>(current_time - start_time).count();

        // Render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glUniform1f(time_loc, time);

        // Render fullscreen quad
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        SDL_GL_SwapWindow(window);

        // Calculate FPS
        frame_count++;
        if (frame_count % 30 == 0) {
            auto end_time = std::chrono::high_resolution_clock::now();
            float elapsed = std::chrono::duration<float>(end_time - start_time).count();
            float fps = frame_count / elapsed;
            std::cout << "FPS: " << fps << "\r" << std::flush;
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