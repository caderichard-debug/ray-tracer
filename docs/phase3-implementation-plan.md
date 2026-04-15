# Phase 3 Implementation Plan: SIMD Integration and BVH

## Overview

**Status:** 📋 Planning
**Expected Timeline:** 4-6 hours
**Expected Improvement:** 30-60% overall (10-20% from SIMD + 20-40% from BVH for complex scenes)

**Current Baseline:**
- Resolution: 960x540 (default)
- Samples: 1 (default)
- Performance: ~3-5 MRays/sec
- Scene: Cornell Box (10 spheres)

---

## Task 1: Integrate SIMD Ray Packet Tracing

### Current State
- ✅ `src/math/vec3_avx2.h` - Vec3_AVX2 class with AVX2 operations
- ✅ `src/math/ray_packet.h` - RayPacket structure for 8 rays
- ✅ `src/primitives/sphere_simd.h` - AVX2 sphere intersection for 8 rays
- ✅ `src/primitives/sphere_avx2.h` - AVX2 single-sphere optimization
- ❌ **NOT INTEGRATED** into rendering pipeline

### Problem
The SIMD infrastructure exists but is unused. The current renderer uses:
- Scalar ray tracing (one ray at a time)
- AVX2 utility functions for individual operations (dot_product_avx2, etc.)
- No ray packet processing

### Implementation Plan

#### 1.1 Add Ray Packet Rendering to Renderer Class

**File:** `src/renderer/renderer.h`

**Additions:**
```cpp
// Ray packet tracing
struct RayPacketResult {
    std::vector<Color> colors;  // 8 colors
    __m256 valid_mask;          // Which rays succeeded
};

Color ray_color_packet(const Ray rays[8], const Scene& scene, int depth) const;
void render_primary_rays_simd(const Camera& cam, const Scene& scene,
                              std::vector<std::vector<Color>>& framebuffer,
                              int width, int height, int samples);
```

**Rationale:**
- Public API for packet-based rendering
- Separates SIMD path from scalar path
- Allows easy comparison/benchmarking

---

#### 1.2 Implement Ray Packet Tracing Function

**File:** `src/renderer/renderer.cpp`

**Implementation steps:**

```cpp
Color Renderer::ray_color_packet(const Ray rays[8], const Scene& scene, int depth) const {
    // 1. Create RayPacket from 8 rays
    RayPacket packet;
    for (int i = 0; i < 8; i++) {
        packet.set_ray(i, rays[i]);
    }

    // 2. Test intersection against all spheres (SIMD)
    PacketHitRecord hits;
    bool any_hit = false;

    for (const auto& sphere : scene.spheres) {
        PacketHitRecord sphere_hits;
        if (hit_sphere_simd(packet, sphere, sphere_hits)) {
            if (!any_hit || sphere_hits.t < hits.t) {
                hits = sphere_hits;
                any_hit = true;
            }
        }
    }

    // 3. Process hits (fall back to scalar for shading)
    std::vector<Color> colors(8);
    for (int i = 0; i < 8; i++) {
        if (hits.valid[i]) {
            // Scalar shading (individual hit point)
            HitRecord rec;
            rec.t = extract_float(hits.t, i);
            rec.p = extract_vec3(hits.positions, i);
            rec.normal = extract_vec3(hits.normals, i);
            rec.mat = scene.spheres[hit_sphere_idx].material;

            colors[i] = compute_phong_shading(rec, scene);
        } else {
            colors[i] = background_color(packet.get_direction(i));
        }
    }

    // 4. Handle reflections (scalar for now - future: packet reflections)
    // ... (similar to current ray_color logic)

    return colors;
}
```

**Key Decisions:**
- **Hybrid approach**: SIMD for intersection, scalar for shading
- **Rationale**: Shading is divergent (different materials, lights), intersection is coherent
- **Future optimization**: Packet-based shading for coherent paths

---

#### 1.3 Create SIMD Render Loop

