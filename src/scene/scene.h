#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <memory>
#include "../primitives/primitive.h"
#include "../primitives/sphere.h"
#include "../primitives/sphere_simd.h"
#include "../math/ray.h"
#include "../math/ray_packet.h"
#include "../math/frustum.h"
#include "light.h"

class Scene {
public:
    std::vector<std::shared_ptr<Primitive>> objects;
    std::vector<Light> lights;
    Color ambient_light;

    Scene() : ambient_light(0.1f, 0.1f, 0.1f), simd_cache_ready(false) {}

    bool simd_scene_cache_ready() const { return simd_cache_ready; }

    // Build the SIMD sphere cache and non-sphere list.
    // Call once (single-threaded) before a parallel render pass.
    void build_simd_cache() const {
        simd_geom_cache.clear();
        original_sphere_cache.clear();
        non_sphere_cache.clear();
        for (const auto& obj : objects) {
            auto sphere = std::dynamic_pointer_cast<Sphere>(obj);
            if (sphere) {
                simd_geom_cache.push_back(SphereSimdGeom{sphere->center.x, sphere->center.y, sphere->center.z,
                                                          sphere->radius});
                original_sphere_cache.push_back(sphere);
            } else {
                non_sphere_cache.push_back(obj);
            }
        }
        simd_cache_ready = true;
    }

    // Test non-sphere objects (triangles, quads) for the closest hit.
    // Used after SIMD sphere intersection to check if non-sphere geometry is closer.
    bool hit_non_spheres(const Ray& r, float t_min, float t_max, HitRecord& rec) const {
        bool hit_anything = false;
        float closest_so_far = t_max;
        for (const auto& obj : non_sphere_cache) {
            HitRecord temp;
            if (obj->hit(r, t_min, closest_so_far, temp)) {
                hit_anything = true;
                closest_so_far = temp.t;
                rec = temp;
            }
        }
        return hit_anything;
    }

    void add_object(std::shared_ptr<Primitive> obj) {
        objects.push_back(obj);
    }

    void add_light(const Light& light) {
        lights.push_back(light);
    }

    // Performance optimization: no-op for now (spatial sorting hurt perf)
    void optimize_spatial_layout() {
        // Spatial sorting removed - hurt performance due to cache misses
        // Keeping method for compatibility
    }

