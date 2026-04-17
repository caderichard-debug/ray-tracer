# Performance results (centralized)

This directory is the **single place** for benchmark outputs and curated result tables.

## Layout

```
benchmark_results/
├── README.md                 # This file
├── runs/                     # Raw logs (auto-generated; gitignored *.log)
│   └── *.log
└── summaries/                # Human-written Markdown (commit to git)
    └── *.md
```

## What goes where

| Subfolder | Content | Git |
|-----------|---------|-----|
| **`runs/`** | Full logs from `make benchmark`, `make benchmark-cpu-gpu`, `./benchmark_simd_wavefront.sh`, or manual redirects (`tee`). Use timestamped filenames. | `*.log` ignored (see repo `.gitignore`) |
| **`summaries/`** | Short tables: date, OS, CPU, compiler flags, resolution, samples, median time, speedup vs baseline, notes. | **Commit** when you want a permanent record |

## Naming suggestions

- **Runs:** `runs/YYYYMMDD_description.log` (example: `runs/20260417_simd_wavefront_w800_s4.log`)
- **Summaries:** `summaries/YYYYMMDD_topic.md`

## Interactive worker path IDs (`RT_PROFILE`)

When `RT_PROFILE=1` is set for **`raytracer_interactive_cpu`**, stderr lines like `[RT_PROFILE] path N avg_ms=...` use:

| `path` | Meaning |
|--------|---------|
| 1 | Progressive + wavefront |
| 2 | Progressive + SIMD packets |
| 3 | Progressive scalar accumulation |
| 4 | Full-frame wavefront |
| 5 | Full-frame SIMD packets |
| 6 | Default scalar OpenMP path |

## Instructions

See **`docs/benchmarking.md`** for how to run benchmarks and copy results into **`summaries/`**.
