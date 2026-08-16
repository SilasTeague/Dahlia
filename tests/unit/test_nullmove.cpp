#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "movegen/attacks.h"
#include "position/position.h"
#include "position/zobrist.h"
#include "search/search.h"

// Null-move pruning and its zugzwang guard (REFERENCE.md 3.8 responsibility 8,
// Milestone 5), which the milestone names as needing a dedicated regression
// test: "a known K+P endgame position where naive null-move fails, confirming
// the guard works".
//
// Two layers, because they fail for different reasons and a test that mixes
// them tells you nothing about which broke:
//
//   1. make_null_move/unmake_null_move restore the position exactly -- the
//      same property test make_move/unmake_move gets, for the same reason.
//   2. The guard changes what the search concludes in a pawn endgame.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

bool same_position(const Position& a, const Position& b) {
	if (a.side_to_move != b.side_to_move || a.castling_rights != b.castling_rights ||
	    a.en_passant_square != b.en_passant_square || a.halfmove_clock != b.halfmove_clock ||
	    a.fullmove_count != b.fullmove_count || a.zobrist_key != b.zobrist_key) {
		return false;
	}
	for (int color = 0; color < 2; color++) {
		for (int piece = 0; piece < 6; piece++) {
			if (a.pieces[color][piece] != b.pieces[color][piece]) return false;
		}
	}
	for (int square = 0; square < 64; square++) {
		if (a.board[square] != b.board[square]) return false;
	}
	for (int i = 0; i < 3; i++) {
		if (a.aggregates[i] != b.aggregates[i]) return false;
	}
	return true;
}

search::SearchResult search_at_depth(const char* fen, int depth) {
	Position pos = parse_fen(fen);
	search::SearchLimits limits;
	limits.depth = depth;
	search::TranspositionTable tt(16);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	return search::think(pos, limits, tt, move_history, stop);
}

}  // namespace

TEST_CASE("null move: make/unmake restores the position exactly", "[nullmove][position]") {
	const char* fens[] = {
		"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
		// En passant available: the square and its Zobrist contribution both
		// have to come back.
		"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
		"8/8/4k3/8/8/4K3/4P3/8 b - - 12 40",
	};

	for (const char* fen : fens) {
		INFO("fen: " << fen);
		Position pos = parse_fen(fen);
		const Position before = pos;

		StateInfo undo;
		make_null_move(pos, undo);
		unmake_null_move(pos, undo);

		CHECK(same_position(pos, before));
	}
}

TEST_CASE("null move: passing gives the turn away and clears en passant", "[nullmove][position]") {
	// The en passant square must not survive a pass. Leaving it set would let
	// the side that just declined to capture be captured *by* it a move later,
	// and would give two genuinely different positions the same Zobrist key.
	Position pos = parse_fen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
	const Bitboard occupancy_before = pos.aggregates[ALL];

	StateInfo undo;
	make_null_move(pos, undo);

	CHECK(pos.side_to_move == BLACK);
	CHECK(pos.en_passant_square == NULL_SQUARE);
	CHECK(pos.castling_rights == ANY_CASTLING);
	// Not a piece moved: that is the entire point of the null move.
	CHECK(pos.aggregates[ALL] == occupancy_before);
	CHECK(pos.zobrist_key == compute_zobrist_key(pos));
}

TEST_CASE("null move: the zugzwang guard sees pawns-and-king positions", "[nullmove][position]") {
	// The predicate the guard is built on, tested directly so a search-level
	// failure can be told apart from a broken bitboard query.
	Position pawns_only = parse_fen("8/8/4k3/8/8/4K3/4P3/8 w - - 0 1");
	CHECK_FALSE(has_non_pawn_material(pawns_only, WHITE));
	CHECK_FALSE(has_non_pawn_material(pawns_only, BLACK));

	Position with_a_knight = parse_fen("8/8/4k3/8/8/4KN2/4P3/8 w - - 0 1");
	CHECK(has_non_pawn_material(with_a_knight, WHITE));
	CHECK_FALSE(has_non_pawn_material(with_a_knight, BLACK));

	Position start = parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	CHECK(has_non_pawn_material(start, WHITE));
	CHECK(has_non_pawn_material(start, BLACK));
}

// The regression test the milestone asks for, and it is a real one: this
// position is the search benchmark's K+P endgame, and the guard is worth about
// 700 centipawns of judgement in it.
//
// White to move wins by 1.Ke4, taking the opposition -- the pawn promotes and
// the score jumps to roughly +980. Rebuilding the engine with the guard
// removed and nothing else changed, the same search at the same depth returns
// **+254** and plays 1.Kd4 instead: null-move pruning assumes passing is worse
// than any legal move, and in a pawn endgame that assumption is exactly
// backwards, so the free move handed over in the null search refutes lines that
// no real move can refute.
//
// Depth 18 is the shallowest depth at which the win is visible at all, which is
// also why this is pinned rather than left to the tactics suite: a change that
// quietly costs the engine a ply here would go unnoticed everywhere else.
//
// It was depth 17 until Milestone 6, and the ply it moved by is worth
// explaining, because the cause is not the one it looks like. Aspiration
// windows -- not LMR -- are what moved it, and they did so *without being
// inexact*: rebuilt with the TT's score cutoffs disabled, an aspiration build
// and a full-window build return the identical score (+262) at depth 17, so the
// narrow window provably changes no search result. What it changes is what
// lands in the transposition table. A narrow-window search stores bounds where
// a full-window search stored exact scores, and this position is dense with
// transpositions, so the engine had been reading a real extra ply or two of
// effective depth out of those exact entries. That is the same TT
// bound-versus-exact effect Milestone 5 traced in this very position when PVS
// landed (REFERENCE.md 3.8), showing up a second time and for the same reason.
//
// The ply is given back on the clock, which is the only place it matters: the
// engine now reaches the win in 72 ms at depth 18 where it used to need 79 ms
// at depth 17.
TEST_CASE("null move: the zugzwang guard keeps a won pawn endgame won",
          "[nullmove][search][zugzwang]") {
	search::SearchResult result = search_at_depth("8/8/4k3/8/8/4K3/4P3/8 w - - 0 1", 18);

	// A win, not "slightly better": without the guard this is +254.
	CHECK(result.score > 500);
	CHECK(result.best_move.from == e3);
	CHECK(result.best_move.to == e4);
}
