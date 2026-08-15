#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "core/move.h"
#include "eval/evaluate.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Quiescence search tests (REFERENCE.md 3.8 responsibility 3, Milestone 4),
// which names stand-pat and delta pruning as the conditions to isolate and
// test explicitly, because they are where quiescence bugs hide.
//
// quiescence() itself is internal to search.cpp -- it shares the search's
// mutable state (node count, stop flag, deadline, history table), and giving
// it a public signature would mean inventing a context type that exists only
// for these tests. It is exercised through think() at depth 1 instead, where
// every root move leads directly into a quiescence node, so the assertions
// below are about the interface the engine actually uses.

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

// What a depth-1 search is worth if quiescence does nothing but stand pat:
// the best static score the side to move can step into, over the moves that
// leave the opponent no capture to answer with.
//
// Computed here rather than written down as a literal, because since Milestone
// 5 the evaluation is positional -- every move changes the score, so the old
// "a quiet move leaves the material count alone" shortcut is gone, and a
// hardcoded number would pin the piece-square tables instead of the search.
// This walks the same moves the search does and applies only the rule under
// test: nobody is obliged to capture.
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
		// A move the opponent can answer with a capture leads to a node
		// quiescence will search rather than stand pat on, so it says nothing
		// about the floor being tested here.
		if (!capture_available) {
			int16_t score = static_cast<int16_t>(-eval::evaluate(pos));
			if (score > best) best = score;
		}

		unmake_move(pos, moves.moves[i], undo);
	}
	return best;
}

// White queen on d1, black pawn on d5 defended twice (c6 and e6). Qxd5 wins a
// pawn on the board and loses the queen on the very next ply.
constexpr const char* kHorizonFen = "4k3/8/2p1p3/3p4/8/8/8/3QK3 w - - 0 1";

}  // namespace

// The regression this whole feature exists to prevent. Before quiescence, a
// depth-1 search of kHorizonFen returned Qxd5 with a score of +700 -- it saw
// the pawn come off and stopped looking, one ply before cxd5. The search depth
// is unchanged; only the leaf handling is different.
TEST_CASE("quiescence: refuses a capture that loses material one ply past the horizon",
          "[quiescence][search]") {
	search::SearchResult result = search_to_depth(kHorizonFen, 1);

	CHECK_FALSE(same_move(result.best_move, Move{d1, d5}));
}

// Stand pat: the side to move is never obliged to capture, so a node's score
// can never be dragged below its static evaluation by the availability of a
// bad capture.
//
// The exact-equality check is the point. Every capture available to White here
// loses material, so the node is worth the best quiet move on the board and
// nothing else: White steps the queen somewhere, Black -- who then has no
// capture at all -- stands pat and returns its own static score. Any bug that
// forced a capture instead of standing pat, or that lost the sign on the way
// back up, moves this number.
TEST_CASE("quiescence: stand-pat floors a node at its static evaluation", "[quiescence][search]") {
	search::SearchResult result = search_to_depth(kHorizonFen, 1);

	CHECK(result.score == best_static_reply(kHorizonFen));
}

// The mirror of the test above: declining bad captures must not turn into
// declining good ones. A queen hanging on d5 with nothing defending it is
// exactly what quiescence exists to find, and it is also the case delta
// pruning could plausibly break -- a margin applied in the wrong direction
// prunes the winning capture instead of the pointless ones.
TEST_CASE("quiescence: still takes a genuinely free piece", "[quiescence][search]") {
	search::SearchResult result = search_to_depth("4k3/8/8/3q4/8/8/8/3QK3 w - - 0 1", 1);

	CHECK(same_move(result.best_move, Move{d1, d5}));
	CHECK(result.score > 800);
}

// Termination is structural, not a depth limit: every move quiescence searches
// either removes a piece from the board or promotes a pawn, and both are
// bounded. Kiwipete is the densest capture position in the test set -- eight
// captures at the root, several of them leading to real exchange sequences --
// so a quiescence that failed to terminate would hang here rather than return
// a number.
//
// The node bound is what makes this an assertion rather than a hope: a
// depth-1 search that visits fewer than 100k nodes has demonstrably resolved
// every capture sequence instead of wandering.
TEST_CASE("quiescence: terminates on a capture-dense position", "[quiescence][search]") {
	search::SearchResult result = search_to_depth(
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1);

	CHECK(result.best_move.from != NULL_SQUARE);
	CHECK(result.nodes < 100000);
}

// A position with no captures available anywhere is the degenerate case:
// quiescence should return the static evaluation immediately and add nothing
// to the tree. White is a rook up, and no capture exists for either side at
// any depth, so the search cannot be doing anything but standing pat at every
// leaf.
TEST_CASE("quiescence: a position with no captures scores as its static evaluation",
          "[quiescence][search]") {
	constexpr const char* kQuietFen = "4k3/8/8/8/8/8/8/4K2R w - - 0 1";
	search::SearchResult result = search_to_depth(kQuietFen, 1);

	CHECK(result.score == best_static_reply(kQuietFen));
	// Still recognisably a rook up: the positional terms move the number, they
	// don't drown the material in it.
	CHECK(result.score > 400);
}
