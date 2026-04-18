# Path tracing mode — implementation plan

This document outlines how to add an optional **path tracing** mode alongside the existing Phong ray tracer, with performance and maintainability as first-class constraints.

## Goals

- **Unbiased** (or selectively biased with MIS) global illumination: diffuse interreflections, glossy lobes, and image-based lighting later.
- **Coexist** with current code: keep `Renderer::ray_color` Phong path as default; add `ray_color_path` or a `RenderMode` enum without breaking batch and interactive entry points.
- **Scale on CPU**: SIMD-friendly bounce loops, Russian roulette, next-event estimation (NEE) for lights, and optional **BVH** reuse for rays.

## Phase 1 — Foundations (minimal viable path tracer)

1. **Material extensions**  
   - Add a `bool is_specular_delta` / `MaterialType` branch where path tracing uses **only** BSDF sampling (no Phong hack).  
   - Lambert: cosine-weighted hemisphere. Metal: reflect with Fresnel (Schlick) as a first step.

2. **Integrator**  
   - Simple **forward path tracing**: at each bounce, sample BSDF, spawn ray, multiply throughput.  
   - **Russian roulette** after depth ≥ 2 to cap cost.

3. **Lights**  
   - Start with **point lights via NEE**: at each vertex, trace shadow ray to light and add contribution (same shadow infrastructure as today).  
   - Scene API: iterate existing `Light` list.

4. **RNG**  
   - Per-pixel/per-bounce **PCG** or **Sobol** blocks (you already have PCG headers in interactive); avoid `random_float()` atomics on hot path — use thread-local or `pixel_seed = hash(pixel, frame, bounce)`.

5. **Entry points**  
   - `--path-trace` on batch; interactive toggle “Path trace ON” that switches integrator branch.

**Performance**: keep max bounces low by default (3–5), RR start at 2, NEE only for small light count.

## Phase 2 — Quality and stability

1. **Next-event estimation + MIS**  
   - If you add **area/quad lights**, sample area + BSDF and combine with balance or power heuristic.

2. **HDR and tone mapping**  
   - Accumulate **linear** radiance in `Color` (or `float3`) with **many more spp** in batch; apply same tonemap/gamma as today at the end only.

3. **Denoising hook**  
   - Your bilateral/variance post stack applies to **any** RGB buffer; for path tracing, optionally enable only in interactive preview or final frame.

## Phase 3 — Geometry and scalability

1. **BVH** for camera rays and extension rays (already present for Phong — wire path tracer through same `trace_closest`).  
2. **Wavefront / tiled** accumulation if divergence becomes an issue (optional).  
3. **Disney / GGX** (advanced): add only after Lambert+metal path is stable.

## Testing

- Extend **regression** harness: path mode will be **stochastic** unless fixed-seed + single-thread; prefer **hash of fixed-spp batch** at tiny resolution with `ENABLE_OPENMP=0` and documented seed.  
- Golden hash rotation whenever integrator changes intentionally.

## Non-goals (for now)

- Full spectral rendering, volumetrics, and bidirectional / MLT path tracing — document as future work if needed.

## Summary

Ship a **toggleable path integrator** with NEE for point lights, RR, BVH hits, and fixed RNG discipline; layer MIS, area lights, and advanced BSDFs once the narrow path is fast and stable.
