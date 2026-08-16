#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "movegen/attacks.h"
#include "position/position.h"
#include "position/zobrist.h"
#include "search/search.h"

// Null-move pruning and its zugzwang guard, in two layers that fail for
// different reasons: make/unmake round-tripping, and the guard changing what the
// search concludes in a pawn endgame. See docs/search.md#null-move-pruning.

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
		// En passant: the square and its Zobrist contribution both have to come back.
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
	// The en passant square must not survive a pass: leaving it set would give two
	// genuinely different positions the same Zobrist key.
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
	// The predicate itself, so a search failure can be told from a broken query.
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

// The benchmark's K+P endgame: 1.Ke4 takes the opposition and promotes, while a
// guard-less build plays 1.Kd4 and scores +254. Depth 18 is the shallowest the
// win is visible at -- it was 17 until aspiration windows landed, for a reason
// worth reading: docs/results.md#the-interesting-failure.
TEST_CASE("null move: the zugzwang guard keeps a won pawn endgame won",
          "[nullmove][search][zugzwang]") {
	search::SearchResult result = search_at_depth("8/8/4k3/8/8/4K3/4P3/8 w - - 0 1", 18);

	// A win, not "slightly better": without the guard this is +254.
	CHECK(result.score > 500);
	CHECK(result.best_move.from == e3);
	CHECK(result.best_move.to == e4);
}
