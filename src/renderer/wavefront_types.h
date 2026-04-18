#ifndef WAVEFRONT_TYPES_H
#define WAVEFRONT_TYPES_H

#include "../math/ray.h"
#include "../math/vec3.h"

struct WavefrontRay {
    Ray ray;
    Color throughput;
    Color radiance;
    int pixel_x, pixel_y;
};

#endif // WAVEFRONT_TYPES_H
