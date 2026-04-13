#ifndef VEC3_AVX2_H
#define VEC3_AVX2_H

#include <immintrin.h>
#include <cmath>

// AVX2 Vec3 - operates on 8 Vec3s simultaneously using __m256
// Memory layout: Structure of Arrays (SoA)
// This means we store 8 x-coordinates, 8 y-coordinates, 8 z-coordinates
// instead of 8 complete Vec3s with x,y,z interleaved
class Vec3_AVX2 {
public:
    __m256 x, y, z;  // 8 floats each = 24 floats total

    // Constructors
    Vec3_AVX2() : x(_mm256_setzero_ps()), y(_mm256_setzero_ps()), z(_mm256_setzero_ps()) {}

    Vec3_AVX2(__m256 x, __m256 y, __m256 z) : x(x), y(y), z(z) {}

    // Broadcast same value to all 8 lanes
    Vec3_AVX2(float value) {
        x = y = z = _mm256_set1_ps(value);
    }

    // Vec3 operations
    Vec3_AVX2 operator+(const Vec3_AVX2& v) const {
        return Vec3_AVX2(
            _mm256_add_ps(x, v.x),
            _mm256_add_ps(y, v.y),
            _mm256_add_ps(z, v.z)
        );
    }

    Vec3_AVX2 operator-(const Vec3_AVX2& v) const {
        return Vec3_AVX2(
            _mm256_sub_ps(x, v.x),
            _mm256_sub_ps(y, v.y),
            _mm256_sub_ps(z, v.z)
        );
    }

    Vec3_AVX2 operator*(const Vec3_AVX2& v) const {
        return Vec3_AVX2(
            _mm256_mul_ps(x, v.x),
            _mm256_mul_ps(y, v.y),
            _mm256_mul_ps(z, v.z)
        );
    }

    Vec3_AVX2 operator*(float t) const {
        __m256 tv = _mm256_set1_ps(t);
        return Vec3_AVX2(
            _mm256_mul_ps(x, tv),
            _mm256_mul_ps(y, tv),
            _mm256_mul_ps(z, tv)
        );
    }

    Vec3_AVX2 operator/(float t) const {
        __m256 tv = _mm256_set1_ps(t);
        return Vec3_AVX2(
            _mm256_div_ps(x, tv),
            _mm256_div_ps(y, tv),
            _mm256_div_ps(z, tv)
        );
    }

    // Dot product: returns 8 dot products (one per lane)
    __m256 dot(const Vec3_AVX2& v) const {
        // dot = x*x + y*y + z*z
        __m256 xx = _mm256_mul_ps(x, v.x);
        __m256 yy = _mm256_mul_ps(y, v.y);
        __m256 zz = _mm256_mul_ps(z, v.z);

        // Horizontal sum: add xx+yy+zz for each lane
        __m256 temp1 = _mm256_hadd_ps(xx, yy);  // [x0+x1, x2+x3, y0+y1, y2+y3, ...]
        __m256 temp2 = _mm256_hadd_ps(temp1, temp1);  // Accumulate
        __m256 result = _mm256_hadd_ps(temp2, zz);

        return result;
    }

    // Length squared (no sqrt for performance when possible)
    __m256 length_squared() const {
        return dot(*this);
    }

    // Length (with sqrt)
    __m256 length() const {
        __m256 ls = length_squared();
        return _mm256_sqrt_ps(ls);
    }

    // Extract individual results (lane index 0-7)
    float get_x(int lane) const {
        float result[8];
        _mm256_storeu_ps(result, x);
        return result[lane];
    }

    float get_y(int lane) const {
        float result[8];
        _mm256_storeu_ps(result, y);
        return result[lane];
    }

    float get_z(int lane) const {
        float result[8];
        _mm256_storeu_ps(result, z);
        return result[lane];
    }

    // Static helper: load 8 Vec3s into Vec3_AVX2
    static Vec3_AVX2 load_8_vec3(const float* x_ptr, const float* y_ptr, const float* z_ptr) {
        return Vec3_AVX2(
            _mm256_loadu_ps(x_ptr),
            _mm256_loadu_ps(y_ptr),
            _mm256_loadu_ps(z_ptr)
        );
    }

    // Static helper: load from array of 8 Vec3s (AoS layout)
    // Assumes input is array of 8 Vec3s: [x0,y0,z0, x1,y1,z1, ...]
    static Vec3_AVX2 load_AoS(const Vec3* vecs) {
        // Transpose AoS to SoA
        // Input:  [x0,y0,z0, x1,y1,z1, x2,y2,z2, x3,y3,z3, ...]
        // Output: [x0,x1,x2,x3, x4,x5,x6,x7] in x register

        // Load 8 Vec3s (24 floats)
        // This is complex - for simplicity, we'll extract component-wise
        float x_arr[8], y_arr[8], z_arr[8];
        for (int i = 0; i < 8; i++) {
            x_arr[i] = vecs[i].x;
            y_arr[i] = vecs[i].y;
            z_arr[i] = vecs[i].z;
        }

        return Vec3_AVX2(
            _mm256_loadu_ps(x_arr),
            _mm256_loadu_ps(y_arr),
            _mm256_loadu_ps(z_arr)
        );
    }
};

// Utility functions
inline Vec3_AVX2 operator*(float t, const Vec3_AVX2& v) {
    return v * t;
}

// Comparison for min/max operations
inline __m256 vmin_ps(__m256 a, __m256 b) {
    return _mm256_min_ps(a, b);
}

inline __m256 vmax_ps(__m256 a, __m256 b) {
    return _mm256_max_ps(a, b);
}

#endif // VEC3_AVX2_H
