#!/usr/bin/env bash
# Build and run bench_feature_deltas: toggle/path matrix vs scalar baseline (Cornell box).
# Logs full stdout to benchmark_results/runs/ and writes the Markdown table to summaries/.
#
# Usage:
#   ./benchmark_feature_deltas.sh
#       → writes summaries with feature table + scaling (resolution, SPP, max_depth, threads) by default.
#   ./benchmark_feature_deltas.sh -- --no-sweep
#       → feature table only (faster).
#   ./benchmark_feature_deltas.sh -- -w 800 -s 8 -r 3 --threads 8
#
# Extra args after -- are passed to ./build/bench_feature_deltas

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

RUNS_DIR="$ROOT/benchmark_results/runs"
SUM_DIR="$ROOT/benchmark_results/summaries"
mkdir -p "$RUNS_DIR" "$SUM_DIR"

TS="$(date +%Y%m%d_%H%M%S)"
LOG="$RUNS_DIR/feature_deltas_${TS}.log"
MD="$SUM_DIR/feature_deltas_${TS}.md"

EXTRA=()
if [[ "${1:-}" == "--" ]]; then
  shift
  EXTRA=("$@")
elif [[ $# -gt 0 ]]; then
  echo "Unknown arguments: $*" >&2
  echo "Pass bench_feature_deltas flags after --, e.g.  $0 -- -w 800 -s 8" >&2
  exit 2
fi

echo "Building bench_feature_deltas..."
make bench-feature-deltas

BIN="$ROOT/build/bench_feature_deltas"
if [[ ! -x "$BIN" ]]; then
  echo "ERROR: $BIN not found or not executable" >&2
  exit 1
fi

echo "Running (log: $LOG, markdown: $MD)..."
# With `set -u`, "${EXTRA[@]}" can error when EXTRA is empty (bash version dependent).
if [[ ${#EXTRA[@]} -eq 0 ]]; then
  "$BIN" --markdown-out "$MD" 2>&1 | tee "$LOG"
else
  "$BIN" "${EXTRA[@]}" --markdown-out "$MD" 2>&1 | tee "$LOG"
fi

echo ""
echo "Done. Summary: $MD"
echo "      Preview PNGs (folder cleared each run): $SUM_DIR/feature_deltas_latest_run/"