**File:** `src/renderer/renderer.cpp`

**Implementation:**
```cpp
void Renderer::render_primary_rays_simd(const Camera& cam, const Scene& scene,
                                       std::vector<std::vector<Color>>& framebuffer,
                                       int width, int height, int samples) {
    Camera camera = cam.get_camera();

    // Process pixels in groups of 8 (4x2 pixel blocks)
    for (int j = 0; j < height; j += 2) {
        for (int i = 0; i < width; i += 4) {
            // Gather 8 camera rays
            Ray rays[8];
            int ray_idx = 0;

            for (int dy = 0; dy < 2 && j + dy < height; dy++) {
                for (int dx = 0; dx < 4 && i + dx < width; dx++) {
                    // Generate camera ray
                    float u = float(i + dx + random_float()) / (width - 1);
                    float v = float(j + dy + random_float()) / (height - 1);
                    rays[ray_idx++] = camera.get_ray(u, v);
                }
            }

            // Pad to 8 rays if needed (edge case)
            while (ray_idx < 8) {
                rays[ray_idx++] = rays[0];  // Duplicate first ray
            }

            // Trace packet
            std::vector<Color> colors = ray_color_packet(rays, scene, max_depth);

            // Scatter results back to framebuffer
            ray_idx = 0;
            for (int dy = 0; dy < 2 && j + dy < height; dy++) {
                for (int dx = 0; dx < 4 && i + dx < width; dx++) {
                    framebuffer[j + dy][i + dx] = colors[ray_idx++];
                }
            }
        }
    }
}
```

**Rationale:**
- Process 4x2 pixel blocks (8 rays = AVX2 width)
- Spatial coherence (adjacent pixels have similar rays)
- Handles edge cases (padding with duplicate rays)

---

#### 1.4 Add SIMD Toggle to Interactive Mode

**File:** `src/main_cpu_interactive.cpp`

**Add to ControlsPanel:**
```cpp
// In render() function parameters
bool enable_simd_packets = false,

// In button handling (category 14 = simd)
case 14:  // SIMD packets
    result.simd_packets_changed = true;
    result.new_simd_packets = button.value;
    break;
```

**Add to Renderer:**
```cpp
// In renderer.h
bool enable_simd_packets;  // Enable ray packet tracing

// In renderer.cpp constructor
enable_simd_packets = false;

// In render loop
if (enable_simd_packets) {
    render_primary_rays_simd(cam, scene, framebuffer, width, height, samples);
} else {
    // Scalar rendering (current code)
}
```

**UI Button:**
```cpp
// Phase 3 Optimizations section
const char* simd_label = enable_simd_packets ? "SIMD: ON" : "SIMD: OFF";
int simd_button_width = 120;
SDL_Rect simd_button_rect = {15, y_offset, simd_button_width, 24};
// ... (similar to other toggles)
```

---

#### 1.5 Utility Functions for SIMD

**File:** `src/renderer/renderer.cpp` (or new `src/utils/simd_utils.h`)

**Helper functions:**
```cpp
// Extract float from __m256 at index
inline float extract_float(__m256 v, int idx) {
    float result[8];
    _mm256_storeu_ps(result, v);
    return result[idx];
}

// Extract Vec3 from Vec3_AVX2 at index
inline Vec3 extract_vec3(Vec3_AVX2 v, int idx) {
    float x_arr[8], y_arr[8], z_arr[8];
    _mm256_storeu_ps(x_arr, v.x);
    _mm256_storeu_ps(y_arr, v.y);
    _mm256_storeu_ps(z_arr, v.z);
    return Vec3(x_arr[idx], y_arr[idx], z_arr[idx]);
}
```

---

### Testing Strategy

#### Test 1: Correctness
1. Render same scene with SIMD ON and OFF
2. Compare outputs pixel-by-pixel
3. Allow 0.1% tolerance for floating-point differences

