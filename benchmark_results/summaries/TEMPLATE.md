# Benchmark summary template

Copy this file to `summaries/YYYYMMDD_short_title.md` and fill in after a run.

## Metadata

- **Date:**
- **Git commit:** (`git rev-parse --short HEAD`)
- **OS / arch:**
- **CPU model:** (`sysctl -n machdep.cpu.brand_string` on macOS, or `/proc/cpuinfo`)
- **OpenMP threads:** (from bench output or `omp_get_max_threads()`)

## Configuration

- **Scene:** Cornell box (default for `bench_cpu_render_modes`)
- **Width × height:**
- **Samples per pixel:**
- **Warmup / timed reps:**

## Results

| Mode            | Median time (s) | MRays/s (est.) | Notes |
|-----------------|-----------------|----------------|-------|
| scalar_openmp   |                 |                |       |
| simd_packets    |                 |                |       |
| wavefront       |                 |                |       |

## Raw log

Link or path: `../runs/<filename>.log`

## Comparison

- vs previous summary (date / commit):
- Regression or improvement notes:
