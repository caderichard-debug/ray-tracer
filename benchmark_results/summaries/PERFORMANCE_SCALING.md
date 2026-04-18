# Performance scaling reference

Use this file to keep **comparable** numbers across hardware generations. Prefer copying tables out of a fresh `bench_feature_deltas` run with `--sweep` (see `docs/benchmarking.md`), then trimming columns you do not need.

## How to generate

Scaling deltas are written automatically with `./benchmark_feature_deltas.sh` or `./build/bench_feature_deltas` (omit **`--no-sweep`**).

```bash
cd ray-tracer
./build/bench_feature_deltas -r 5 --warmup 2 --markdown-out benchmark_results/summaries/feature_deltas_$(date +%Y%m%d_%H%M%S).md
```

Set thread cap explicitly when comparing machines:

```bash
./build/bench_feature_deltas --threads 8 --sweep --markdown-out benchmark_results/summaries/sweep_8t.md
```

## Machine record (fill in)

| Date | CPU / SoC | RAM | Compiler | Notes |
|------|-----------|-----|----------|-------|
| | | | | |

## Resolution (fixed SPP)

Reference column is **640×360** at the chosen SPP. Negative *% time* vs 640w means faster than that row.

| Width | Height | Median (s) | vs 640w | MRays/s |
|------:|-------:|-----------:|---------|--------:|
| | | | | |

## Samples per pixel (fixed 640×360)

Reference is **SPP = 4** unless you change the sweep source in `bench_feature_deltas.cpp`.

| SPP | Median (s) | vs spp=4 | MRays/s |
|----:|-----------:|----------|--------:|
| | | | |

## Max recursion depth (fixed 640×360, SPP = 4)

Reference is **max_depth = 5** (matches default interactive `Renderer`). Lower depth caps reflection recursion.

| max_depth | Median (s) | vs depth 5 | MRays/s |
|----------:|-----------:|-----------:|--------:|
| | | | |

## OpenMP threads (fixed 640×360, SPP = 4)

First row is the reference for *vs first row*.

| Threads | Median (s) | vs first row | MRays/s |
|--------:|-----------:|--------------|--------:|
| | | | |

## Other knobs you may benchmark separately

| Knob | How to measure | Typical reference |
|------|----------------|-------------------|
| Compile-time flags | `make benchmark` (batch rebuild matrix) | default `Makefile` flags |
| SIMD / wavefront | `make bench-cpu-modes` or feature-deltas SIMD rows | scalar baseline in same binary |
| GPU vs CPU | `make benchmark-cpu-gpu` | batch GPU path |
| Progressive / adaptive | Interactive `RT_PROFILE=1` | path IDs in `benchmark_results/README.md` |
