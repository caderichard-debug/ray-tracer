#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "vec3.h"
#include <array>

// Camera frustum for culling objects outside view
// Defines 6 planes: left, right, top, bottom, near, far

namespace Frustum {

struct Plane {
    Vec3 normal;    // Plane normal (pointing inward)
    float distance; // Distance from origin

    Plane() : normal(0, 0, 0), distance(0) {}
    Plane(Vec3 n, float d) : normal(n), distance(d) {}

    // Test if a point is on the positive side of the plane (inside frustum)
    bool is_inside(Vec3 point) const {
        return dot(point, normal) + distance >= 0;
    }
};

struct Frustum {
    std::array<Plane, 6> planes; // left, right, top, bottom, near, far

    Frustum() {}

    // Test if a bounding sphere is inside the frustum
    bool is_sphere_inside(Vec3 center, float radius) const {
        for (const auto& plane : planes) {
            // Distance from sphere center to plane
            float dist = dot(center, plane.normal) + plane.distance;
            if (dist < -radius) {
                return false; // Sphere is completely outside this plane
            }
        }
        return true; // Sphere is inside all planes
    }

    // Test if an axis-aligned bounding box is inside the frustum
    bool is_aabb_inside(Vec3 min, Vec3 max) const {
        // Test all 8 corners of the AABB
        Vec3 corners[8] = {
            Vec3(min.x, min.y, min.z),
            Vec3(min.x, min.y, max.z),
            Vec3(min.x, max.y, min.z),
            Vec3(min.x, max.y, max.z),
            Vec3(max.x, min.y, min.z),
            Vec3(max.x, min.y, max.z),
            Vec3(max.x, max.y, min.z),
            Vec3(max.x, max.y, max.z)
        };

        // Check if any corner is inside all planes
        for (const auto& corner : corners) {
            bool inside = true;
            for (const auto& plane : planes) {
                if (dot(corner, plane.normal) + plane.distance < 0) {
                    inside = false;
                    break;
                }
            }
            if (inside) return true; // At least one corner is inside
        }

        return false; // All corners are outside at least one plane
    }
};

// Create frustum from camera parameters
inline Frustum create_frustum(Vec3 position, Vec3 forward, Vec3 up, Vec3 right,
                             float fov_y, float aspect_ratio, float near_plane, float far_plane) {
    Frustum frustum;

    float tan_half_fov = std::tan(fov_y / 2.0f);

    // Calculate frustum dimensions
    float near_height = 2.0f * near_plane * tan_half_fov;
    float near_width = near_height * aspect_ratio;
    float far_height = 2.0f * far_plane * tan_half_fov;
    float far_width = far_height * aspect_ratio;

    // Frustum center points
    Vec3 near_center = position + forward * near_plane;
    Vec3 far_center = position + forward * far_plane;

    // Calculate plane normals and distances
    Vec3 far_normal = -forward; // Pointing inward
    frustum.planes[4] = Plane(far_normal, -dot(far_center, far_normal));

    Vec3 near_normal = forward; // Pointing inward
    frustum.planes[5] = Plane(near_normal, -dot(near_center, near_normal));

    // Right plane
    Vec3 right_normal = (cross(up, (forward * far_plane + right * (far_width / 2.0f)) - position)).normalized();
    frustum.planes[0] = Plane(right_normal, -dot(position, right_normal));

    // Left plane
    Vec3 left_normal = (cross((forward * far_plane - right * (far_width / 2.0f)) - position, up)).normalized();
    frustum.planes[1] = Plane(left_normal, -dot(position, left_normal));

    // Top plane
    Vec3 top_normal = (cross(right, (forward * far_plane + up * (far_height / 2.0f)) - position)).normalized();
    frustum.planes[2] = Plane(top_normal, -dot(position, top_normal));

    // Bottom plane
    Vec3 bottom_normal = (cross((forward * far_plane - up * (far_height / 2.0f)) - position, right)).normalized();
    frustum.planes[3] = Plane(bottom_normal, -dot(position, bottom_normal));

    return frustum;
}

} // namespace Frustum

#endif // FRUSTUM_H