#ifndef BVH_H
#define BVH_H

#include "../math/ray.h"
#include "../math/frustum.h"
#include "../math/vec3.h"
#include "../primitives/sphere.h"
#include "../primitives/primitive.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

// Axis-Aligned Bounding Box
struct AABB {
    Vec3 min;
    Vec3 max;

    AABB() : min(Vec3(1e30f, 1e30f, 1e30f)),
             max(Vec3(-1e30f, -1e30f, -1e30f)) {}

    AABB(Vec3 min_val, Vec3 max_val) : min(min_val), max(max_val) {}

    // Expand AABB to include point
    void expand(Vec3 point) {
        min = Vec3(std::min(min.x, point.x),
                   std::min(min.y, point.y),
                   std::min(min.z, point.z));
        max = Vec3(std::max(max.x, point.x),
                   std::max(max.y, point.y),
                   std::max(max.z, point.z));
    }

    // Expand AABB to include another AABB
    void expand(AABB other) {
        expand(other.min);
        expand(other.max);
    }

    // Test if ray hits AABB (slab method)
    bool hit(const Ray& r, float t_min, float t_max) const {
        for (int axis = 0; axis < 3; axis++) {
            float inv_d = 1.0f / r.direction()[axis];
            float t0 = (min[axis] - r.origin()[axis]) * inv_d;
            float t1 = (max[axis] - r.origin()[axis]) * inv_d;

            if (inv_d < 0.0f) {
                std::swap(t0, t1);
            }

            t_min = std::max(t0, t_min);
            t_max = std::min(t1, t_max);

            if (t_max <= t_min) {
                return false;
            }
        }
        return true;
    }
};

// BVH Node
struct BVHNode {
    AABB bbox;
    std::shared_ptr<BVHNode> left;
    std::shared_ptr<BVHNode> right;
    std::vector<int> primitive_indices;  // Leaf node only
    bool is_leaf;

    BVHNode() : is_leaf(false) {}
};

// BVH Tree
class BVH {
    std::shared_ptr<BVHNode> root;
    std::vector<std::shared_ptr<Sphere>> primitives;

public:
    BVH() {}

    // Build BVH from primitives
    void build(const std::vector<std::shared_ptr<Sphere>>& spheres);

    // Traverse BVH to find closest hit. Optional view_frustum skips sphere tests outside the frustum
    // (same convention as Scene::hit — use for primary rays only).
    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec,
             const std::vector<std::shared_ptr<Sphere>>& spheres,
             const Frustum::Frustum* view_frustum = nullptr) const;

private:
    std::shared_ptr<BVHNode> build_recursive(std::vector<int> indices, int depth);
    int split_primitives(const std::vector<int>& indices, AABB& bbox);
    bool hit_recursive(std::shared_ptr<BVHNode> node,
                     const Ray& r, float t_min, float t_max,
                     HitRecord& rec,
                     const std::vector<std::shared_ptr<Sphere>>& spheres,
                     const Frustum::Frustum* view_frustum) const;
};

#endif // BVH_H
