#!/usr/bin/env bash
# Deterministic regression: single-threaded batch, fixed PPM, SHA-256 vs golden.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RAY_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$RAY_ROOT"

mkdir -p tests/regression

make batch-cpu ENABLE_OPENMP=0 -j4

OUT_PPM="tests/regression/out.ppm"
GOLDEN="tests/golden/cornell_160w_s1_d3.sha256"

./build/raytracer_batch_cpu -w 160 -s 1 -d 3 --deterministic --fixed-ppm "$OUT_PPM" -o regression_batch_ignore

if command -v shasum >/dev/null 2>&1; then
  HASH=$(shasum -a 256 "$OUT_PPM" | awk '{print $1}')
elif command -v sha256sum >/dev/null 2>&1; then
  HASH=$(sha256sum "$OUT_PPM" | awk '{print $1}')
else
  echo "regression: need shasum (macOS) or sha256sum (Linux)" >&2
  exit 2
fi

if [[ ! -f "$GOLDEN" ]]; then
  echo "regression: missing golden $GOLDEN" >&2
  exit 3
fi

# Compare first line only (allow comments in golden file)
read -r EXPECTED < "$GOLDEN"
if [[ "$HASH" != "$EXPECTED" ]]; then
  echo "REGRESSION FAIL: SHA-256 mismatch"
  echo "  got      $HASH"
  echo "  expected $EXPECTED"
  exit 1
fi

echo "regression: OK ($HASH)"
