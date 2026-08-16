#!/usr/bin/env bash
# Build the engine, run the full benchmark suite, record one history file under
# bench/results/history/, and print what changed since the last run.
#
#   scripts/run_benchmarks.sh
#
# The intended workflow is: make a change, run this, read the deltas, and commit
# the new history file alongside the change. Timing numbers are only comparable
# within one machine, so run this on the same machine every time -- the script
# records the CPU, compiler, and flags in every file, and the comparison warns
# loudly if any of them moved between runs (docs/REFERENCE.md 3.11).
#
# To see the whole arc rather than just the last step:
#
#   scripts/compare_bench_results.py --history
#   scripts/compare_bench_results.py --history --benchmark Middlegame
#
# Environment overrides:
#   BUILD_DIR           build tree (default build/bench)
#   BENCH_REPETITIONS   repetitions per benchmark; the median is recorded
#   BENCH_CXX_FLAGS     CMAKE_CXX_FLAGS for the benchmark build
#   BENCH_TAG           release tag to record in the result file
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/bench}"
REPETITIONS="${BENCH_REPETITIONS:-5}"

# CMake's own RelWithDebInfo default, stated explicitly so it lands in the
# result file rather than being an invisible assumption.
#
# This matches what the release preset and docker/Dockerfile.release actually
# compile at, despite both nominally passing -O3: CMake emits CMAKE_CXX_FLAGS
# before CMAKE_CXX_FLAGS_RELWITHDEBINFO ("-O2 -g -DNDEBUG"), and the last -O
# flag wins. Raising this to a real -O3 means changing it here and in those two
# places together, and it will shift every recorded time at once.
CXX_FLAGS="${BENCH_CXX_FLAGS:--O2 -g}"

# The version string, not just the binary path -- a compiler upgrade is one of
# the few things that can move these numbers without a line of Dahlia changing.
CXX_BIN="${CXX:-$(command -v c++)}"
COMPILER="$("$CXX_BIN" --version 2>/dev/null | head -1)"
COMPILER="${COMPILER:-$CXX_BIN}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_CXX_FLAGS="$CXX_FLAGS" \
	-DDAHLIA_BUILD_TESTS=OFF \
	-DDAHLIA_BUILD_BENCHMARKS=ON
cmake --build "$BUILD_DIR" --parallel --target dahlia_microbench dahlia_search_bench

# Repetitions + aggregates-only: the median is what gets recorded, and the
# per-repetition rows would bloat the history file without being read.
GBENCH_ARGS=(
	--benchmark_format=json
	--benchmark_repetitions="$REPETITIONS"
	--benchmark_report_aggregates_only=true
)

MICROBENCH_JSON="$(mktemp)"
"$BUILD_DIR/bench/microbench/dahlia_microbench" "${GBENCH_ARGS[@]}" > "$MICROBENCH_JSON"

SEARCH_BENCH_JSON="$(mktemp)"
"$BUILD_DIR/bench/search_bench/dahlia_search_bench" "${GBENCH_ARGS[@]}" > "$SEARCH_BENCH_JSON"

mkdir -p "$ROOT_DIR/bench/results/history"
DATE="$(date -u +%Y-%m-%d)"
HASH="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
OUT="$ROOT_DIR/bench/results/history/${DATE}-${HASH}.json"

# Running before committing is the expected workflow, so HEAD is the change's
# *parent*, not the change itself. Recorded rather than papered over: `git log`
# on the committed file resolves the ambiguity, and the flag keeps the file from
# claiming to measure a commit it doesn't.
DIRTY=false
if ! git -C "$ROOT_DIR" diff --quiet HEAD -- 2>/dev/null; then
	DIRTY=true
fi

python3 "$ROOT_DIR/scripts/format_bench_result.py" \
	--gbench-json "$MICROBENCH_JSON" \
	--search-gbench-json "$SEARCH_BENCH_JSON" \
	--compiler "$COMPILER" \
	--build-type "RelWithDebInfo" \
	--cxx-flags "$CXX_FLAGS" \
	--repetitions "$REPETITIONS" \
	--tag "${BENCH_TAG:-}" \
	--dirty "$DIRTY" \
	--out "$OUT"

echo
echo "Wrote ${OUT#"$ROOT_DIR"/}"
if [ "$DIRTY" = true ]; then
	echo "  (working tree dirty: recorded against parent commit $HASH — commit this file with your change)"
fi
echo

python3 "$ROOT_DIR/scripts/compare_bench_results.py" --latest