#### Test 2: Performance
1. Benchmark at 800x450, 16 samples
2. Measure: wall time, MRays/sec
3. Compare SIMD vs Scalar

**Expected Results:**
- Primary rays: 4-6x speedup
- Overall: 10-20% improvement (due to scalar shadows/reflections)

---

### Risk Assessment

**Low Risk:**
- SIMD code already exists and tested
- Hybrid approach minimizes complexity

**Medium Risk:**
- Edge cases (odd resolutions, partial packets)
- Floating-point precision differences

**Mitigation:**
- Extensive testing at multiple resolutions
- Fallback to scalar for edge cases
- Visual comparison tools

---

## Task 2: Implement BVH Acceleration Structure

### Current State
- Linear scene traversal (test every sphere for every ray)
- O(n) complexity per ray (n = number of objects)
- Current scene: 10 spheres (fast enough)
- Scalability issue: 50+ objects would be slow

### Implementation Plan

#### 2.1 Design BVH Data Structures

**File:** `src/acceleration/bvh.h` (new)

```cpp
#ifndef BVH_H
#define BVH_H

#include "../math/ray.h"
#include "../primitives/primitive.h"
#include "../primitives/sphere.h"
#include <vector>
#include <memory>
#include <algorithm>

// Axis-Aligned Bounding Box
struct AABB {
    Vec3 min;
    Vec3 max;

    AABB() : min(infinity, infinity, infinity),
             max(-infinity, -infinity, -infinity) {}

    AABB(Vec3 min, Vec3 max) : min(min), max(max) {}

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

    // Test if ray hits AABB
    bool hit(const Ray& r, float t_min, float t_max) const;
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

    // Traverse BVH to find closest hit
    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec,
             const std::vector<std::shared_ptr<Sphere>>& spheres) const;

private:
    std::shared_ptr<BVHNode> build_recursive(std::vector<int> indices, int depth);
    int split_primitives(const std::vector<int>& indices, AABB& bbox);
};

#endif // BVH_H
```

---

#### 2.2 Implement AABB Intersection

**File:** `src/acceleration/bvh.cpp`

```cpp
bool AABB::hit(const Ray& r, float t_min, float t_max) const {
    // Slab method for AABB intersection
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
```

---

#### 2.3 Implement BVH Build

**File:** `src/acceleration/bvh.cpp`

```cpp
void BVH::build(const std::vector<std::shared_ptr<Sphere>>& spheres) {
    primitives = spheres;

    // Create leaf indices
    std::vector<int> indices(spheres.size());
    for (int i = 0; i < spheres.size(); i++) {
        indices[i] = i;
    }

    // Build tree recursively
    root = build_recursive(indices, 0);
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
```

---

#### 2.4 Implement BVH Traversal

**File:** `src/acceleration/bvh.cpp`

```cpp
bool BVH::hit(const Ray& r, float t_min, float t_max, HitRecord& rec,
             const std::vector<std::shared_ptr<Sphere>>& spheres) const {
    return hit_recursive(root, r, t_min, t_max, rec, spheres);
}

bool BVH::hit_recursive(std::shared_ptr<BVHNode> node,
                        const Ray& r, float t_min, float t_max,
                        HitRecord& rec,
                        const std::vector<std::shared_ptr<Sphere>>& spheres) const {
    // Check if ray hits node's bounding box
    if (!node->bbox.hit(r, t_min, t_max)) {
        return false;
    }

    // Leaf node: test primitives
    if (node->is_leaf) {
        bool hit_anything = false;
        float closest_so_far = t_max;

        for (int idx : node->primitive_indices) {
            HitRecord temp_rec;
            if (spheres[idx]->hit(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }
        return hit_anything;
    }

    // Internal node: recurse children
    HitRecord left_rec, right_rec;
    bool hit_left = hit_recursive(node->left, r, t_min, closest_so_far, left_rec, spheres);
    bool hit_right = hit_recursive(node->right, r, t_min, closest_so_far, right_rec, spheres);

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
```

