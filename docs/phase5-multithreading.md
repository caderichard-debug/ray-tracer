# Phase 5: Multi-threading

**Status:** ⏳ PENDING
**Build Target:** `make phase5`
**Dependencies:** [Phase 4](phase4-advanced.md)

## Overview

Phase 5 adds OpenMP multi-threading for parallel rendering across CPU cores.

## Planned Features

### 1. OpenMP Integration
- Tile-based parallelization
- Dynamic scheduling
- Thread-safe framebuffer

### 2. Load Balancing
- 32x32 pixel tiles
- Work stealing
- Near-linear scaling

### 3. Performance Targets
- 3.5-3.8x speedup on 4 cores
- Combined with SIMD: 14-20x total speedup
- Final: ~20-40 MRays/sec

## Implementation Details

### Tile-Based Rendering
```cpp
#pragma omp parallel for schedule(dynamic)
for (int tile_y = 0; tile_y < num_tiles_y; tile_y++) {
    for (int tile_x = 0; tile_x < num_tiles_x; tile_x++) {
        render_tile(tile_x, tile_y);
    }
}
```

### Thread Safety
- Scene graph is read-only
- Framebuffer writes are synchronized
- Per-thread random number generators

## Performance

| Phase | Speedup | MRays/sec |
|-------|---------|-----------|
| Phase 4 | 6-10x | 6-10 |
| Phase 5 | 14-20x | 20-40 |

## Implementation Status

- ⏳ OpenMP integration
- ⏳ Tile rendering
- ⏳ Thread safety
- ⏳ Benchmarking

## Next Steps

[→ Phase 6: Polish](phase6-polish.md)
