# Benchmarking guide

This document explains how to measure performance in this project, where outputs go, and how to record results for comparisons over time.

## Where results live

All machine-generated logs and human-written comparison tables should stay under **`benchmark_results/`** (repository root, next to the `Makefile`).

| Location | Purpose |
|----------|---------|
| **`benchmark_results/runs/`** | Raw logs from scripts and `make benchmark*` (timestamps, compiler noise, full stderr). Safe to delete; see `.gitignore`. |
| **`benchmark_results/summaries/`** | Short Markdown tables you edit after a run (date, hardware, config, numbers). **Commit** these when you want history in git. |
| **`benchmark_results/README.md`** | Index and conventions for that folder. |

Legacy scripts may still mention `benchmark_results.txt` in the repo root; prefer the centralized paths above.

---

## Recommended: headless SIMD / wavefront / scalar (Cornell box)

This is the most reliable automated comparison for **scalar OpenMP**, **SIMD packet tracing**, and **wavefront** on the same scene and camera.

### Build

```bash
make bench-cpu-modes
```

### Run

```bash
./build/bench_cpu_render_modes [options]
```

| Option | Meaning | Default |
|--------|---------|--------|
| `-w`, `--width` | Image width (16:9 height derived) | `800` |
| `-s`, `--samples` | Samples per pixel | `4` |
| `-r`, `--reps` | Timed repetitions (median reported) | `3` |
| `--warmup` | Warmup passes before timing | `1` |

Example:

```bash
./build/bench_cpu_render_modes -w 1280 -s 4 -r 8 --warmup 2
```

### Shell wrapper (build + run + log)

```bash
./benchmark_simd_wavefront.sh
```

Writes a timestamped file under **`benchmark_results/runs/`**.

---

## Makefile batch benchmarks

These rebuild the **batch CPU** binary with different compile-time feature flags, then run `./raytracer`. They capture **stderr** from the batch renderer (not wall-clock sections unless you wrap with `time` yourself).

```bash
make benchmark          # Feature matrix → benchmark_results/runs/cpu_performance.log
make benchmark-cpu-gpu # CPU vs GPU batch → benchmark_results/runs/cpu_vs_gpu.log
```

**Note:** `make benchmark` performs many full rebuilds; expect a long run. Ensure `./raytracer` exists and points at the batch CPU build (symlink created by `make batch-cpu`).

---

## Interactive CPU profiling (`RT_PROFILE`)

The SDL interactive CPU build can print rolling averages for time spent in each **render path** inside the worker (useful after changing progressive, SIMD, or wavefront code).

```bash
RT_PROFILE=1 ./build/raytracer_interactive_cpu
```

Unset `RT_PROFILE` or set it to `0` to disable. Details of path IDs are in **`benchmark_results/README.md`** (worker path legend) or in the source near `[RT_PROFILE]`.

---

## Legacy / demo scripts (use with care)

| Script | Notes |
|--------|--------|
| `benchmark.sh` | Expects `gdate` and `build/raytracer_phase2`; may not match current Makefile output names. |
| `benchmark_features.sh` | Builds `raytracer_interactive` (old name); includes a **placeholder** C++ harness with simulated timings. |
| `benchmark_advanced_features.sh` | Mostly scaffolding; not a substitute for real timed renders. |
| `benchmark_simd.sh` | Historically pointed at interactive toggles; prefer **`bench_cpu_render_modes`** for numbers. |

If you improve these scripts, redirect output to **`benchmark_results/runs/`** and add a row to a file under **`summaries/`**.

---

## Documenting a run (workflow)

1. Run a benchmark (e.g. `./benchmark_simd_wavefront.sh` or `make benchmark`).
2. Copy the important lines (resolution, samples, median seconds, MRays/s, machine, CPU model) into a new file under **`benchmark_results/summaries/`**, e.g. `2026-04-17_simd_wavefront.md`.
3. Optionally link that summary from `docs/cpu-performance-results.md` if you maintain a long-form results doc.
4. Commit **`summaries/*.md`**; raw **`runs/*.log`** are ignored by git by default.

A blank table template is in **`benchmark_results/summaries/TEMPLATE.md`**.

---

## See also

- **`benchmark_results/README.md`** — folder layout and git policy.
- **`docs/cpu-performance-results.md`** — narrative performance notes (if present).
- **`make help`** — lists `bench-cpu-modes`, `benchmark`, and related targets.
