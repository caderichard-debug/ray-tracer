#ifndef SPHERE_SIMD_H
#define SPHERE_SIMD_H

#include "sphere.h"
#include "../math/vec3_avx2.h"
#include "../math/ray_packet.h"

// Geometry only (16 bytes): hot SIMD loop avoids shared_ptr traffic.
struct SphereSimdGeom {
    float cx, cy, cz, radius;
};

inline void sphere_simd_geom_hit_packet(const SphereSimdGeom& g, const RayPacket& packet, HitRecordPacket& hits) {
    const __m256 cx = _mm256_set1_ps(g.cx);
    const __m256 cy = _mm256_set1_ps(g.cy);
    const __m256 cz = _mm256_set1_ps(g.cz);
    const __m256 r_sq = _mm256_set1_ps(g.radius * g.radius);

    const __m256 oc_x = _mm256_sub_ps(packet.origins.x, cx);
    const __m256 oc_y = _mm256_sub_ps(packet.origins.y, cy);
    const __m256 oc_z = _mm256_sub_ps(packet.origins.z, cz);

    __m256 oc_dot = _mm256_mul_ps(oc_x, oc_x);
    oc_dot = _mm256_add_ps(oc_dot, _mm256_mul_ps(oc_y, oc_y));
    oc_dot = _mm256_add_ps(oc_dot, _mm256_mul_ps(oc_z, oc_z));
    const __m256 c = _mm256_sub_ps(oc_dot, r_sq);

    const __m256 b_half_x = _mm256_mul_ps(oc_x, packet.directions.x);
    const __m256 b_half_y = _mm256_mul_ps(oc_y, packet.directions.y);
    const __m256 b_half_z = _mm256_mul_ps(oc_z, packet.directions.z);
    const __m256 b_half = _mm256_add_ps(b_half_x, _mm256_add_ps(b_half_y, b_half_z));
    const __m256 b = _mm256_mul_ps(b_half, _mm256_set1_ps(2.0f));

    const __m256 b_sq = _mm256_mul_ps(b, b);
    const __m256 four_c = _mm256_mul_ps(_mm256_set1_ps(4.0f), c);
    const __m256 discriminant = _mm256_sub_ps(b_sq, four_c);

    const __m256 disc_mask = _mm256_cmp_ps(discriminant, _mm256_setzero_ps(), _CMP_GE_OQ);
    const __m256 sqrt_disc = _mm256_sqrt_ps(discriminant);

    const __m256 neg_b = _mm256_sub_ps(_mm256_setzero_ps(), b);
    const __m256 t1 = _mm256_mul_ps(_mm256_add_ps(neg_b, sqrt_disc), _mm256_set1_ps(0.5f));
    const __m256 t2 = _mm256_mul_ps(_mm256_sub_ps(neg_b, sqrt_disc), _mm256_set1_ps(0.5f));

    const __m256 t2_valid_low = _mm256_cmp_ps(t2, packet.t_min, _CMP_GE_OQ);
    const __m256 t2_valid_high = _mm256_cmp_ps(t2, packet.t_max, _CMP_LE_OQ);
    const __m256 t2_valid = _mm256_and_ps(t2_valid_low, t2_valid_high);
    const __m256 t_candidate = _mm256_blendv_ps(t1, t2, t2_valid);

    const __m256 t_valid_low = _mm256_cmp_ps(t_candidate, packet.t_min, _CMP_GE_OQ);
    const __m256 t_valid_high = _mm256_cmp_ps(t_candidate, packet.t_max, _CMP_LE_OQ);
    const __m256 t_valid = _mm256_and_ps(t_valid_low, t_valid_high);
    const __m256 hit_mask = _mm256_and_ps(disc_mask, t_valid);

    hits.t = t_candidate;
    hits.valid = hit_mask;

    const __m256 rad = _mm256_set1_ps(g.radius);
    const __m256 px = _mm256_add_ps(packet.origins.x, _mm256_mul_ps(t_candidate, packet.directions.x));
    const __m256 py = _mm256_add_ps(packet.origins.y, _mm256_mul_ps(t_candidate, packet.directions.y));
    const __m256 pz = _mm256_add_ps(packet.origins.z, _mm256_mul_ps(t_candidate, packet.directions.z));
    hits.positions = Vec3_AVX2(px, py, pz);
    hits.normals = Vec3_AVX2(_mm256_div_ps(_mm256_sub_ps(px, cx), rad),
                              _mm256_div_ps(_mm256_sub_ps(py, cy), rad),
                              _mm256_div_ps(_mm256_sub_ps(pz, cz), rad));
}

// SIMD ray-sphere intersection for 8 rays at once
class Sphere_SIMD {
public:
    Point3 center;
    float radius;
    std::shared_ptr<Material> mat;

    Sphere_SIMD() {}
    Sphere_SIMD(Point3 cen, float r, std::shared_ptr<Material> m)
        : center(cen), radius(r), mat(m) {}

    void hit_packet(const RayPacket& packet, HitRecordPacket& hits) const {
        sphere_simd_geom_hit_packet(SphereSimdGeom{center.x, center.y, center.z, radius}, packet, hits);
    }
};

#endif // SPHERE_SIMD_H
