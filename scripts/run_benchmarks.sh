#!/usr/bin/env bash
# Build (Release) and run the benchmark suite, producing one history file
# under bench/results/history/ per REFERENCE.md 3.11. Local use or CI.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/bench}"
COMPILER="${CXX:-$(command -v c++)}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DDAHLIA_BUILD_TESTS=OFF \
	-DDAHLIA_BUILD_BENCHMARKS=ON
cmake --build "$BUILD_DIR" --parallel --target dahlia_microbench

RAW_JSON="$(mktemp)"
"$BUILD_DIR/bench/microbench/dahlia_microbench" --benchmark_format=json > "$RAW_JSON"

mkdir -p "$ROOT_DIR/bench/results/history"
DATE="$(date -u +%Y-%m-%d)"
HASH="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
OUT="$ROOT_DIR/bench/results/history/${DATE}-${HASH}.json"

python3 "$ROOT_DIR/scripts/format_bench_result.py" \
	--gbench-json "$RAW_JSON" \
	--compiler "$COMPILER" \
	--build-type "RelWithDebInfo" \
	--out "$OUT"

echo "Wrote $OUT"