---

#### 2.5 Integrate BVH into Scene

**File:** `src/scene/scene.h`

```cpp
#include "../acceleration/bvh.h"

class Scene {
    // ... existing members ...

    // BVH acceleration
    std::shared_ptr<BVH> bvh;
    bool enable_bvh;

public:
    // ... existing methods ...

    void build_bvh();
    bool hit_bvh(const Ray& r, float t_min, float t_max, HitRecord& rec) const;
};
```

**File:** `src/scene/scene.cpp`

```cpp
void Scene::build_bvh() {
    bvh = std::make_shared<BVH>();
    bvh->build(spheres);
    enable_bvh = true;
}

bool Scene::hit_bvh(const Ray& r, float t_min, float t_max, HitRecord& rec) const {
    if (!bvh) {
        return hit(r, t_min, t_max, rec);  // Fallback to linear
    }
    return bvh->hit(r, t_min, t_max, rec, spheres);
}
```

---

#### 2.6 Add BVH Toggle to Interactive Mode

**File:** `src/main_cpu_interactive.cpp`

**Similar to SIMD toggle:**
```cpp
// In ControlsPanel::render()
bool enable_bvh = false,

// Button handling
case 15:  // BVH
    result.bvh_changed = true;
    result.new_bvh = button.value;
    break;

// In main loop
if (click_result.bvh_changed) {
    enable_bvh = click_result.new_bvh;
    if (enable_bvh) {
        scene.build_bvh();
        std::cout << "BVH acceleration enabled" << std::endl;
    } else {
        std::cout << "BVH acceleration disabled" << std::endl;
    }
    need_render = true;
}

// In rendering
if (enable_bvh) {
    hit_result = scene.hit_bvh(ray, 0.001f, infinity, rec);
} else {
    hit_result = scene.hit(ray, 0.001f, infinity, rec);
}
```

**UI Button:**
```cpp
const char* bvh_label = enable_bvh ? "BVH: ON" : "BVH: OFF";
// ... (similar to other toggles)
```

---

### Testing Strategy

#### Test 1: Correctness
1. Build BVH for Cornell Box (10 spheres)
2. Render with BVH ON and OFF
3. Compare outputs (must be identical)

#### Test 2: Performance (Simple Scene)
1. Cornell Box (10 spheres)
2. Expected: Minimal improvement or slight slowdown (BVH overhead)

#### Test 3: Performance (Complex Scene)
1. Create test scene with 50-100 spheres
2. Benchmark BVH vs Linear
3. Expected: 20-40% improvement

#### Test 4: Scalability
1. Test with 10, 50, 100, 200 spheres
2. Plot performance vs. scene complexity
3. Verify O(log n) vs O(n) behavior

---

### Complex Scene Generation

**File:** `src/scene/stress_test_scene.h` (new)

```cpp
#ifndef STRESS_TEST_SCENE_H
#define STRESS_TEST_SCENE_H

Scene create_stress_test_scene(int num_spheres = 100) {
    Scene scene;

    // Add ground plane
    scene.add_sphere(std::make_shared<Sphere>(
        Point3(0, -1000, 0), 1000,
        std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5))
    ));

    // Add random spheres
    for (int i = 0; i < num_spheres; i++) {
        float x = random_float() * 20 - 10;
        float y = random_float() * 10;
        float z = random_float() * 20 - 10;
        float radius = random_float() * 0.5 + 0.2;

        Color color(random_float(), random_float(), random_float());
        std::shared_ptr<Material> mat;

        if (random_float() < 0.3) {
            mat = std::make_shared<Metal>(color, 0.0);
        } else {
            mat = std::make_shared<Lambertian>(color);
        }

        scene.add_sphere(std::make_shared<Sphere>(Point3(x, y, z), radius, mat));
    }

    // Add lights
    scene.add_light(Point3(0, 10, 0), Color(1, 1, 1));

    return scene;
}

#endif // STRESS_TEST_SCENE_H
```

