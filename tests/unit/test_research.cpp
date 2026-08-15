#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "core/move.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Re-search correctness (Milestone 6): both of that milestone's heuristics work
// by searching something cheaper than the real question and repeating the
// search when the cheap answer turns out not to settle it. Aspiration windows
// re-search a depth when the score escapes the window; LMR re-searches a move
// at full depth when the reduced search says it beats alpha. Neither is
// supposed to reach a conclusion the plain search wouldn't have, and this file
// is where that is checked -- against a "safe mode" search in the same binary,
// which is what search::SearchTuning exists for.
//
// The two heuristics get different tests because they make different promises,
// and blurring that is the easiest way to write a test here that looks strong
// and asserts nothing:
//
//   * Aspiration windows promise *exactness*. A search whose score lands inside
//     its window has computed the same value a full-window search would have,
//     and a score that escapes is not accepted -- it widens and searches again.
//     So "aspiration on" and "aspiration off" must agree, and the test asserts
//     equality outright.
//
//   * LMR promises no such thing. A reduced move that fails low is believed
//     without verification, which is the entire saving, so the reduced search
//     can genuinely miss a good move. What it cannot do is the opposite --
//     promote a move on shallow evidence -- because a reduced move that beats
//     alpha is always re-searched at full depth before it is believed. That
//     asymmetry, not equality, is what the LMR tests below pin.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// A spread of position types rather than a long list: an opening, a dense
// middlegame, a pawn endgame, and a tactical position, matching
// bench/search_bench and test_search_nodes.cpp so a failure here can be lined
// up against a node count there.
struct TestPosition {
	const char* name;
	const char* fen;
};

