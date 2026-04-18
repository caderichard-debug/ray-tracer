#ifndef SPHERE_SIMD_H
#define SPHERE_SIMD_H

#include "sphere.h"
#include "../math/vec3_avx2.h"
#include "../math/ray_packet.h"

// SIMD ray-sphere intersection for 8 rays at once
class Sphere_SIMD {
public:
    Point3 center;
    float radius;
    std::shared_ptr<Material> mat;

    Sphere_SIMD() {}
    Sphere_SIMD(Point3 cen, float r, std::shared_ptr<Material> m)
        : center(cen), radius(r), mat(m) {}

    // Vectorized intersection test for ray packet
    // Returns hit record with results for all 8 rays
    void hit_packet(const RayPacket& packet, HitRecordPacket& hits) const {
        // For each ray in packet, solve quadratic equation
        // |origin + t*direction - center|² = radius²

        // Broadcast sphere center to all lanes
        __m256 cx = _mm256_set1_ps(center.x);
        __m256 cy = _mm256_set1_ps(center.y);
        __m256 cz = _mm256_set1_ps(center.z);

        __m256 r_sq = _mm256_set1_ps(radius * radius);

        // Calculate oc = origin - center for all rays
        __m256 oc_x = _mm256_sub_ps(packet.origins.x, cx);
        __m256 oc_y = _mm256_sub_ps(packet.origins.y, cy);
        __m256 oc_z = _mm256_sub_ps(packet.origins.z, cz);

        // Quadratic coefficients: a*t² + b*t + c = 0
        // a = dot(direction, direction) = 1 (if normalized)
        // b = 2*dot(oc, direction)
        // c = dot(oc, oc) - radius²

        // Calculate c = |oc|² - r²
        __m256 oc_dot = _mm256_mul_ps(oc_x, oc_x);
        oc_dot = _mm256_add_ps(oc_dot, _mm256_mul_ps(oc_y, oc_y));
        oc_dot = _mm256_add_ps(oc_dot, _mm256_mul_ps(oc_z, oc_z));
        __m256 c = _mm256_sub_ps(oc_dot, r_sq);

        // Calculate b = 2*dot(oc, direction)
        __m256 b_half_x = _mm256_mul_ps(oc_x, packet.directions.x);
        __m256 b_half_y = _mm256_mul_ps(oc_y, packet.directions.y);
        __m256 b_half_z = _mm256_mul_ps(oc_z, packet.directions.z);
        __m256 b_half = _mm256_add_ps(b_half_x, _mm256_add_ps(b_half_y, b_half_z));
        __m256 b = _mm256_mul_ps(b_half, _mm256_set1_ps(2.0f));

        // Discriminant: b² - 4ac (simplified to b² - 4c since a=1)
        __m256 b_sq = _mm256_mul_ps(b, b);
        __m256 four_c = _mm256_mul_ps(_mm256_set1_ps(4.0f), c);
        __m256 discriminant = _mm256_sub_ps(b_sq, four_c);

        // Check if discriminant >= 0 (ray hits sphere)
        __m256 disc_mask = _mm256_cmp_ps(discriminant, _mm256_setzero_ps(), _CMP_GE_OQ);

        // Calculate sqrt of discriminant (only where valid)
        __m256 sqrt_disc = _mm256_sqrt_ps(discriminant);

        // Two solutions: t = (-b ± sqrt(discriminant)) / 2a
        // Since a=1: t = (-b ± sqrt_disc) / 2
        __m256 neg_b = _mm256_sub_ps(_mm256_setzero_ps(), b);
        __m256 t1 = _mm256_mul_ps(_mm256_add_ps(neg_b, sqrt_disc), _mm256_set1_ps(0.5f));
        __m256 t2 = _mm256_mul_ps(_mm256_sub_ps(neg_b, sqrt_disc), _mm256_set1_ps(0.5f));

        // t1 = larger root (exit/far intersection): (-b + sqrt_disc) / 2
        // t2 = smaller root (entry/near intersection): (-b - sqrt_disc) / 2
        // Always try the smaller root (entry point) first, fall back to larger only if out of range.

        // Check if t2 (smaller/near root) is in valid range [t_min, t_max]
        __m256 t2_valid_low = _mm256_cmp_ps(t2, packet.t_min, _CMP_GE_OQ);
        __m256 t2_valid_high = _mm256_cmp_ps(t2, packet.t_max, _CMP_LE_OQ);
        __m256 t2_valid = _mm256_and_ps(t2_valid_low, t2_valid_high);

        // Use t2 (entry point) when valid, fall back to t1 (exit point) otherwise
        __m256 t_candidate = _mm256_blendv_ps(t1, t2, t2_valid);  // If t2_valid, use t2; else t1

        // Check if the selected t is actually in valid range
        __m256 t_valid_low = _mm256_cmp_ps(t_candidate, packet.t_min, _CMP_GE_OQ);
        __m256 t_valid_high = _mm256_cmp_ps(t_candidate, packet.t_max, _CMP_LE_OQ);
        __m256 t_valid = _mm256_and_ps(t_valid_low, t_valid_high);

        // Combine with discriminant check
        __m256 hit_mask = _mm256_and_ps(disc_mask, t_valid);

        // Store results
        hits.t = t_candidate;
        hits.valid = hit_mask;

        // Calculate hit positions and normals (only for valid hits)
        // P(t) = origin + t*direction
        __m256 px = _mm256_add_ps(packet.origins.x, _mm256_mul_ps(t_candidate, packet.directions.x));
        __m256 py = _mm256_add_ps(packet.origins.y, _mm256_mul_ps(t_candidate, packet.directions.y));
        __m256 pz = _mm256_add_ps(packet.origins.z, _mm256_mul_ps(t_candidate, packet.directions.z));
        hits.positions = Vec3_AVX2(px, py, pz);

        // Normal = (P - center) / radius
        __m256 nx = _mm256_div_ps(_mm256_sub_ps(px, cx), _mm256_set1_ps(radius));
        __m256 ny = _mm256_div_ps(_mm256_sub_ps(py, cy), _mm256_set1_ps(radius));
        __m256 nz = _mm256_div_ps(_mm256_sub_ps(pz, cz), _mm256_set1_ps(radius));
        hits.normals = Vec3_AVX2(nx, ny, nz);
    }
};

#endif // SPHERE_SIMD_H
