#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "core/move.h"
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
	// What the search concluded, alongside how much tree it walked. A pure
	// efficiency change -- ordering, PVS, a bigger table -- moves the node
	// count and leaves these two alone, and that is the distinction the table
	// exists to make visible: a diff that moves the score is either an
	// evaluation change or a pruning heuristic that has started guessing.
	int16_t score;
	const char* best_move;  // UCI
};

// Same positions as bench/search_bench/bench_search.cpp, so a node count seen
// here and one seen in the benchmark history refer to the same search.
//
// These counts include quiescence nodes, which is why two of the first three
// went *up* at Milestone 4 even though move ordering cut the main tree sharply:
// depth 5 now means five plies plus however many captures are pending at each
// leaf. The comparable pre-quiescence numbers were 34,195 / 145,195 / 820. The
// figure that shows the ordering work is nodes-to-depth-7, which fell from
// 828,549 to 370,254 (opening) and 4,866,267 to 2,431,525 (middlegame) across
// the same milestone -- see bench/results/history/.
//
// The tactical position was added at Milestone 5, filling the tactical slot
// REFERENCE.md 3.11 asks the macrobenchmark position set to cover; its
// Milestone 3 baseline at this depth was 458,327 nodes.
//
// Milestone 5's tapered piece-square tables raised all four counts by 1-20%
// (23,023 / 161,277 / 1,411 / 151,544 before). That is the expected direction
// and not a regression in the search: a positional evaluation returns far
// fewer equal scores than a material-only one did, so fewer sibling moves
// share a value and the cheap cutoffs that came from ties disappear. What the
// tables buy is bought in strength, measured by the tactics suite, not here.
//
// PVS then took them back down (26,959 / 193,194 / 1,427 / 159,796 before it)
// without moving a single score or best move below -- which is the whole claim
// PVS makes, pinned here rather than asserted in a comment.
//
// Null-move pruning took another 11-83% (26,183 / 152,499 / 1,330 / 156,240
// before it), and the endgame's exact 1,330 is the interesting number: it did
// not move at all, because that position is a king and a pawn and the zugzwang
// guard switches the heuristic off there entirely. The one position null-move
// pruning cannot help is the one it would otherwise get wrong -- see
// test_nullmove.cpp.
//
// Milestone 6's late move reductions then took 44-81% off again (21,337 /
// 136,030 / 1,330 / 25,809 before them), the largest single-change reduction in
// the project. Unlike every step above it, this one is *not* a pure efficiency
// change and the table records that honestly: the endgame's score moved by two
// centipawns (110 -> 108) and the opening changed its mind between two moves it
// scores identically (g1f3 -> d2d4, both 33). LMR searches unpromising moves
// shallowly and accepts a fail-low without verifying it, so it is allowed to
// reach a different conclusion -- which is exactly why the conclusions live in
// their own test below rather than being folded into the node count.
//
// Aspiration windows, landing alongside them, are the opposite kind of change
// and cost nothing here: at depth 5 they are below kAspirationMinDepth and
// never engage at all.
constexpr NodeBudget kExpected[] = {
	{"opening", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4260, 33, "d2d4"},
	{"middlegame (Kiwipete)",
	 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 25959, -27, "e2a6"},
	{"endgame (K+P)", "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1", 5, 742, 108, "e3e4"},
	{"tactical (WAC.019)",
	 "r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1", 5, 7510, 328, "c4f7"},
};

search::SearchResult search_at_depth(const char* fen, int depth) {
	Position pos = parse_fen(fen);
	search::SearchLimits limits;
	limits.depth = depth;
	search::TranspositionTable tt(16);
	// Fresh per search, like the table above: a history table carried between
	// positions would make these counts depend on what ran before them.
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	return search::think(pos, limits, tt, move_history, stop);
}

std::string move_to_uci(Move m) {
	if (m.from == NULL_SQUARE) return "none";
	auto square = [](Square s) {
		return std::string{static_cast<char>('a' + (s % 8)), static_cast<char>('1' + (s / 8))};
	};
	std::string uci = square(m.from) + square(m.to);
	switch (m.promotion) {
		case PROMOTION_KNIGHT: uci += 'n'; break;
		case PROMOTION_BISHOP: uci += 'b'; break;
		case PROMOTION_ROOK: uci += 'r'; break;
		case PROMOTION_QUEEN: uci += 'q'; break;
		default: break;
	}
	return uci;
}

}  // namespace

TEST_CASE("search: fixed-depth node counts match the recorded baseline", "[search][nodes]") {
	for (const NodeBudget& expected : kExpected) {
		INFO("position: " << expected.name << " at depth " << expected.depth
		      << "\nIf this change was intentional, update kExpected in this file "
		         "and note the before/after in the PR.");
		CHECK(search_at_depth(expected.fen, expected.depth).nodes == expected.nodes);
	}
}

// The other half of the same table, split out because the two failures mean
// opposite things. A node count that moves is a search that got cheaper or
// dearer; a score or best move that moves is a search that changed its mind,
// and only an evaluation change or a heuristic that prunes something real
// should be able to do that.
//
// Since Milestone 6 the engine contains one heuristic of exactly that second
// kind -- late move reductions -- so this test is no longer expected to hold
// across every search change, only to make a changed conclusion impossible to
// land by accident. An LMR tuning change that moves a score here is legitimate;
// it just has to be argued for rather than absorbed silently into a node count.
TEST_CASE("search: fixed-depth conclusions match the recorded baseline", "[search][nodes]") {
	for (const NodeBudget& expected : kExpected) {
		INFO("position: " << expected.name << " at depth " << expected.depth
		      << "\nA node-count-only change should not reach this test. If the search "
		         "genuinely changed its mind, say why in the PR.");
		search::SearchResult result = search_at_depth(expected.fen, expected.depth);
		CHECK(result.score == expected.score);
		CHECK(move_to_uci(result.best_move) == expected.best_move);
	}
}

// The TT is an optimization: it may change how much tree the search walks, but
// never what the search concludes.
//
// Node count deliberately isn't asserted here -- it legitimately falls as the
// table grows, because a bigger table evicts fewer entries and so produces more
// cutoffs (measured at Milestone 4: 164,722 nodes at 1 MB down to 161,277 at
// 16 MB, flat from there to 256 MB once replacement stops thrashing). That is
// why kExpected above pins a hash size:
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
		search::HistoryTable move_history;
		std::atomic<bool> stop{false};
		return search::think(pos, limits, tt, move_history, stop);
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