constexpr TestPosition kPositions[] = {
	{"opening", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
	{"middlegame (Kiwipete)",
	 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
	{"endgame (K+P)", "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1"},
	{"tactical (WAC.019)",
	 "r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1"},
};

search::SearchResult search_with(const char* fen, int depth, const search::SearchTuning& tuning) {
	Position pos = parse_fen(fen);
	search::SearchLimits limits;
	limits.depth = depth;
	// Fresh table per search: a table carried between the two halves of a
	// comparison would let the second search read the first one's conclusions,
	// which is precisely the thing being compared.
	search::TranspositionTable tt(16);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	return search::think(pos, limits, tt, move_history, stop, nullptr, PositionHistory{}, tuning);
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

// The property the milestone asks for, for the half of it that can promise it.
//
// The comparison is against SearchTuning::exact() -- the safe mode -- with the
// aspiration window switched back on for one side and nothing else different.
// Comparing against the *playing* configuration instead would not test the
// window at all, because three of the engine's other techniques read the window
// before deciding what to skip:
//
//   - The transposition table stores bounds where a wide window stored exact
//     scores, so a narrowed search fills a weaker table and a later probe cuts
//     off less often. The window's arithmetic is untouched; what changes is how
//     much the table can answer later. That effect is real and costs a ply in
//     the K+P endgame -- measured in test_nullmove.cpp, and pinned as its own
//     case below rather than left as a footnote.
//   - Null-move pruning searches against beta, so a different beta prunes
//     different subtrees.
//   - Delta pruning discards captures that cannot reach alpha, so a different
//     alpha discards a different set.
//
// Each of those is free to change a score, which makes them the wrong yardstick
// for a technique that isn't. With all three held still, what is left is the
// question worth asking: does the window itself ever return a bound as though
// it were a score?
//
// Depth 6 rather than something shallower: kAspirationMinDepth is 4, so this
// exercises three successive windows, each seeded by the last one's score. At
// depth 4 a single window either fits or it doesn't and the test would pass
// without the loop ever running twice. Deeper would be better still and is what
// this started at, but safe mode has no pruning in it at all -- depth 8 across
// these four positions ran for three minutes in a Debug build, which is not a
// price worth paying on every CI run for a third data point.
//
// The score is asserted and the best move is not, which is a real distinction
// rather than a weakened test. Alpha-beta guarantees the *value* of a position;
// when several moves share that value -- and in the opening several do, four
// moves scoring 33 at this depth -- which one is kept comes down to the order
// they were tried in and which of them beat the running alpha at the time. A
// narrower window legitimately changes that, and a test that pinned it would be
// pinning search order under the name of correctness.
TEST_CASE("aspiration windows: the window never changes the search result",
          "[search][aspiration][research]") {
	search::SearchTuning windowed = search::SearchTuning::exact();
	windowed.aspiration_windows = true;

	const search::SearchTuning full_window = search::SearchTuning::exact();

	for (const TestPosition& position : kPositions) {
		INFO("position: " << position.name
		      << "\nA failing window must be re-searched wider, never accepted. If this "
		         "fails, the aspiration loop is returning a bound as though it were a score.");
		search::SearchResult aspirated = search_with(position.fen, 6, windowed);
		search::SearchResult reference = search_with(position.fen, 6, full_window);

		CHECK(aspirated.score == reference.score);
	}
}

// Same claim, made where it is hardest: a position whose score moves sharply
// between plies is the one that makes the window fail and the re-search run. If
// the aspiration loop ever accepted a fail-high or fail-low as a final answer,
// this is where it would show.
//
// WAC.019 qualifies -- it is won by a sacrifice, so the score jumps as the
// compensation comes into view rather than drifting a few centipawns per ply.
TEST_CASE("aspiration windows: a score that moves between plies still comes back exact",
          "[search][aspiration][research]") {
	search::SearchTuning windowed = search::SearchTuning::exact();
	windowed.aspiration_windows = true;

	const search::SearchTuning full_window = search::SearchTuning::exact();

	const char* fen = "r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1";
	for (int depth = 4; depth <= 7; depth++) {
		INFO("depth: " << depth);
		CHECK(search_with(fen, depth, windowed).score == search_with(fen, depth, full_window).score);
	}
}

// The claim the test above deliberately does not make, pinned as its own case
// so that it reads as a known property rather than as this file's tests being
// quietly weakened.
//
// With the table's cutoffs left on -- which is how the engine actually plays --
// an aspirating search and a full-window one may return different scores, and
// in the K+P endgame they do. Asserting the difference rather than merely
// noting it means that if a future change makes aspiration table-neutral, this
// fails and says so, instead of the project carrying a caveat that stopped
// being true.
//
// Only the disagreement is asserted here. Its counterpart -- that the two agree
// once the cutoffs are off -- is what the two tests above establish, and
// re-establishing it on *this* position would mean searching a pawn endgame to
// depth 15 with the table answering nothing, which costs hundreds of millions
// of nodes. It was run by hand at depth 17 while diagnosing this (both sides:
// +262) and is deliberately not paid for on every CI run.
TEST_CASE("aspiration windows: the table, not the window, is what moves the score",
          "[search][aspiration][research][tt]") {
	search::SearchTuning windowed;
	windowed.late_move_reductions = false;

	search::SearchTuning full_window = windowed;
	full_window.aspiration_windows = false;

	// Depth 15 is the shallowest depth at which the two diverge in this
	// position, and so the cheapest place to pin it.
	const char* kp_endgame = "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1";
	CHECK(search_with(kp_endgame, 15, windowed).score !=
	      search_with(kp_endgame, 15, full_window).score);
}

// The LMR half. The promise is one-directional -- a reduced move is never
// believed on its shallow score alone once it beats alpha -- and this is the
// position that would catch a missing re-search: the winning move is a quiet
// one, so it is exactly the kind LMR reduces.
//
// White is a rook down and mates by 1.Ra1-a8, a quiet rook move to an empty
// square with no capture and no check until it arrives. If the reduced search's
// fail-high were accepted at its reduced-depth score instead of being
// re-searched at full depth, the mate score would never be established and the
// engine would have no reason to prefer this move over any other rook move.
TEST_CASE("LMR: a reduced move that beats alpha is re-searched, not believed",
          "[search][lmr][research]") {
	search::SearchTuning reduced;  // defaults: both heuristics on
	search::SearchTuning full_depth;
	full_depth.late_move_reductions = false;

	// Back-rank mate in one, and a board with enough quiet rook moves that the
	// mating move cannot be reached before kLmrFullDepthMoves runs out at every
	// node on the way.
	const char* fen = "6k1/5ppp/8/8/8/8/8/R6K w - - 0 1";

	search::SearchResult with_lmr = search_with(fen, 6, reduced);
	search::SearchResult without_lmr = search_with(fen, 6, full_depth);

	CHECK(move_to_uci(with_lmr.best_move) == "a1a8");
	CHECK(with_lmr.score >= search::kMateScore - search::kMaxPly);
	// Not merely "finds a mate": the same mate, at the same distance, that the
	// unreduced search finds.
	CHECK(with_lmr.score == without_lmr.score);
	CHECK(move_to_uci(with_lmr.best_move) == move_to_uci(without_lmr.best_move));
}

// The weaker but broader LMR claim, and the one worth having as a regression
// guard: reductions may cost the search a move it would otherwise have found,
// but they must not cost it *material it can see*. A reduction bug that skipped
// the full-depth re-search, or reduced captures, would show up as the engine
// quietly declining free pieces, and this is the cheapest position that catches
// that.
TEST_CASE("LMR: reductions do not hide a free piece", "[search][lmr][research]") {
	search::SearchTuning reduced;

	for (int depth = 3; depth <= 8; depth++) {
		INFO("depth: " << depth);
		// h1h8 wins an undefended rook; nothing else on the board comes close.
		search::SearchResult result = search_with("4k2r/8/8/8/8/8/8/4K2R w - - 0 1", depth, reduced);
		CHECK(move_to_uci(result.best_move) == "h1h8");
	}
}

// Reductions must not turn a mate the search can see into one it can't. Mate
// scores are the one part of the score range where being one ply short changes
// the answer categorically rather than by a few centipawns, so they get their
// own check across a range of depths -- a reduction that is too aggressive at
// one particular depth would otherwise slip through a single-depth test.
TEST_CASE("LMR: forced mates survive the reductions", "[search][lmr][research]") {
	search::SearchTuning reduced;
	search::SearchTuning full_depth;
	full_depth.late_move_reductions = false;

	const char* fen = "6k1/5ppp/8/8/8/8/8/R6K w - - 0 1";
	for (int depth = 2; depth <= 8; depth++) {
		INFO("depth: " << depth);
		CHECK(search_with(fen, depth, reduced).score == search_with(fen, depth, full_depth).score);
	}
}
