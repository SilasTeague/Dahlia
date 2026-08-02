#!/usr/bin/env bash
# Build (Release) and run the full benchmark suite (microbench + search_bench),
# producing one history file under bench/results/history/ per REFERENCE.md
# 3.11. Run this before committing any change under src/movegen/,
# src/position/, src/search/, src/eval/, or bench/ itself -- that's the same
# path set that gates benchmark.yml in CI, so a local run here is the fast
# preview of what CI will record.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/bench}"
COMPILER="${CXX:-$(command -v c++)}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DDAHLIA_BUILD_TESTS=OFF \
	-DDAHLIA_BUILD_BENCHMARKS=ON
cmake --build "$BUILD_DIR" --parallel --target dahlia_microbench dahlia_search_bench

MICROBENCH_JSON="$(mktemp)"
"$BUILD_DIR/bench/microbench/dahlia_microbench" --benchmark_format=json > "$MICROBENCH_JSON"

SEARCH_BENCH_JSON="$(mktemp)"
"$BUILD_DIR/bench/search_bench/dahlia_search_bench" --benchmark_format=json > "$SEARCH_BENCH_JSON"

mkdir -p "$ROOT_DIR/bench/results/history"
DATE="$(date -u +%Y-%m-%d)"
HASH="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
OUT="$ROOT_DIR/bench/results/history/${DATE}-${HASH}.json"

python3 "$ROOT_DIR/scripts/format_bench_result.py" \
	--gbench-json "$MICROBENCH_JSON" \
	--search-gbench-json "$SEARCH_BENCH_JSON" \
	--compiler "$COMPILER" \
	--build-type "RelWithDebInfo" \
	--out "$OUT"

echo "Wrote $OUT"
