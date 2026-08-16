#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "core/move.h"
#include "eval/evaluate.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// quiescence() is internal to search.cpp, so it is exercised through think() at
// depth 1, where every root move leads directly into a quiescence node.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

bool same_move(Move a, Move b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

search::SearchResult search_to_depth(const char* fen, int depth) {
	Position pos = parse_fen(fen);
	search::SearchLimits limits;
	limits.depth = depth;
	search::TranspositionTable tt(1);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	return search::think(pos, limits, tt, move_history, stop);
}

// What a depth-1 search is worth if quiescence only ever stands pat. Computed
// rather than hardcoded: with a positional evaluation a literal would pin the
// piece-square tables instead of the search.
int16_t best_static_reply(const char* fen) {
	Position pos = parse_fen(fen);
	MoveList moves;
	generate_legal_moves(moves, pos);

	int16_t best = -search::kInfiniteScore;
	for (int i = 0; i < moves.count; i++) {
		StateInfo undo;
		make_move(pos, moves.moves[i], undo);

		MoveList replies;
		generate_legal_moves(replies, pos);
		bool capture_available = false;
		for (int j = 0; j < replies.count; j++) {
			if (search::is_capture(pos, replies.moves[j])) capture_available = true;
		}
		// A capture available to the opponent means quiescence searches rather
		// than stands pat there, so it says nothing about this floor.
		if (!capture_available) {
			int16_t score = static_cast<int16_t>(-eval::evaluate(pos));
			if (score > best) best = score;
		}

		unmake_move(pos, moves.moves[i], undo);
	}
	return best;
}

// Qxd5 wins a pawn defended twice, and loses the queen on the very next ply.
constexpr const char* kHorizonFen = "4k3/8/2p1p3/3p4/8/8/8/3QK3 w - - 0 1";

}  // namespace

// Before quiescence this scored Qxd5 at +700, watching the pawn come off and
// stopping one ply before cxd5 -- same depth, different leaf handling.
TEST_CASE("quiescence: refuses a capture that loses material one ply past the horizon",
          "[quiescence][search]") {
	search::SearchResult result = search_to_depth(kHorizonFen, 1);

	CHECK_FALSE(same_move(result.best_move, Move{d1, d5}));
}

// Every capture here loses material, so the node is worth exactly the best quiet
// move -- a forced capture, or a lost sign, moves this number.
TEST_CASE("quiescence: stand-pat floors a node at its static evaluation", "[quiescence][search]") {
	search::SearchResult result = search_to_depth(kHorizonFen, 1);

	CHECK(result.score == best_static_reply(kHorizonFen));
}

// The mirror of the test above: declining bad captures must not become declining
// good ones, which is what a delta margin applied backwards would do.
TEST_CASE("quiescence: still takes a genuinely free piece", "[quiescence][search]") {
	search::SearchResult result = search_to_depth("4k3/8/8/3q4/8/8/8/3QK3 w - - 0 1", 1);

	CHECK(same_move(result.best_move, Move{d1, d5}));
	CHECK(result.score > 800);
}

// Termination is structural, not a depth limit. Kiwipete is the densest capture
// position in the set, and the node bound is what makes this an assertion rather
// than a hope: under 100k nodes means the sequences resolved.
TEST_CASE("quiescence: terminates on a capture-dense position", "[quiescence][search]") {
	search::SearchResult result = search_to_depth(
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1);

	CHECK(result.best_move.from != NULL_SQUARE);
	CHECK(result.nodes < 100000);
}

// The degenerate case: no capture exists for either side at any depth, so the
// search can only be standing pat at every leaf.
TEST_CASE("quiescence: a position with no captures scores as its static evaluation",
          "[quiescence][search]") {
	constexpr const char* kQuietFen = "4k3/8/8/8/8/8/8/4K2R w - - 0 1";
	search::SearchResult result = search_to_depth(kQuietFen, 1);

	CHECK(result.score == best_static_reply(kQuietFen));
	// Still recognisably a rook up: positional terms move the number, not drown it.
	CHECK(result.score > 400);
}
