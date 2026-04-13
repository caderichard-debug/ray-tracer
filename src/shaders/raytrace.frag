#version 330 core

// Fragment shader for GPU ray tracing
// Uses array uniforms for scene data (compatible with OpenGL 3.3)

// Camera uniforms
uniform vec3 camera_pos;
uniform vec3 camera_target;
uniform vec3 camera_up;
uniform float vfov;
uniform float aspect_ratio;

// Scene data as arrays (OpenGL 3.3 compatible)
uniform vec3 sphere_centers[10];
uniform float sphere_radii[10];
uniform vec3 sphere_colors[10];
uniform int num_spheres;
uniform int max_depth;
uniform vec2 resolution;

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

// Sphere intersection
bool hit_sphere(vec3 center, float radius, int material_id, Ray r, float t_min, float t_max, inout HitRecord rec) {
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

    // Check spheres
    for (int i = 0; i < num_spheres; i++) {
        if (hit_sphere(sphere_centers[i], sphere_radii[i], i, r, t_min, closest_so_far, temp_rec)) {
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

    // Return color based on material
    return sphere_colors[rec.material_id] * (0.5 + 0.5 * rec.normal.y);
}

Ray get_camera_ray(vec2 uv) {
    vec3 w = normalize(camera_pos - camera_target);
    vec3 u = normalize(cross(camera_up, w));
    vec3 v = cross(w, u);

    float half_height = tan(vfov * 0.01745329252 * 0.5);
    float half_width = aspect_ratio * half_height;

    vec3 lower_left = camera_pos - half_width * u - half_height * v - w;
    vec3 horizontal = 2.0 * half_width * u;
    vec3 vertical = 2.0 * half_height * v;

    return Ray(camera_pos, lower_left + uv.x * horizontal + uv.y * vertical - camera_pos);
}

void main() {
    // Initialize random seed based on pixel position
    rand_seed = gl_FragCoord.x * 123.456 + gl_FragCoord.y * 789.012;

    // Normalized pixel coordinates (0 to 1)
    vec2 uv = gl_FragCoord.xy / resolution;

    // Get camera ray for this pixel
    Ray r = get_camera_ray(uv);

    // Simple ray color (no multi-sampling for now)
    vec3 color = ray_color(r, max_depth);

    // Simple gamma correction
    color = sqrt(color);

    frag_color = vec4(color, 1.0);
}
