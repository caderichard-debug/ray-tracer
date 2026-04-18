#include "bvh.h"
#include <iostream>

namespace {

inline Vec3 box_center(const BVFlatNode& n) {
    return Vec3(0.5f * (n.bmin[0] + n.bmax[0]), 0.5f * (n.bmin[1] + n.bmax[1]), 0.5f * (n.bmin[2] + n.bmax[2]));
}

} // namespace

int BVH::flatten_node(const std::shared_ptr<BVHNode>& node) {
    BVFlatNode fn{};
    fn.bmin[0] = node->bbox.min.x;
    fn.bmin[1] = node->bbox.min.y;
    fn.bmin[2] = node->bbox.min.z;
    fn.bmax[0] = node->bbox.max.x;
    fn.bmax[1] = node->bbox.max.y;
    fn.bmax[2] = node->bbox.max.z;

    const int my_index = static_cast<int>(flat_nodes.size());
    flat_nodes.push_back(fn);

    if (node->is_leaf) {
        flat_nodes[my_index].left = -1;
        flat_nodes[my_index].right = -1;
        flat_nodes[my_index].prim_start = static_cast<int>(flat_leaf_prim_indices.size());
        flat_nodes[my_index].prim_count = static_cast<int>(node->primitive_indices.size());
        for (int p : node->primitive_indices) {
            flat_leaf_prim_indices.push_back(p);
        }
        return my_index;
    }

    const int L = flatten_node(node->left);
    const int R = flatten_node(node->right);
    flat_nodes[my_index].left = L;
    flat_nodes[my_index].right = R;
    flat_nodes[my_index].prim_start = 0;
    flat_nodes[my_index].prim_count = 0;
    return my_index;
}

bool BVH::hit_flat_iterative(const Ray& r, float t_min, float t_max, HitRecord& rec,
                             const std::vector<std::shared_ptr<Sphere>>& spheres,
                             const Frustum::Frustum* view_frustum) const {
    constexpr int kMaxStack = 256;
    int stack[kMaxStack];
    int sp = 0;
    if (flat_root_index < 0) {
        return false;
    }
    stack[sp++] = flat_root_index;

    bool hit_anything = false;
    float closest_so_far = t_max;
    HitRecord best;

    while (sp > 0) {
        const int ni = stack[--sp];
        const BVFlatNode& node = flat_nodes[static_cast<size_t>(ni)];
        if (!ray_hit_aabb_raw(r, node.bmin, node.bmax, t_min, closest_so_far)) {
            continue;
        }

        if (node.prim_count > 0) {
            for (int k = 0; k < node.prim_count; ++k) {
                const int idx = flat_leaf_prim_indices[static_cast<size_t>(node.prim_start + k)];
                const auto& sph = spheres[static_cast<size_t>(idx)];
                if (view_frustum && !view_frustum->is_sphere_inside(sph->center, sph->radius)) {
                    continue;
                }
                HitRecord temp_rec;
                if (sph->hit(r, t_min, closest_so_far, temp_rec)) {
                    hit_anything = true;
                    closest_so_far = temp_rec.t;
                    best = temp_rec;
                }
            }
        } else {
            const int li = node.left;
            const int ri = node.right;
            if (li < 0 || ri < 0) {
                continue;
            }
            const BVFlatNode& Ln = flat_nodes[static_cast<size_t>(li)];
            const BVFlatNode& Rn = flat_nodes[static_cast<size_t>(ri)];
            const bool hit_l = ray_hit_aabb_raw(r, Ln.bmin, Ln.bmax, t_min, closest_so_far);
            const bool hit_r = ray_hit_aabb_raw(r, Rn.bmin, Rn.bmax, t_min, closest_so_far);
            if (hit_l && hit_r) {
                const Vec3 lc = box_center(Ln);
                const Vec3 rc = box_center(Rn);
                const Vec3 o = r.origin();
                const float dl = (lc - o).length_squared();
                const float dr = (rc - o).length_squared();
                if (dl < dr) {
                    if (sp + 2 > kMaxStack) {
                        continue;
                    }
                    stack[sp++] = ri;
                    stack[sp++] = li;
                } else {
                    if (sp + 2 > kMaxStack) {
                        continue;
                    }
                    stack[sp++] = li;
                    stack[sp++] = ri;
                }
            } else if (hit_l) {
                if (sp >= kMaxStack) {
                    continue;
                }
                stack[sp++] = li;
            } else if (hit_r) {
                if (sp >= kMaxStack) {
                    continue;
                }
                stack[sp++] = ri;
            }
        }
    }

    if (hit_anything) {
        rec = best;
    }
    return hit_anything;
}

void BVH::build(const std::vector<std::shared_ptr<Sphere>>& spheres) {
    primitives = spheres;
    flat_nodes.clear();
    flat_leaf_prim_indices.clear();
    flat_root_index = -1;

    if (spheres.empty()) {
        root = nullptr;
        flat_root_index = -1;
        return;
    }

    // Create leaf indices
    std::vector<int> indices(spheres.size());
    for (int i = 0; i < spheres.size(); i++) {
        indices[i] = i;
    }

    // Build tree recursively
    root = build_recursive(indices, 0);
    flat_root_index = flatten_node(root);

    std::cout << "BVH built with " << spheres.size() << " primitives (" << flat_nodes.size() << " flat nodes)"
              << std::endl;
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
    if (has_flat_layout()) {
        return hit_flat_iterative(r, t_min, t_max, rec, spheres, view_frustum);
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
