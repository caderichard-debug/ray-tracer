#ifndef MORTON_H
#define MORTON_H

#include <cstdint>
#include <algorithm>

// Morton Z-order curve utilities for cache-friendly pixel traversal
// Improves spatial locality for better cache utilization

namespace Morton {

// Interleave bits to create Morton code
inline uint32_t interleave_bits(uint32_t x) {
    x = (x | (x << 16)) & 0x0000FFFF;
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;
    return x;
}

// Compute Morton code for 2D coordinates
inline uint32_t encode_2d(uint32_t x, uint32_t y) {
    return (interleave_bits(y) << 1) + interleave_bits(x);
}

// Decode Morton code to get x coordinate
inline uint32_t decode_x(uint32_t code) {
    return interleave_bits(code >> 0);
}

// Decode Morton code to get y coordinate
inline uint32_t decode_y(uint32_t code) {
    return interleave_bits(code >> 1);
}

// Pixel structure for Morton ordering
struct MortalPixel {
    uint32_t morton_code;
    uint32_t x;
    uint32_t y;

    bool operator<(const MortalPixel& other) const {
        return morton_code < other.morton_code;
    }
};

// Generate Morton-ordered pixel list for an image
inline std::vector<MortalPixel> generate_morton_pixels(int width, int height) {
    std::vector<MortalPixel> pixels;
    pixels.reserve(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t code = encode_2d(x, y);
            pixels.push_back({code, static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
        }
    }

    // Sort by Morton code for Z-order traversal
    std::sort(pixels.begin(), pixels.end());
    return pixels;
}

} // namespace Morton

#endif // MORTON_H
