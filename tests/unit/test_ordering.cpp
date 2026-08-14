#include <catch2/catch_test_macros.hpp>

#include "core/move.h"
#include "movegen/attacks.h"
#include "movegen/movegen.h"
#include "position/position.h"
#include "search/ordering.h"

// Move ordering tests (REFERENCE.md 3.8 responsibility 4, Milestone 4).
//
// Nothing here is a correctness test in the usual sense: ordering cannot make
// the search wrong, only slow. What these tests pin is the *ranking* -- that
// each heuristic outranks the ones below it and that the bands never overlap --
// because a band collision is silent. It costs nodes, passes every other test
// in the suite, and shows up only as a number in the benchmark history that
// nobody can explain.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

constexpr Move kNoMove{NULL_SQUARE, NULL_SQUARE};

bool same_move(Move a, Move b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

// Orders `pos`'s legal moves fully and returns them, so a test can assert on
// position N of the final ordering rather than on raw scores.
MoveList ordered_moves(Position& pos, Move tt_move = kNoMove,
                       const search::KillerEntry& killers = search::KillerEntry{},
                       const search::HistoryTable& history = search::HistoryTable{}) {
	MoveList moves;
	generate_legal_moves(moves, pos);

	search::MoveScores scores;
	search::score_moves(moves, pos, tt_move, killers, history, scores);
	for (int i = 0; i < moves.count; i++) search::select_next_move(moves, scores, i);
	return moves;
}

int index_of(const MoveList& moves, Move m) {
	for (int i = 0; i < moves.count; i++) {
		if (same_move(moves.moves[i], m)) return i;
	}
	return -1;
}

}  // namespace

TEST_CASE("ordering: is_capture recognizes an ordinary capture", "[ordering]") {
	Position pos = parse_fen("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");

	CHECK(search::is_capture(pos, Move{e4, d5}));
	CHECK_FALSE(search::is_capture(pos, Move{e4, e5}));
}

