#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "core/move.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// A golden-file test, not a benchmark: node count at a fixed depth is
// deterministic on any machine, compiler or optimization level, so it runs in CI
// with no tolerance band. It is expected to fail whenever search or movegen
// changes on purpose -- update the table below in the same PR and the diff
// becomes the regression report. See docs/benchmarking.md.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

struct NodeBudget {
	const char* name;
	const char* fen;
	int depth;
	uint64_t nodes;
	// What the search concluded: a pure efficiency change moves the node count and
	// leaves these two alone, which is the distinction the table exists to make.
	int16_t score;
	const char* best_move;  // UCI
};

// The same four positions as bench/search_bench, at a fixed 16 MB hash, counting
// quiescence nodes. Every value this table has ever held, and why each moved:
// docs/benchmarking.md#how-the-pinned-table-has-moved.
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
	// Fresh per search: carried between positions it would make these counts
	// depend on what ran before them.
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

// Split out because the two failures mean opposite things: a moved node count is
// a search that got cheaper, a moved score is one that changed its mind. LMR is
// allowed to do the second, so this makes it deliberate rather than impossible.
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

// The TT may change how much tree is walked, never what the search concludes.
// Node count is deliberately not asserted -- it legitimately falls as the table
// grows -- but the score catches a probe returning another position's entry.
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
