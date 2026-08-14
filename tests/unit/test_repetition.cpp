#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "movegen/attacks.h"
#include "position/history.h"
#include "position/position.h"
#include "search/search.h"

// Draw-by-repetition tests (REFERENCE.md 3.4/3.8): the rule itself, and the
// two things the search has to get right -- seeing a repetition it walks into
// inside its own tree, and seeing a threefold completed by a position that was
// only ever reached in the moves actually played.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

bool same_move(Move a, Move b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

// Builds `history` by playing `moves` from `fen`, exactly as the UCI layer
// does for `position ... moves`, and returns the position they lead to.
Position play(const char* fen, const std::vector<Move>& moves, PositionHistory& history) {
	Position pos = parse_fen(fen);
	history.clear();
	for (Move m : moves) {
		history.push(pos.zobrist_key);
		StateInfo undo;
		make_move(pos, m, undo);
	}
	return pos;
}

// White is a queen down and can only shuffle its king between g1 and h2;
// black's king shuffles between g8 and h8. Every position is reachable again
// in two plies, and material is lopsided enough that a repetition draw (0) is
// unmistakable next to the alternative (about -900).
constexpr const char* kShuffleFen = "q5k1/8/8/8/8/8/8/6K1 w - - 0 1";

}  // namespace

TEST_CASE("repetition: a position repeated inside the search tree is a draw", "[repetition]") {
	Position pos = parse_fen(kShuffleFen);
	pos.halfmove_clock = 8;  // the window the scan is allowed to look back over

	PositionHistory history;
	history.push(0xAAAA);       // ply 0 (the search root)
	history.push(0xBBBB);       // ply 1
	history.push(pos.zobrist_key);  // ply 2 -- same side to move as `pos`, and the same position
	history.push(0xCCCC);       // ply 3

	// One earlier occurrence, and it was reached by the search itself: a draw.
	CHECK(is_repetition_draw(history, pos, /*ply=*/4));
}

TEST_CASE("repetition: a played position needs a third occurrence, not a second", "[repetition]") {
	Position pos = parse_fen(kShuffleFen);
	pos.halfmove_clock = 8;

	PositionHistory history;
	history.push(pos.zobrist_key);  // first occurrence, in the played game
	history.push(0xBBBB);
	history.push(0xCCCC);
	history.push(0xDDDD);

	// `ply` 0 means every entry predates the search: two occurrences on the
	// board (one in the history, one now) is a twofold, and play continues.
	CHECK_FALSE(is_repetition_draw(history, pos, /*ply=*/0));

	history.keys[2] = pos.zobrist_key;  // second occurrence -- now the board shows a threefold
	CHECK(is_repetition_draw(history, pos, /*ply=*/0));
}

TEST_CASE("repetition: an identical key with the other side to move doesn't count", "[repetition]") {
	// Impossible in a real line -- side to move is part of the Zobrist key --
	// but it pins down that the scan steps two plies at a time rather than one.
	Position pos = parse_fen(kShuffleFen);
	pos.halfmove_clock = 8;

	PositionHistory history;
	history.push(pos.zobrist_key);
	history.push(0xBBBB);
	history.push(0xCCCC);

	CHECK_FALSE(is_repetition_draw(history, pos, /*ply=*/3));
}

TEST_CASE("repetition: positions before the last irreversible move are out of reach", "[repetition]") {
	Position pos = parse_fen(kShuffleFen);

	PositionHistory history;
	history.push(pos.zobrist_key);
	history.push(0xBBBB);
	history.push(0xCCCC);
	history.push(0xDDDD);

	pos.halfmove_clock = 4;  // scan reaches entry 0
	CHECK(is_repetition_draw(history, pos, /*ply=*/4));

	// A capture or pawn move at ply 1 reset the clock, so the position at ply 0
	// can never come back -- the matching key must be a Zobrist collision.
	pos.halfmove_clock = 3;
	CHECK_FALSE(is_repetition_draw(history, pos, /*ply=*/4));
}

TEST_CASE("repetition: search takes a perpetual check over losing material", "[repetition][search]") {
	// White is a rook down, but 1. Qh5+ Kg8 2. Qe8+ Kh7 3. Qh5+ repeats the
	// position after move 1, and every black reply is forced (no square but the
	// one, nothing to block or capture with). The draw is only visible if the
	// search recognizes the repeat at ply 5 -- without it the line just
	// evaluates to the material deficit.
	Position pos = parse_fen("8/r5pk/8/8/8/8/q4PPP/3Q2K1 w - - 0 1");

	search::SearchLimits limits;
	limits.depth = 6;
	search::TranspositionTable tt(16);
	std::atomic<bool> stop{false};
	search::SearchResult result = search::think(pos, limits, tt, stop);

	CHECK(result.score == 0);
	CHECK(same_move(result.best_move, Move{d1, h5}));
}

TEST_CASE("repetition: search claims a threefold completed by the played game", "[repetition][search]") {
	// The same two positions have already been on the board twice each, so
	// White's Kg1-h2 is the third occurrence and ends the game. Nothing in the
	// search tree repeats at depth 2 -- only the moves played before the root
	// make this a draw, which is exactly what a search that ignores the game
	// history cannot see.
	PositionHistory history;
	Position pos = play(kShuffleFen,
	                    {{g1, h2}, {g8, h8}, {h2, g1}, {h8, g8},
	                     {g1, h2}, {g8, h8}, {h2, g1}, {h8, g8}},
	                    history);

	search::SearchLimits limits;
	limits.depth = 2;
	search::TranspositionTable tt(16);
	std::atomic<bool> stop{false};
	search::SearchResult result = search::think(pos, limits, tt, stop, nullptr, history);

	CHECK(result.score == 0);
	CHECK(same_move(result.best_move, Move{g1, h2}));

	// Same position, same depth, no history: White is simply a queen down.
	Position fresh = parse_fen(kShuffleFen);
	tt.clear();
	search::SearchResult no_history = search::think(fresh, limits, tt, stop);
	CHECK(no_history.score < -500);
}