// En passant is the case a bare board[to] check gets wrong: the captured pawn
// is beside the destination square, not on it, so the destination reads empty.
TEST_CASE("ordering: is_capture recognizes en passant", "[ordering]") {
	Position pos = parse_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");

	CHECK(search::is_capture(pos, Move{e5, d6}));
	// Same destination square, no en passant right: not a capture.
	Position no_ep = parse_fen("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
	CHECK_FALSE(search::is_capture(no_ep, Move{e5, d6}));
}

// The defining property of MVV-LVA. Both moves capture; the pawn's capture of
// the queen is searched first because it wins material outright even if the
// queen is defended, while the queen's capture of the pawn loses material if
// the pawn is.
TEST_CASE("ordering: MVV-LVA prefers PxQ over QxP", "[ordering]") {
	// White pawn on b4 and queen on d1; black queen on c5 and pawn on d5.
	Position pos = parse_fen("4k3/8/8/2qp4/1P6/8/8/3QK3 w - - 0 1");

	CHECK(search::mvv_lva_score(pos, Move{b4, c5}) > search::mvv_lva_score(pos, Move{d1, d5}));

	MoveList moves = ordered_moves(pos);
	CHECK(index_of(moves, Move{b4, c5}) < index_of(moves, Move{d1, d5}));
}

// The TT move is the single most valuable ordering signal there is -- it was
// the best move here at a shallower depth -- so it outranks even the most
// attractive-looking capture.
TEST_CASE("ordering: the TT move outranks every capture", "[ordering]") {
	Position pos = parse_fen("4k3/8/8/2qp4/1P6/8/8/3QK3 w - - 0 1");
	Move tt_move{e1, e2};  // a quiet king move, chosen precisely because it looks worthless

	MoveList moves = ordered_moves(pos, tt_move);

	CHECK(index_of(moves, tt_move) == 0);
}

// Captures beat killers beat history beat everything else. Asserted as one
// chain rather than three separate tests because it is the *ordering between*
// the bands that matters, not any band's internal scale.
TEST_CASE("ordering: bands rank capture > killer > history > quiet", "[ordering]") {
	Position pos = parse_fen("4k3/8/8/3p4/4P3/8/8/R3K3 w - - 0 1");

	search::KillerEntry killers;
	killers.store(Move{a1, a8});  // a quiet rook lift, nominated as this ply's killer

	search::HistoryTable history;
	history.update(WHITE, Move{a1, a4}, 8);  // a different quiet, with a cutoff record

	MoveList moves = ordered_moves(pos, kNoMove, killers, history);

	int capture = index_of(moves, Move{e4, d5});
	int killer = index_of(moves, Move{a1, a8});
	int historied = index_of(moves, Move{a1, a4});
	int quiet = index_of(moves, Move{a1, a2});

	CHECK(capture < killer);
	CHECK(killer < historied);
	CHECK(historied < quiet);
}

TEST_CASE("ordering: promotions rank by the piece promoted to", "[ordering]") {
	Position pos = parse_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");

	MoveList moves = ordered_moves(pos);

	CHECK(index_of(moves, Move{a7, a8, PROMOTION_QUEEN}) <
	      index_of(moves, Move{a7, a8, PROMOTION_ROOK}));
	CHECK(index_of(moves, Move{a7, a8, PROMOTION_ROOK}) <
	      index_of(moves, Move{a7, a8, PROMOTION_KNIGHT}));
	// And every promotion is searched before the king shuffles.
	CHECK(index_of(moves, Move{a7, a8, PROMOTION_KNIGHT}) < index_of(moves, Move{e1, e2}));
}

// Two slots, most recent first. The demotion is the part worth pinning: a
// store that overwrote both slots, or that failed to demote, would leave the
// table remembering strictly less than it should.
TEST_CASE("ordering: killers demote rather than overwrite", "[ordering]") {
	search::KillerEntry killers;
	killers.store(Move{a1, a2});
	killers.store(Move{b1, b2});

	CHECK(same_move(killers.first, Move{b1, b2}));
	CHECK(same_move(killers.second, Move{a1, a2}));
}

// Storing the same move twice must not fill both slots with it -- that would
// throw away the second killer for no gain.
TEST_CASE("ordering: re-storing the current killer leaves the second slot alone", "[ordering]") {
	search::KillerEntry killers;
	killers.store(Move{a1, a2});
	killers.store(Move{b1, b2});
	killers.store(Move{b1, b2});

	CHECK(same_move(killers.first, Move{b1, b2}));
	CHECK(same_move(killers.second, Move{a1, a2}));
}

TEST_CASE("ordering: history weights deep cutoffs above shallow ones", "[ordering]") {
	search::HistoryTable history;
	history.update(WHITE, Move{a1, a2}, 2);
	history.update(WHITE, Move{b1, b2}, 8);

	CHECK(history.get(WHITE, Move{b1, b2}) > history.get(WHITE, Move{a1, a2}));
	// And the two sides keep separate books: a1a2 is White's record, not Black's.
	CHECK(history.get(BLACK, Move{a1, a2}) == 0);
}

// Saturation handling: entries are halved rather than clamped, so the ranking
// the ordering actually reads survives. A table that clamped instead would let
// every well-used move pile up at the ceiling and stop distinguishing them --
// the stale-saturation pitfall REFERENCE.md 3.8 names.
TEST_CASE("ordering: history aging preserves the relative ranking", "[ordering]") {
	search::HistoryTable history;
	for (int i = 0; i < 8000; i++) {
		history.update(WHITE, Move{a1, a2}, 8);
		if (i % 2 == 0) history.update(WHITE, Move{b1, b2}, 8);
	}

	CHECK(history.get(WHITE, Move{a1, a2}) > history.get(WHITE, Move{b1, b2}));
	CHECK(history.get(WHITE, Move{b1, b2}) > 0);
}

TEST_CASE("ordering: clear empties the history table", "[ordering]") {
	search::HistoryTable history;
	history.update(WHITE, Move{a1, a2}, 5);
	REQUIRE(history.get(WHITE, Move{a1, a2}) > 0);

	history.clear();

	CHECK(history.get(WHITE, Move{a1, a2}) == 0);
}
