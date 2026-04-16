#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <memory>
#include <fstream>
#include "../primitives/primitive.h"
#include "../primitives/sphere.h"
#include "../primitives/sphere_simd.h"
#include "../math/ray.h"
#include "../math/ray_packet.h"
#include "light.h"

class Scene {
public:
    std::vector<std::shared_ptr<Primitive>> objects;
    std::vector<Light> lights;
    Color ambient_light; // Ambient illumination

    Scene() : ambient_light(0.1f, 0.1f, 0.1f) {}

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

    // Check if ray hits any object in the scene
    // Returns the closest hit (smallest t)
    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const {
        bool hit_anything = false;
        float closest_so_far = t_max;

        for (const auto& object : objects) {
            HitRecord temp_rec;
            if (object->hit(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }

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

    // SIMD packet intersection: test 8 rays against all scene objects
    // Returns vector of 8 HitRecords (one per ray)
    bool hit_packet(const RayPacket& packet,
                    float t_min, float t_max, std::vector<HitRecord>& hit_records) const {
        hit_records.resize(8);
        for (int i = 0; i < 8; i++) {
            hit_records[i].t = infinity;
            hit_records[i].mat = nullptr;  // Initialize to null
        }

        // Extract spheres from scene for SIMD processing
        std::vector<Sphere_SIMD> simd_spheres;
        std::vector<std::shared_ptr<Sphere>> original_spheres;
        for (const auto& obj : objects) {
            auto sphere = std::dynamic_pointer_cast<Sphere>(obj);
            if (sphere) {
                simd_spheres.push_back(Sphere_SIMD(sphere->center, sphere->radius, sphere->mat));
                original_spheres.push_back(sphere);
            }
        }

        if (simd_spheres.empty()) {
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

        // Test ray packet against each sphere using SIMD
        int total_simd_hits = 0;
        for (size_t sphere_idx = 0; sphere_idx < simd_spheres.size(); sphere_idx++) {
            HitRecordPacket packet_hits;
            simd_spheres[sphere_idx].hit_packet(packet, packet_hits);

            // Check which rays have closer hits
            __m256 is_closer = _mm256_and_ps(packet_hits.valid,
                                             _mm256_cmp_ps(packet_hits.t, closest_t, _CMP_LT_OQ));

            int hit_mask = _mm256_movemask_ps(is_closer);
            if (hit_mask) {
                total_simd_hits += __builtin_popcount(hit_mask);

                // FIX: Extract t-values from packet_hits BEFORE updating closest_t
                // This ensures each ray gets the correct t-value from the correct sphere
                float t_arr[8];
                _mm256_storeu_ps(t_arr, packet_hits.t);

                // Update closest t values for next iteration
                closest_t = _mm256_min_ps(closest_t, packet_hits.t);

                // Track which sphere each ray hit and assign correct t-value
                for (int i = 0; i < 8; i++) {
                    if (hit_mask & (1 << i)) {
                        hit_sphere_indices[i] = sphere_idx;
                        hit_records[i].t = t_arr[i];
                        // NOTE: Material will be set AFTER all spheres are tested
                    }
                }
            }
        }

        // Set materials based on which sphere each ray hit (FIX: Set materials AFTER all spheres tested)
        // Debug: log material assignments for all rays
        static bool debug_mat_assign_once = true;
        if (debug_mat_assign_once) {
            std::ofstream debug_log("simd_debug.log", std::ios::app);
            debug_log << "SIMD: Material assignments (detailed):" << std::endl;
            for (int i = 0; i < 8; i++) {
                if (hit_sphere_indices[i] >= 0) {
                    // Log BEFORE assignment
                    debug_log << "  Ray " << i << ": sphere_idx=" << hit_sphere_indices[i];
                    debug_log << " original_sphere_mat_albedo=("
                              << original_spheres[hit_sphere_indices[i]]->mat->albedo.x << ","
                              << original_spheres[hit_sphere_indices[i]]->mat->albedo.y << ","
                              << original_spheres[hit_sphere_indices[i]]->mat->albedo.z << ")";
                }
            }
            debug_log << std::endl;

            // Now assign materials
            for (int i = 0; i < 8; i++) {
                if (hit_sphere_indices[i] >= 0) {
                    hit_records[i].mat = original_spheres[hit_sphere_indices[i]]->mat;
                }
            }

            // Log AFTER assignment
            debug_log << "SIMD: After material assignment:" << std::endl;
            for (int i = 0; i < 8; i++) {
                debug_log << "  Ray " << i << ": sphere_idx=" << hit_sphere_indices[i];
                if (hit_records[i].mat) {
                    debug_log << " hit_record_mat_albedo=("
                              << hit_records[i].mat->albedo.x << ","
                              << hit_records[i].mat->albedo.y << ","
                              << hit_records[i].mat->albedo.z << ")";
                    // Verify they match
                    if (hit_sphere_indices[i] >= 0) {
                        auto& orig_albedo = original_spheres[hit_sphere_indices[i]]->mat->albedo;
                        auto& hit_albedo = hit_records[i].mat->albedo;
                        if (orig_albedo.x != hit_albedo.x ||
                            orig_albedo.y != hit_albedo.y ||
                            orig_albedo.z != hit_albedo.z) {
                            debug_log << " MISMATCH!";
                        }
                    }
                } else {
                    debug_log << " mat=nullptr";
                }
                debug_log << std::endl;
            }
            debug_log.close();
            debug_mat_assign_once = false;
        } else {
            // Non-debug version: just assign materials
            for (int i = 0; i < 8; i++) {
                if (hit_sphere_indices[i] >= 0) {
                    hit_records[i].mat = original_spheres[hit_sphere_indices[i]]->mat;
                }
            }
        }

        // Compute position and normal for hits (extract ray data from packet to ensure consistency)
        for (int i = 0; i < 8; i++) {
            if (hit_sphere_indices[i] >= 0) {
                int sphere_idx = hit_sphere_indices[i];
                const Sphere* sphere = original_spheres[sphere_idx].get();

                // Extract origin and direction from packet (not scalar_rays, for consistency)
                float ox_arr[8], oy_arr[8], oz_arr[8];
                float dx_arr[8], dy_arr[8], dz_arr[8];
                _mm256_storeu_ps(ox_arr, packet.origins.x);
                _mm256_storeu_ps(oy_arr, packet.origins.y);
                _mm256_storeu_ps(oz_arr, packet.origins.z);
                _mm256_storeu_ps(dx_arr, packet.directions.x);
                _mm256_storeu_ps(dy_arr, packet.directions.y);
                _mm256_storeu_ps(dz_arr, packet.directions.z);

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

        static bool debug_simd_once = true;
        if (debug_simd_once) {
            std::ofstream debug_log("simd_debug.log", std::ios::app);
            debug_log << "SIMD: Computed position/normal for hits" << std::endl;
            for (int i = 0; i < 8; i++) {
                if (hit_sphere_indices[i] >= 0) {
                    debug_log << "  Ray " << i << ": t=" << hit_records[i].t
                              << " p=(" << hit_records[i].p.x << "," << hit_records[i].p.y << "," << hit_records[i].p.z << ")"
                              << " n=(" << hit_records[i].normal.x << "," << hit_records[i].normal.y << "," << hit_records[i].normal.z << ")"
                              << " mat=" << (hit_records[i].mat ? "yes" : "no")
                              << " front_face=" << hit_records[i].front_face << std::endl;
                }
            }
            debug_log.close();
            debug_simd_once = false;
        }

        // Check if any ray hit
        for (int i = 0; i < 8; i++) {
            if (hit_records[i].t < infinity) {
                return true;
            }
        }
        return false;
    }
};

#endif // SCENE_H
