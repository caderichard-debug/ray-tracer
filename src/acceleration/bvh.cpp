#include "bvh.h"
#include <iostream>

void BVH::build(const std::vector<std::shared_ptr<Sphere>>& spheres) {
    primitives = spheres;

    if (spheres.empty()) {
        root = nullptr;
        return;
    }

    // Create leaf indices
    std::vector<int> indices(spheres.size());
    for (int i = 0; i < spheres.size(); i++) {
        indices[i] = i;
    }

    // Build tree recursively
    root = build_recursive(indices, 0);

    std::cout << "BVH built with " << spheres.size() << " primitives" << std::endl;
}

std::shared_ptr<BVHNode> BVH::build_recursive(std::vector<int> indices, int depth) {
    auto node = std::make_shared<BVHNode>();

    // Base case: leaf node (1-2 primitives or max depth)
    if (indices.size() <= 2 || depth >= 20) {
        node->is_leaf = true;
        node->primitive_indices = indices;

        // Calculate bbox
        for (int idx : indices) {
            Vec3 center = primitives[idx]->center;
            float radius = primitives[idx]->radius;
            node->bbox.expand(center - Vec3(radius, radius, radius));
            node->bbox.expand(center + Vec3(radius, radius, radius));
        }
        return node;
    }

    // Calculate bounding box for all primitives
    node->bbox = AABB();
    for (int idx : indices) {
        Vec3 center = primitives[idx]->center;
        float radius = primitives[idx]->radius;
        node->bbox.expand(center - Vec3(radius, radius, radius));
        node->bbox.expand(center + Vec3(radius, radius, radius));
    }

    // Split primitives
    int split_axis = split_primitives(indices, node->bbox);

    // Partition indices
    std::vector<int> left_indices, right_indices;
    Vec3 center = (node->bbox.min + node->bbox.max) / 2.0f;

    for (int idx : indices) {
        if (primitives[idx]->center[split_axis] < center[split_axis]) {
            left_indices.push_back(idx);
        } else {
            right_indices.push_back(idx);
        }
    }

    // Handle edge case (all primitives on one side)
    if (left_indices.empty() || right_indices.empty()) {
        int mid = indices.size() / 2;
        left_indices = std::vector<int>(indices.begin(), indices.begin() + mid);
        right_indices = std::vector<int>(indices.begin() + mid, indices.end());
    }

    // Recurse
    node->left = build_recursive(left_indices, depth + 1);
    node->right = build_recursive(right_indices, depth + 1);

    return node;
}

int BVH::split_primitives(const std::vector<int>& indices, AABB& bbox) {
    // Find longest axis
    Vec3 extent = bbox.max - bbox.min;

    if (extent.x >= extent.y && extent.x >= extent.z) {
        return 0;  // X-axis
    } else if (extent.y >= extent.z) {
        return 1;  // Y-axis
    } else {
        return 2;  // Z-axis
    }
}

bool BVH::hit(const Ray& r, float t_min, float t_max, HitRecord& rec,
             const std::vector<std::shared_ptr<Sphere>>& spheres,
             const Frustum::Frustum* view_frustum) const {
    if (!root) {
        return false;
    }
    return hit_recursive(root, r, t_min, t_max, rec, spheres, view_frustum);
}

bool BVH::hit_recursive(std::shared_ptr<BVHNode> node,
                        const Ray& r, float t_min, float t_max,
                        HitRecord& rec,
                        const std::vector<std::shared_ptr<Sphere>>& spheres,
                        const Frustum::Frustum* view_frustum) const {
    // Check if ray hits node's bounding box
    if (!node->bbox.hit(r, t_min, t_max)) {
        return false;
    }

    // Leaf node: test primitives
    if (node->is_leaf) {
        bool hit_anything = false;
        float closest_so_far = t_max;

        for (int idx : node->primitive_indices) {
            const auto& sp = spheres[idx];
            if (view_frustum && !view_frustum->is_sphere_inside(sp->center, sp->radius)) {
                continue;
            }
            HitRecord temp_rec;
            if (sp->hit(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }
        return hit_anything;
    }

    // Internal node: recurse children
    HitRecord left_rec, right_rec;
    bool hit_left = hit_recursive(node->left, r, t_min, t_max, left_rec, spheres, view_frustum);
    bool hit_right = hit_recursive(node->right, r, t_min, t_max, right_rec, spheres, view_frustum);

    if (hit_left && hit_right) {
        rec = (left_rec.t < right_rec.t) ? left_rec : right_rec;
        return true;
    } else if (hit_left) {
        rec = left_rec;
        return true;
    } else if (hit_right) {
        rec = right_rec;
        return true;
    }

    return false;
}