---

### Risk Assessment

**Low Risk:**
- BVH is well-understood algorithm
- Can fallback to linear traversal

**Medium Risk:**
- BVH build time for large scenes
- Memory overhead
- Debugging tree structure

**Mitigation:**
- Start with small scenes (10-20 objects)
- Add debug visualization (tree depth, node counts)
- Profile both build and traversal

---

## Task 3: Benchmarking and Documentation

### Benchmarking Plan

#### Configuration 1: Default (960x540, 1 sample)
- Measure baseline (no SIMD, no BVH)
- Measure SIMD only
- Measure BVH only
- Measure SIMD + BVH

#### Configuration 2: High Quality (800x450, 16 samples)
- Same measurements
- Better statistical accuracy

#### Configuration 3: Stress Test (100+ spheres)
- Linear vs BVH
- SIMD impact on complex scenes

**Metrics to Record:**
- Wall clock time
- MRays/sec (throughput)
- Memory usage
- Build time (BVH)
- Visual quality ( screenshots)

---

### Documentation Updates

**Files to update:**
1. `docs/cpu-optimization-experiments.md`
   - Add Phase 3 results table
   - Update speedup calculations

2. `docs/interactive-controls-guide.md`
   - Document SIMD toggle
   - Document BVH toggle
   - Usage recommendations

3. `README.md`
   - Update performance numbers
   - Add Phase 3 features

4. `CHANGELOG.md`
   - Document Phase 3 implementation

---

## Implementation Order

### Step 1: SIMD Integration (2-3 hours)
1. ✅ Create in-depth plan (this document)
2. Add packet rendering to Renderer class (30 min)
3. Implement ray_color_packet function (1 hour)
4. Create SIMD render loop (30 min)
5. Add SIMD toggle to UI (30 min)
6. Test correctness (30 min)
7. Benchmark and document (30 min)

### Step 2: BVH Implementation (2-3 hours)
1. Create BVH data structures (30 min)
2. Implement AABB intersection (15 min)
3. Implement BVH build (1 hour)
4. Implement BVH traversal (30 min)
5. Integrate into Scene (15 min)
6. Add BVH toggle to UI (15 min)
7. Test correctness (30 min)
8. Create stress test scene (15 min)
9. Benchmark and document (30 min)

### Step 3: Final Integration (30 min)
1. Test SIMD + BVH together
2. Final benchmarking
3. Update all documentation
4. Commit changes

---

## Success Criteria

### SIMD Integration
- ✅ Correctness: Identical output to scalar (within floating-point tolerance)
- ✅ Performance: 10-20% improvement on default settings
- ✅ Interactive: Toggle works in real-time
- ✅ Robustness: Handles all edge cases (odd resolutions, etc.)

### BVH Implementation
- ✅ Correctness: Identical output to linear traversal
- ✅ Performance: 20-40% improvement on complex scenes (50+ objects)
- ✅ Scalability: O(log n) behavior demonstrated
- ✅ Interactive: Toggle works, BVH rebuilds when scene changes

### Documentation
- ✅ All benchmarks recorded
- ✅ Interactive controls documented
- ✅ Code commented and clear
- ✅ Commit history organized

---

## Notes

### SIMD Limitations
- Primary rays benefit most (coherent)
- Shadow rays still scalar (incoherent)
- Future: Packet-based shadow rays for area lights

### BVH Limitations
- Overhead for simple scenes (< 20 objects)
- Build time for dynamic scenes
- Memory usage increases

### Future Enhancements
- SIMD BVH traversal (test 8 nodes at once)
- Packet-based shadow rays
- Spatial splitting (better than object splitting)
- SAH (Surface Area Heuristic) for better splits

---

**Last Updated:** 2026-04-15
**Status:** Ready to implement
**Next Action:** Begin SIMD integration
