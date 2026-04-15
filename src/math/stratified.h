#ifndef STRATIFIED_H
#define STRATIFIED_H

#include <vector>
#include <random>
#include <algorithm>
#include "pcg_random.h"

// Stratified sampling for faster Monte Carlo convergence
// Divides each pixel into a grid and samples once per grid cell (jittered)

namespace Stratified {

// Generate stratified samples for a pixel
// Returns a list of (u, v) coordinates in [0, 1] range
inline std::vector<std::pair<float, float>> generate_stratified_samples(int samples) {
    std::vector<std::pair<float, float>> result;
    result.reserve(samples);

    // Find grid size (closest to sqrt(samples))
    int grid_size = static_cast<int>(std::sqrt(samples));
    while (grid_size * grid_size < samples) {
        grid_size++;
    }

    // Generate jittered samples in each grid cell
    for (int j = 0; j < grid_size; ++j) {
        for (int i = 0; i < grid_size; ++i) {
            if (result.size() >= samples) break;

            // Grid cell boundaries
            float u0 = static_cast<float>(i) / grid_size;
            float v0 = static_cast<float>(j) / grid_size;
            float u1 = static_cast<float>(i + 1) / grid_size;
            float v1 = static_cast<float>(j + 1) / grid_size;

            // Jitter within the cell (use PCG for thread safety)
            float jitter_u = random_float_pcg() / grid_size;
            float jitter_v = random_float_pcg() / grid_size;

            float u = u0 + jitter_u;
            float v = v0 + jitter_v;

            result.push_back({u, v});
        }
    }

    // Shuffle samples to avoid grid artifacts
    std::shuffle(result.begin(), result.end(),
                 std::default_random_engine(std::random_device{}()));

    return result;
}

// Convenience function to get stratified sample for index
inline std::pair<float, float> get_stratified_sample(int index, int total_samples) {
    // Find grid size
    int grid_size = static_cast<int>(std::sqrt(total_samples));
    while (grid_size * grid_size < total_samples) {
        grid_size++;
    }

    // Calculate grid cell
    int i = index % grid_size;
    int j = index / grid_size;

    // Grid cell boundaries
    float u0 = static_cast<float>(i) / grid_size;
    float v0 = static_cast<float>(j) / grid_size;

    // Jitter within the cell
    float jitter_u = random_float_pcg() / grid_size;
    float jitter_v = random_float_pcg() / grid_size;

    return {u0 + jitter_u, v0 + jitter_v};
}

} // namespace Stratified

#endif // STRATIFIED_H