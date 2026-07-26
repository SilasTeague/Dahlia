#include <benchmark/benchmark.h>

#include "core/types.h"

// Milestone 0 placeholder: confirms the Google Benchmark target builds and
// runs before real movegen/eval/TT microbenchmarks exist (see REFERENCE.md 3.11).
static void BM_SquareBBShift(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = 1ULL << Square::e4;
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_SquareBBShift);

BENCHMARK_MAIN();
