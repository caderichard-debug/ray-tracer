#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include <immintrin.h>
#include "../math/vec3.h"

// Utility functions for SIMD optimizations
// Note: Full ray packet tracing with Vec3_AVX2 class not yet implemented
// Current implementation uses cache-friendly pixel blocking

namespace SIMDUtils {

// Extract float from __m256 at index
inline float extract_float(__m256 v, int idx) {
    float result[8];
    _mm256_storeu_ps(result, v);
    return result[idx];
}

} // namespace SIMDUtils

#endif // SIMD_UTILS_H
