# Phase 6: Polish

**Status:** ⏳ PENDING
**Build Target:** `make phase6`
**Dependencies:** [Phase 5](phase5-multithreading.md)

## Overview

Phase 6 adds production-ready features: PNG output, tone mapping, CLI arguments, and example scenes.

## Planned Features

### 1. PNG Output
- stb_image_write library
- Lossless compression
- Widespread compatibility
- 8-bit or 16-bit per channel

### 2. Tone Mapping
- Reinhard tone mapping
- ACES filmic tone mapping
- HDR to LDR conversion
- Exposure control

### 3. CLI Arguments
- Resolution (--width, --height)
- Samples (--samples)
- Max depth (--depth)
- Output file (--output)
- Scene selection (--scene)

### 4. Example Scenes
- Cornell box
- Spheres showcase
- Material comparison
- Performance benchmark

### 5. Benchmarking
- Time measurement
- Rays/sec calculation
- Memory usage
- Comparison tool

## Implementation Details

### stb_image_write
```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Write PNG
stbi_write_png("output.png", width, height, 3, data, stride);
```

### Reinhard Tone Mapping
```cpp
Color reinhard_tone_map(const Color& hdr) {
    return hdr / (Color(1, 1, 1) + hdr);
}
```

### CLI Arguments
```cpp
int main(int argc, char** argv) {
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--width") == 0) {
            width = atoi(argv[++i]);
        }
        // ...
    }
}
```

## Implementation Status

- ⏳ stb_image_write integration
- ⏳ Tone mapping operators
- ⏳ CLI argument parsing
- ⏳ Example scenes
- ⏳ Benchmarking tools

## Final Checklist

- [x] Phase 1: Foundation ✅
- [x] Phase 2: Basic Rendering ✅
- [🚧] Phase 3: SIMD Vectorization (demonstrated)
- [ ] Phase 4: Advanced Features
- [ ] Phase 5: Multi-threading
- [ ] Phase 6: Polish

## Project Completion Goals

When all phases are complete:
- ✅ Production-quality ray tracer
- ✅ 14-20x speedup over scalar baseline
- ✅ 20-40 MRays/sec performance
- ✅ PNG output with tone mapping
- ✅ Multiple example scenes
- ✅ Comprehensive documentation
- ✅ Benchmarking tools
