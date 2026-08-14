#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "core/move.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Search regression tests (REFERENCE.md 1.6/3.8, Milestone 3): a handful of
// tactical positions solvable at shallow depth, guarding against a broken
// alpha-beta/negamax implementation missing forced mates or free material.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

bool same_move(Move a, Move b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

}  // namespace

TEST_CASE("search: finds mate in one (back-rank mate)", "[search]") {
	// Black king boxed in by its own pawns on f7/g7/h7; Ra1-a8 is mate.
	Position pos = parse_fen("6k1/5ppp/8/8/8/8/8/R6K w - - 0 1");

	search::SearchLimits limits;
	limits.depth = 2;
	search::TranspositionTable tt(1);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	search::SearchResult result = search::think(pos, limits, tt, move_history, stop);

	CHECK(same_move(result.best_move, Move{a1, a8}));
	CHECK(result.score >= search::kMateScore - search::kMaxPly);
}

TEST_CASE("search: captures an undefended rook for free", "[search]") {
	Position pos = parse_fen("4k2r/8/8/8/8/8/8/4K2R w - - 0 1");

	search::SearchLimits limits;
	limits.depth = 3;
	search::TranspositionTable tt(1);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	search::SearchResult result = search::think(pos, limits, tt, move_history, stop);

	CHECK(same_move(result.best_move, Move{h1, h8}));
}

TEST_CASE("search: does not hang the queen to a defended pawn", "[search]") {
	// Qxe5 looks tempting (wins a pawn) but d6 recaptures the queen; a
	// depth-2 search must see the recapture and avoid it.
	Position pos = parse_fen("4k3/8/3p4/4p3/4Q3/8/8/4K2R w - - 0 1");

	search::SearchLimits limits;
	limits.depth = 2;
	search::TranspositionTable tt(1);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	search::SearchResult result = search::think(pos, limits, tt, move_history, stop);

	CHECK_FALSE(same_move(result.best_move, Move{e4, e5}));
}
