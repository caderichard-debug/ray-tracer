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
    float far_height = 2.0f * far_plane * tan_half_fov;
    float far_width = far_height * aspect_ratio;

    // Frustum center points
    Vec3 near_center = position + forward * near_plane;
    Vec3 far_center = position + forward * far_plane;

    // Calculate plane normals and distances
    Vec3 far_normal = -forward; // Pointing inward (camera side of far plane)
    frustum.planes[4] = Plane(far_normal, -dot(far_center, far_normal));

    Vec3 near_normal = forward; // Pointing inward (into the frustum from near plane)
    frustum.planes[5] = Plane(near_normal, -dot(near_center, near_normal));

    // Side planes from camera + far-face corners; orient so a deep interior point is "inside".
    const float hw = far_width * 0.5f;
    const float hh = far_height * 0.5f;
    const Vec3 far_tl = far_center - right * hw + up * hh;
    const Vec3 far_tr = far_center + right * hw + up * hh;
    const Vec3 far_bl = far_center - right * hw - up * hh;
    const Vec3 far_br = far_center + right * hw - up * hh;
    const Vec3 inside_hint = position + forward * (0.5f * (near_plane + far_plane));

    auto oriented_plane = [](Vec3 p0, Vec3 p1, Vec3 p2, Vec3 inside_pt) {
        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p0;
        Vec3 n = unit_vector(cross(e1, e2));
        float dist = -dot(p0, n);
        if (dot(inside_pt, n) + dist < 0.0f) {
            n = -n;
            dist = -dot(p0, n);
        }
        return Plane(n, dist);
    };

    // planes[0..3]: right, left, top, bottom (historical order; all tested in is_sphere_inside)
    frustum.planes[0] = oriented_plane(position, far_tr, far_br, inside_hint);
    frustum.planes[1] = oriented_plane(position, far_bl, far_tl, inside_hint);
    frustum.planes[2] = oriented_plane(position, far_tl, far_tr, inside_hint);
    frustum.planes[3] = oriented_plane(position, far_br, far_bl, inside_hint);

    return frustum;
}

} // namespace Frustum

#endif // FRUSTUM_H