#ifndef RAY_PACKET_H
#define RAY_PACKET_H

#include "ray.h"
#include "vec3_avx2.h"

// Ray packet: 8 rays processed simultaneously with AVX2
// Used for coherent primary rays (camera rays)
struct RayPacket {
    Vec3_AVX2 origins;    // 8 ray origins
    Vec3_AVX2 directions; // 8 ray directions
    __m256 t_min;         // 8 t_min values
    __m256 t_max;         // 8 t_max values (intersection distances)
    int valid_mask;       // Bitmask of which rays are active
    int size;             // Number of valid rays (1-8)

    RayPacket() : valid_mask(0xFF), size(8) {
        t_min = _mm256_set1_ps(0.001f);
        t_max = _mm256_set1_ps(infinity);
    }

    // Set ray at index
    void set_ray(int index, const Ray& ray) {
        float x_arr[8] = {0}, y_arr[8] = {0}, z_arr[8] = {0};

        x_arr[index] = ray.origin().x;
        y_arr[index] = ray.origin().y;
        z_arr[index] = ray.origin().z;
        origins = Vec3_AVX2(_mm256_loadu_ps(x_arr),
                           _mm256_loadu_ps(y_arr),
                           _mm256_loadu_ps(z_arr));

        x_arr[index] = ray.direction().x;
        y_arr[index] = ray.direction().y;
        z_arr[index] = ray.direction().z;
        directions = Vec3_AVX2(_mm256_loadu_ps(x_arr),
                              _mm256_loadu_ps(y_arr),
                              _mm256_loadu_ps(z_arr));
    }

    // Load 8 rays from array
    void load_rays(const Ray rays[8]) {
        origins = Vec3_AVX2::load_AoS(&rays[0].origin());
        // Note: This is simplified - proper implementation would transpose data
        // For now, we'll use scalar conversion in practice
    }
};

// Hit record for ray packet
struct HitRecordPacket {
    __m256 t;              // 8 intersection distances
    Vec3_AVX2 positions;   // 8 hit positions
    Vec3_AVX2 normals;     // 8 hit normals
    __m256 valid;          // Which rays hit (0 or 1 per lane)
    int valid_mask;        // Bitmask of valid hits

    HitRecordPacket() {
        t = _mm256_set1_ps(infinity);
        valid = _mm256_setzero_ps();
        valid_mask = 0;
    }
};

#endif // RAY_PACKET_H
