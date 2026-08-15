#include <benchmark/benchmark.h>

#include <atomic>

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

// 16 MB, matching the engine's default and the node counts pinned in
// tests/unit/test_search_nodes.cpp -- node count varies with table size, so
// this is part of the specification of the numbers below, not a free choice.
constexpr size_t kHashMb = 16;

void run_fixed_depth_search(benchmark::State& state, const char* fen) {
	// Parsed and allocated once, outside the loop. Zeroing a 16 MB table costs
	// more than the endgame position costs to search, so doing it inside the
	// timed region made that benchmark mostly a memset measurement.
	const Position start = parse_fen(fen);
	search::TranspositionTable tt(kHashMb);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};

	search::SearchLimits limits;
	limits.depth = kFixedDepth;

	uint64_t nodes = 0;
	int depth_reached = 0;

	for (auto _ : state) {
		state.PauseTiming();
		// Cleared every iteration so each search starts cold. A warm table would
		// let iteration 2 onward search a smaller tree, and the node count would
		// stop being the deterministic figure the unit test pins.
		tt.clear();
		// Same reasoning as the table: a history table left warm from the
		// previous iteration would order the next search better and quietly
		// walk the node count down over the run.
		move_history.clear();
		Position pos = start;
		stop.store(false, std::memory_order_relaxed);
		state.ResumeTiming();

		search::SearchResult result = search::think(pos, limits, tt, move_history, stop);
		benchmark::DoNotOptimize(result);

		nodes = result.nodes;
		depth_reached = result.depth_reached;
	}

	state.counters["nodes"] = static_cast<double>(nodes);
	state.counters["depth_reached"] = static_cast<double>(depth_reached);

	// kIsIterationInvariantRate, not kIsRate. The search is deterministic, so
	// every iteration visits the same `nodes`; this flag tells Google Benchmark
	// to multiply by the iteration count before dividing by elapsed time, which
	// is the actual nodes/second.
	//
	// kIsRate alone divided one iteration's node count by the time taken by all
	// of them, understating nps by the iteration count -- and since that count
	// varies with how slow the position is (269 iterations for the opening vs.
	// 2080 for the endgame), it made three runs of one engine look 200x apart.
	state.counters["nps"] = benchmark::Counter(
		static_cast<double>(nodes), benchmark::Counter::kIsIterationInvariantRate);
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

static void BM_Search_Tactical(benchmark::State& state) {
	// WAC.019, an opening-phase position with a concrete tactic (Bxf7+).
	// REFERENCE.md 3.11 specifies an opening/middlegame/endgame/*tactical* mix
	// and this is the position that fills the last slot: unlike the three
	// above, it rewards resolving forcing lines rather than surveying quiet
	// ones, which is what makes it the position most sensitive to ordering and
	// pruning work. It is the same position as WAC.019 in
	// tests/unit/test_tactics.cpp, so a node-count movement here and a solve/
	// no-solve there refer to the same search.
	run_fixed_depth_search(state, "r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1");
}
BENCHMARK(BM_Search_Tactical)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
