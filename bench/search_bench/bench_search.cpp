#include <benchmark/benchmark.h>

#include <atomic>

#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// The whole engine at a fixed depth on a fixed position set: the only thing that
// catches a function getting faster while the engine gets slower. See
// docs/benchmarking.md.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// Fixed depth so node counts are comparable commit-to-commit.
constexpr int kFixedDepth = 5;

// Matches the engine's default and test_search_nodes.cpp: node count varies with
// table size, so this is part of the specification, not a free choice.
constexpr size_t kHashMb = 16;

void run_fixed_depth_search(benchmark::State& state, const char* fen) {
	// Allocated outside the loop: zeroing 16 MB costs more than the endgame
	// position costs to search, so inside it this measured mostly memset.
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
		// Both cleared so each iteration starts cold: a warm table or history would
		// walk the node count down over the run.
		tt.clear();
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

	// Not kIsRate: the search is deterministic, so every iteration visits the same
	// `nodes` and the count must be multiplied in before dividing by elapsed time.
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
	// The other end of the branching-factor spectrum from Kiwipete.
	run_fixed_depth_search(state, "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1");
}
BENCHMARK(BM_Search_Endgame)->Unit(benchmark::kMillisecond);

static void BM_Search_Tactical(benchmark::State& state) {
	// WAC.019 (Bxf7+), the same position test_tactics.cpp uses: it rewards
	// resolving forcing lines, so it is the most ordering-sensitive of the four.
	run_fixed_depth_search(state, "r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1");
}
BENCHMARK(BM_Search_Tactical)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
