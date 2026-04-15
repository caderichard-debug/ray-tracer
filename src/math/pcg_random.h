#ifndef PCG_RANDOM_H
#define PCG_RANDOM_H

#include <cstdint>
#include <random>

// PCG (Permuted Congruential Generator) - High quality, fast PRNG
// Better statistical properties than XOR-shift, thread-safe with per-thread state
// Reference: http://www.pcg-random.org/

class PCGRandom {
private:
    uint64_t state;
    uint64_t inc;

public:
    // Constructor with seed
    explicit PCGRandom(uint64_t seed = 0x853c49e6748fea9bULL,
                      uint64_t seq = 0xda3e39cb94b95bdbULL)
        : state(0U), inc(seq | 1ULL) {
        // Initialize state
        (*this)();
        state += seed;
        (*this)();
    }

    // Generate random uint32_t
    uint32_t operator()() {
        uint64_t oldstate = state;
        state = oldstate * 6364136223846793005ULL + inc;
        uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = oldstate >> 59u;
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    // Generate random float in [0, 1)
    float random_float() {
        // Generate 23 bits of mantissa (for float precision)
        uint32_t bits = (*this) >> 9;  // Use upper bits for better distribution
        return bits * (1.0f / 8388608.0f);  // Divide by 2^23
    }

    // Generate random float in [min, max)
    float random_range(float min, float max) {
        return min + random_float() * (max - min);
    }

    // Thread-local static instance
    static PCGRandom& thread_local_instance() {
        static thread_local PCGRandom instance(
            0x853c49e6748fea9bULL,
            0xda3e39cb94b95bdbULL + std::hash<std::thread::id>{}(std::this_thread::get_id())
        );
        return instance;
    }
};

// Convenience function for quick random float
inline float random_float_pcg() {
    return PCGRandom::thread_local_instance().random_float();
}

// Fallback to rand() if PCG is not enabled
#ifndef ENABLE_FAST_RNG
    #define RANDOM_FLOAT() (static_cast<float>(rand()) / static_cast<float>(RAND_MAX))
#else
    #define RANDOM_FLOAT() random_float_pcg()
#endif

#endif // PCG_RANDOM_H
