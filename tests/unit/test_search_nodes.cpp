#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Node-count regression test (REFERENCE.md 1.6/3.11): the number of nodes a
// fixed-depth search visits, pinned to a checked-in table.
//
// This is a golden-file test, not a benchmark. Node count at a fixed depth is
// deterministic -- no time-based cutoff applies (time_budget_ms returns its
// unbounded value whenever a depth limit is set), the TT is constructed fresh
// per search, and evaluation is integer arithmetic -- so the same engine
// returns the identical count on any machine, compiler, or optimization level.
// That determinism is what lets this run in ordinary CI with no tolerance band,
// no repetitions, and no timing: any change here is a real change in the shape
// of the search tree.
//
// It is expected to fail whenever search or movegen changes on purpose. When it
// does, update the table below in the same PR -- the diff is then the
// regression report, showing exactly what the change did to the tree (see
// REFERENCE.md 3.11's "nodes to reach depth N" as the metric of record).
//
// Wall-clock performance is deliberately not measured here. Timing is only
// comparable within one machine, so it lives in scripts/run_benchmarks.sh and
// the history under bench/results/.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

struct NodeBudget {
	const char* name;
	const char* fen;
	int depth;
	uint64_t nodes;
};

// Same positions as bench/search_bench/bench_search.cpp, so a node count seen
// here and one seen in the benchmark history refer to the same search.
constexpr NodeBudget kExpected[] = {
	{"opening", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 34195},
	{"middlegame (Kiwipete)",
	 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 145195},
	{"endgame (K+P)", "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1", 5, 820},
};

uint64_t search_nodes(const char* fen, int depth) {
	Position pos = parse_fen(fen);
	search::SearchLimits limits;
	limits.depth = depth;
	search::TranspositionTable tt(16);
	std::atomic<bool> stop{false};
	return search::think(pos, limits, tt, stop).nodes;
}

}  // namespace

TEST_CASE("search: fixed-depth node counts match the recorded baseline", "[search][nodes]") {
	for (const NodeBudget& expected : kExpected) {
		INFO("position: " << expected.name << " at depth " << expected.depth
		      << "\nIf this change was intentional, update kExpected in this file "
		         "and note the before/after in the PR.");
		CHECK(search_nodes(expected.fen, expected.depth) == expected.nodes);
	}
}

// The TT is an optimization: it may change how much tree the search walks, but
// never what the search concludes.
//
// Node count deliberately isn't asserted here -- it legitimately falls as the
// table grows, because a bigger table evicts fewer entries and so produces more
// cutoffs (measured: 146,584 nodes at 1 MB down to 145,076 at 256 MB, flattening
// as replacement stops thrashing). That is why kExpected above pins a hash size:
// 16 MB is part of the specification of those numbers, not an incidental choice.
//
// The score is the invariant, and it is the one that catches the failure mode
// REFERENCE.md 3.7 warns about: a probe that returns an entry belonging to a
// different position would change the score, which a verified key comparison is
// supposed to make impossible.
TEST_CASE("search: result is independent of transposition table size", "[search][nodes][tt]") {
	auto search_with_hash = [](size_t megabytes) {
		Position pos = parse_fen(kExpected[1].fen);
		search::SearchLimits limits;
		limits.depth = kExpected[1].depth;
		search::TranspositionTable tt(megabytes);
		std::atomic<bool> stop{false};
		return search::think(pos, limits, tt, stop);
	};

	search::SearchResult reference = search_with_hash(16);
	for (size_t megabytes : {size_t{1}, size_t{4}, size_t{64}}) {
		INFO("hash size: " << megabytes << " MB");
		search::SearchResult result = search_with_hash(megabytes);
		CHECK(result.score == reference.score);
		CHECK(result.best_move.from == reference.best_move.from);
		CHECK(result.best_move.to == reference.best_move.to);
	}
}
