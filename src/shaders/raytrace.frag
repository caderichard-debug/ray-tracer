#version 330 core

// Fragment shader for GPU ray tracing
// This approach is compatible with older GPUs that don't support compute shaders

// Camera uniforms
layout(std140) uniform CameraBlock {
    vec4 position;
    vec4 lookat;
    vec4 vup;
    float vfov;
    float aspect_ratio;
} camera;

// Scene data (simplified for compatibility)
uniform samplerBuffer sphere_data;
uniform samplerBuffer material_data;
uniform int num_spheres;
uniform int max_depth;
uniform vec2 resolution;
uniform int frame_count;

// Output color
out vec4 frag_color;

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

// Random number generation
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

vec3 ray_at(Ray r, float t) {
    return r.origin + t * r.direction;
}

// Simple sphere intersection (simplified for demo)
bool hit_sphere(vec4 sphere, int material_id, Ray r, float t_min, float t_max, inout HitRecord rec) {
    vec3 center = sphere.xyz;
    float radius = sphere.w;

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
    rec.p = ray_at(r, root);
    vec3 outward_normal = (rec.p - center) / radius;
    rec.front_face = dot(r.direction, outward_normal) < 0.0;
    rec.normal = rec.front_face ? outward_normal : -outward_normal;
    rec.material_id = material_id;

    return true;
}

bool scene_hit(Ray r, float t_min, float t_max, inout HitRecord rec) {
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_so_far = t_max;

    // Check spheres (simplified - just use first few as demo)
    for (int i = 0; i < min(5, num_spheres); i++) {
        vec4 sphere = texelFetch(sphere_data, i);
        if (hit_sphere(sphere, i % 3, r, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

vec3 ray_color(Ray r, int depth) {
    HitRecord rec;

    if (depth <= 0) {
        return vec3(0.0, 0.0, 0.0);
    }

    // Background gradient
    if (!scene_hit(r, 0.001, 1000.0, rec)) {
        vec3 unit_direction = normalize(r.direction);
        float t = 0.5 * (unit_direction.y + 1.0);
        return mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);
    }

    // Simple coloring based on normal
    vec3 normal_color = 0.5 * (rec.normal + vec3(1.0));

    // Add some simple lighting
    vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(rec.normal, light_dir), 0.0);

    return normal_color * (0.2 + 0.8 * diff);
}

void main() {
    // Simple test pattern to verify OpenGL is working
    vec2 uv = gl_FragCoord.xy / resolution;

    // Create a colorful test pattern
    vec3 color;
    color.r = uv.x;
    color.g = uv.y;
    color.b = 0.5 + 0.5 * sin(gl_FragCoord.x * 0.01 + gl_FragCoord.y * 0.01);

    // Add some bands
    float bands = sin(gl_FragCoord.y * 0.05) * 0.5 + 0.5;
    color *= (0.5 + 0.5 * bands);

    frag_color = vec4(color, 1.0);
}