    // Check if ray hits any object in the scene (closest hit). When view_frustum is non-null, sphere
    // primitives fully outside the frustum are skipped (primary rays only — pass nullptr for bounces).
    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec, const Frustum::Frustum* view_frustum) const {
        bool hit_anything = false;
        float closest_so_far = t_max;

        for (const auto& object : objects) {
            if (view_frustum) {
                auto sp = std::dynamic_pointer_cast<Sphere>(object);
                if (sp && !view_frustum->is_sphere_inside(sp->center, sp->radius)) {
                    continue;
                }
            }
            HitRecord temp_rec;
            if (object->hit(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const { return hit(r, t_min, t_max, rec, nullptr); }

    // Check if any object blocks the shadow ray
    // Returns true if shadow ray is blocked
    bool is_shadowed(const Ray& shadow_ray, float t_max) const {
        HitRecord shadow_rec;
        const float t_min = 0.001f;

        for (const auto& object : objects) {
            if (object->hit(shadow_ray, t_min, t_max, shadow_rec)) {
                return true;  // Early exit on first hit
            }
        }
        return false;
    }

    // SIMD packet intersection: tests spheres using AVX2.
    // Call build_simd_cache() once before using this in a parallel loop.
    bool hit_packet(const RayPacket& packet,
                    float t_min, float t_max, std::vector<HitRecord>& hit_records) const {
        hit_records.resize(8);
        for (int i = 0; i < 8; i++) {
            hit_records[i].t = infinity;
            hit_records[i].mat = nullptr;
        }

        const std::vector<SphereSimdGeom>* simd_geoms_ptr;
        const std::vector<std::shared_ptr<Sphere>>* original_spheres_ptr;
        std::vector<SphereSimdGeom> temp_geom;
        std::vector<std::shared_ptr<Sphere>> temp_orig;
        if (simd_cache_ready) {
            simd_geoms_ptr = &simd_geom_cache;
            original_spheres_ptr = &original_sphere_cache;
        } else {
            for (const auto& obj : objects) {
                auto sphere = std::dynamic_pointer_cast<Sphere>(obj);
                if (sphere) {
                    temp_geom.push_back(SphereSimdGeom{sphere->center.x, sphere->center.y, sphere->center.z,
                                                       sphere->radius});
                    temp_orig.push_back(sphere);
                }
            }
            simd_geoms_ptr = &temp_geom;
            original_spheres_ptr = &temp_orig;
        }
        const auto& simd_geoms = *simd_geoms_ptr;
        const auto& original_spheres = *original_spheres_ptr;

        if (simd_geoms.empty()) {
            // No spheres, fall back to scalar for each ray
            bool any_hit = false;

            // Extract rays from packet
            float ox_arr[8], oy_arr[8], oz_arr[8];
            float dx_arr[8], dy_arr[8], dz_arr[8];
            _mm256_storeu_ps(ox_arr, packet.origins.x);
            _mm256_storeu_ps(oy_arr, packet.origins.y);
            _mm256_storeu_ps(oz_arr, packet.origins.z);
            _mm256_storeu_ps(dx_arr, packet.directions.x);
            _mm256_storeu_ps(dy_arr, packet.directions.y);
            _mm256_storeu_ps(dz_arr, packet.directions.z);

            for (int i = 0; i < 8; i++) {
                Vec3 origin(ox_arr[i], oy_arr[i], oz_arr[i]);
                Vec3 dir(dx_arr[i], dy_arr[i], dz_arr[i]);
                Ray ray(origin, dir);

                if (hit(ray, t_min, t_max, hit_records[i])) {
                    any_hit = true;
                }
            }
            return any_hit;
        }

        // Track closest hits
        __m256 closest_t = _mm256_set1_ps(t_max);
        int hit_sphere_indices[8];
        for (int i = 0; i < 8; i++) {
            hit_sphere_indices[i] = -1;  // Properly initialize all 8 elements
        }

        for (size_t sphere_idx = 0; sphere_idx < simd_geoms.size(); sphere_idx++) {
            HitRecordPacket packet_hits;
            sphere_simd_geom_hit_packet(simd_geoms[sphere_idx], packet, packet_hits);

            // Check which rays have closer hits
            __m256 is_closer = _mm256_and_ps(packet_hits.valid,
                                             _mm256_cmp_ps(packet_hits.t, closest_t, _CMP_LT_OQ));

            int hit_mask = _mm256_movemask_ps(is_closer);
            if (hit_mask) {
                float t_arr[8];
                _mm256_storeu_ps(t_arr, packet_hits.t);

                // Only update closest_t for lanes with a valid closer hit.
                // _mm256_min_ps would corrupt inactive lanes with negative/garbage t values
                // (e.g. from spheres behind the camera), breaking all subsequent comparisons.
                closest_t = _mm256_blendv_ps(closest_t, packet_hits.t, is_closer);

                for (int i = 0; i < 8; i++) {
                    if (hit_mask & (1 << i)) {
                        hit_sphere_indices[i] = sphere_idx;
                        hit_records[i].t = t_arr[i];
                    }
                }
            }
        }

        // Assign materials after all spheres tested to ensure correct sphere-per-ray pairing
        for (int i = 0; i < 8; i++) {
            if (hit_sphere_indices[i] >= 0) {
                hit_records[i].mat = original_spheres[hit_sphere_indices[i]]->mat;
            }
        }

        // Compute position and normal for hits (extract ray data from packet to ensure consistency)
        // OPTIMIZATION: Extract ray data ONCE outside the loop instead of 8 times
        float ox_arr[8], oy_arr[8], oz_arr[8];
        float dx_arr[8], dy_arr[8], dz_arr[8];
        _mm256_storeu_ps(ox_arr, packet.origins.x);
        _mm256_storeu_ps(oy_arr, packet.origins.y);
        _mm256_storeu_ps(oz_arr, packet.origins.z);
        _mm256_storeu_ps(dx_arr, packet.directions.x);
        _mm256_storeu_ps(dy_arr, packet.directions.y);
        _mm256_storeu_ps(dz_arr, packet.directions.z);

        for (int i = 0; i < 8; i++) {
            if (hit_sphere_indices[i] >= 0) {
                int sphere_idx = hit_sphere_indices[i];
                const Sphere* sphere = original_spheres[sphere_idx].get();

                Vec3 ray_origin(ox_arr[i], oy_arr[i], oz_arr[i]);
                Vec3 ray_dir(dx_arr[i], dy_arr[i], dz_arr[i]);

                // Manually compute hit position: P = origin + t * direction
                hit_records[i].p = ray_origin + hit_records[i].t * ray_dir;

                // Manually compute outward normal: (P - center) / radius
                Vec3 outward_normal = (hit_records[i].p - sphere->center) / sphere->radius;

                // CRITICAL: Normalize the normal! set_face_normal() doesn't normalize
                outward_normal = unit_vector(outward_normal);

                // Set face normal (determines front/back face)
                Ray ray_for_normal(ray_origin, ray_dir);
                hit_records[i].set_face_normal(ray_for_normal, outward_normal);

                // Material already set by SIMD pass
            }
        }

        // Check if any ray hit
        for (int i = 0; i < 8; i++) {
            if (hit_records[i].t < infinity) {
                return true;
            }
        }
        return false;
    }
private:
    mutable std::vector<SphereSimdGeom> simd_geom_cache;
    mutable std::vector<std::shared_ptr<Sphere>> original_sphere_cache;
    mutable std::vector<std::shared_ptr<Primitive>> non_sphere_cache;
    mutable bool simd_cache_ready;
};

#endif // SCENE_H
