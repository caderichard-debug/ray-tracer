#ifndef VEC3_AVX2_H
#define VEC3_AVX2_H

#include <immintrin.h>
#include "vec3.h"

// AVX2 (256-bit SIMD) optimized vector operations
// Can process 8 floats at once for improved performance

namespace AVX2 {

// AVX2-optimized dot product for Vec3
inline float dot_product_avx2(const Vec3& a, const Vec3& b) {
    __m128 va = _mm_set_ps(0.0f, a.z, a.y, a.x);
    __m128 vb = _mm_set_ps(0.0f, b.z, b.y, b.x);
    
    __m128 product = _mm_mul_ps(va, vb);
    product = _mm_hadd_ps(product, product);
    product = _mm_hadd_ps(product, product);
    
    return _mm_cvtss_f32(product);
}

// AVX2-optimized vector addition
inline Vec3 add_avx2(const Vec3& a, const Vec3& b) {
    __m128 va = _mm_set_ps(0.0f, a.z, a.y, a.x);
    __m128 vb = _mm_set_ps(0.0f, b.z, b.y, b.x);
    
    __m128 result = _mm_add_ps(va, vb);
    
    float temp[4];
    _mm_storeu_ps(temp, result);
    return Vec3(temp[0], temp[1], temp[2]);
}

// AVX2-optimized vector subtraction
inline Vec3 sub_avx2(const Vec3& a, const Vec3& b) {
    __m128 va = _mm_set_ps(0.0f, a.z, a.y, a.x);
    __m128 vb = _mm_set_ps(0.0f, b.z, b.y, b.x);
    
    __m128 result = _mm_sub_ps(va, vb);
    
    float temp[4];
    _mm_storeu_ps(temp, result);
    return Vec3(temp[0], temp[1], temp[2]);
}

// AVX2-optimized vector scale
inline Vec3 scale_avx2(const Vec3& v, float t) {
    __m128 va = _mm_set_ps(0.0f, v.z, v.y, v.x);
    __m128 vt = _mm_set1_ps(t);
    
    __m128 result = _mm_mul_ps(va, vt);
    
    float temp[4];
    _mm_storeu_ps(temp, result);
    return Vec3(temp[0], temp[1], temp[2]);
}

// AVX2-optimized normalize
inline Vec3 normalize_avx2(const Vec3& v) {
    float len_sq = dot_product_avx2(v, v);
    if (len_sq > 0) {
        float inv_len = 1.0f / std::sqrt(len_sq);
        return scale_avx2(v, inv_len);
    }
    return Vec3(0, 0, 0);
}

// AVX2-optimized reflection
inline Vec3 reflect_avx2(const Vec3& d, const Vec3& n) {
    float dot_dn = dot_product_avx2(d, n);
    Vec3 term = scale_avx2(n, 2.0f * dot_dn);
    return sub_avx2(d, term);
}

} // namespace AVX2

#endif // VEC3_AVX2_H
