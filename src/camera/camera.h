#ifndef CAMERA_H
#define CAMERA_H

#include "../math/vec3.h"
#include "../math/ray.h"
#include "../math/frustum.h"

class Camera {
public:
    // Camera parameters
    Point3 lookfrom;
    Point3 lookat;
    Vec3 vup;
    float vfov; // Vertical field of view in degrees
    float aspect_ratio;
    float aperture; // Depth of field (0 = pinhole)
    float focus_dist;

    // Computed parameters
    Point3 origin;
    Vec3 horizontal;
    Vec3 vertical;
    Vec3 lower_left_corner;
    Vec3 u, v, w;

    // Default constructor
    Camera()
        : lookfrom(0, 0, 0), lookat(0, 0, -1), vup(0, 1, 0),
          vfov(40), aspect_ratio(16.0f / 9.0f), aperture(0), focus_dist(1) {
        update_camera();
    }

    // Full constructor
    Camera(Point3 lookfrom, Point3 lookat, Vec3 vup, float vfov, float aspect_ratio,
           float aperture, float focus_dist)
        : lookfrom(lookfrom), lookat(lookat), vup(vup), vfov(vfov),
          aspect_ratio(aspect_ratio), aperture(aperture), focus_dist(focus_dist) {
        update_camera();
    }

    // Update camera parameters based on current settings
    void update_camera() {
        origin = lookfrom;

        // Calculate camera coordinate system
        w = unit_vector(lookfrom - lookat);  // Forward direction (negative z)
        u = unit_vector(cross(vup, w));      // Right direction (positive x)
        v = cross(w, u);                     // Up direction (positive y)

        // Calculate viewport dimensions
        float theta = degrees_to_radians(vfov);
        float h = std::tan(theta / 2);
        float viewport_height = 2.0f * h * focus_dist;
        float viewport_width = aspect_ratio * viewport_height;

        horizontal = viewport_width * u;
        vertical = viewport_height * v;
        lower_left_corner = origin - horizontal / 2 - vertical / 2 - focus_dist * w;
    }

    // Get ray for a given pixel (u, v in [0, 1])
    Ray get_ray(float s, float t) const {
        // For now, no depth of field (aperture = 0)
        return Ray(origin, lower_left_corner + s * horizontal + t * vertical - origin);
    }

    // Axis-aligned view frustum for primary-ray culling (vfov is degrees; near/far in world units).
    Frustum::Frustum make_view_frustum(float near_plane, float far_plane) const {
        Vec3 w = unit_vector(lookfrom - lookat);
        Vec3 forward = -w;
        Vec3 right = unit_vector(cross(vup, w));
        Vec3 up = cross(w, right);
        const float fovy_rad = vfov * static_cast<float>(M_PI) / 180.0f;
        return Frustum::create_frustum(origin, forward, up, right, fovy_rad, aspect_ratio, near_plane, far_plane);
    }

private:
    float degrees_to_radians(float degrees) {
        return degrees * M_PI / 180.0f;
    }
};

#endif // CAMERA_H
