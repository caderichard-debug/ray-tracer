# Visual regression (CPU batch)

Builds the batch renderer with **OpenMP disabled**, renders a small Cornell box frame with **`--deterministic`** primary jitter and compares the SHA-256 of the raw **P6 PPM** to a committed golden hash.

Bit-stable output requires: **OpenMP off** (sequential integration order), **`--deterministic`** for primary-ray jitter, **`#if ENABLE_STRATIFIED`** (not `#ifdef`) so stratified is really off when set to 0, and **dielectric** using `random_float_pcg()` instead of `std::random_device`-seeded `mt19937`.

Golden hashes may differ across CPU/compiler/`ffast-math`; refresh the golden file on your machine when intentionally changing the integrator.

## Run

```bash
make regression-test
```

Or:

```bash
sh tests/regression/run.sh
```

## Optional hot reload (interactive CPU)

- Copy `config/camera_hot_reload.txt.example` to `config/camera_hot_reload.txt` and edit `eye` / `lookat` lines.
- Press **F5** in the interactive app to reload, or set `RT_HOT_RELOAD_POLL=1` to poll the file every ~90 frames.

Environment override: `RT_HOT_RELOAD_CAMERA=/path/to/file.txt`.

## Refreshing the golden hash

After an **intentional** output change:

```bash
make batch-cpu ENABLE_OPENMP=0 -j4
./build/raytracer_batch_cpu -w 160 -s 1 -d 3 --fixed-ppm tests/regression/out.ppm -o reg_ignore
shasum -a 256 tests/regression/out.ppm | awk '{print $1}' > tests/golden/cornell_160w_s1_d3.sha256
```

Commit the updated `tests/golden/cornell_160w_s1_d3.sha256`.
