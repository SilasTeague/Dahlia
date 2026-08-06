#include <benchmark/benchmark.h>

#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Macrobenchmark (REFERENCE.md 3.11 / Milestone 3 baseline): the whole
// engine -- fixed position set, fixed depth -- tracking nodes/sec and depth
// reached. This is what a microbenchmark can't catch: it's the number that
// tells you a pruning/ordering change actually shrank the tree, independent
// of raw hardware speed (see REFERENCE.md 3.8/3.11's "nodes to reach depth
// N" metric).

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// Fixed depth so node counts are comparable commit-to-commit; Google
// Benchmark's own iteration count/timing is secondary to the "nodes"
// counter below, which is the metric REFERENCE.md 3.11 actually cares about.
constexpr int kFixedDepth = 5;

void run_fixed_depth_search(benchmark::State& state, const char* fen) {
	for (auto _ : state) {
		Position pos = parse_fen(fen);
		search::SearchLimits limits;
		limits.depth = kFixedDepth;
		search::TranspositionTable tt(16);
		search::SearchResult result = search::think(pos, limits, tt);
		benchmark::DoNotOptimize(result);

		state.counters["nodes"] = static_cast<double>(result.nodes);
		state.counters["nps"] = benchmark::Counter(static_cast<double>(result.nodes), benchmark::Counter::kIsRate);
		state.counters["depth_reached"] = static_cast<double>(result.depth_reached);
	}
}

}  // namespace

static void BM_Search_Opening(benchmark::State& state) {
	run_fixed_depth_search(state, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}
BENCHMARK(BM_Search_Opening)->Unit(benchmark::kMillisecond);

static void BM_Search_Middlegame(benchmark::State& state) {
	// Kiwipete: a standard dense-middlegame movegen/search stress position.
	run_fixed_depth_search(state, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
}
BENCHMARK(BM_Search_Middlegame)->Unit(benchmark::kMillisecond);

static void BM_Search_Endgame(benchmark::State& state) {
	// King + pawn endgame: sparse board, long forcing lines relative to
	// piece count, the other end of the branching-factor spectrum from the
	// middlegame position above.
	run_fixed_depth_search(state, "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1");
}
BENCHMARK(BM_Search_Endgame)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
