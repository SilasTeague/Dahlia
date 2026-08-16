#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "core/move.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Re-search correctness, checked against a "safe mode" search in the same binary
// (search::SearchTuning::exact). The two heuristics make different promises, so
// they get different tests: aspiration windows must return the identical score a
// full-window search would, while LMR must merely never promote a move on
// reduced-depth evidence. See docs/testing.md.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// The same four positions as bench/search_bench and test_search_nodes.cpp.
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
	// Fresh per search: a shared table would let one half read the other's conclusions.
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

// Compared against safe mode: the TT, null-move and delta pruning all read the
// current window, so any of them could change a score. Depth 6 exercises three
// successive windows. The score is asserted and the best move is not -- which of
// several equal-scoring moves survives is search order, not correctness.
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

// The same claim where it is hardest: WAC.019 is won by a sacrifice, so its
// score jumps between plies rather than drifting, which is what makes the window
// fail and the re-search run.
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

// The claim the tests above deliberately do not make, asserted so that a future
// change making aspiration table-neutral fails here rather than leaving the
// project with a caveat that stopped being true. See docs/search.md.
TEST_CASE("aspiration windows: the table, not the window, is what moves the score",
          "[search][aspiration][research][tt]") {
	search::SearchTuning windowed;
	windowed.late_move_reductions = false;

	search::SearchTuning full_window = windowed;
	full_window.aspiration_windows = false;

	// The shallowest depth at which the two diverge here, so the cheapest place to pin it.
	const char* kp_endgame = "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1";
	CHECK(search_with(kp_endgame, 15, windowed).score !=
	      search_with(kp_endgame, 15, full_window).score);
}

// The mating move here (1.Ra1-a8) is quiet, so it is exactly the kind LMR
// reduces. Without the full-depth re-search the mate score is never established.
TEST_CASE("LMR: a reduced move that beats alpha is re-searched, not believed",
          "[search][lmr][research]") {
	search::SearchTuning reduced;  // defaults: both heuristics on
	search::SearchTuning full_depth;
	full_depth.late_move_reductions = false;

	// Enough quiet rook moves that kLmrFullDepthMoves runs out before the mate is reached.
	const char* fen = "6k1/5ppp/8/8/8/8/8/R6K w - - 0 1";

	search::SearchResult with_lmr = search_with(fen, 6, reduced);
	search::SearchResult without_lmr = search_with(fen, 6, full_depth);

	CHECK(move_to_uci(with_lmr.best_move) == "a1a8");
	CHECK(with_lmr.score >= search::kMateScore - search::kMaxPly);
	// The same mate at the same distance, not merely some mate.
	CHECK(with_lmr.score == without_lmr.score);
	CHECK(move_to_uci(with_lmr.best_move) == move_to_uci(without_lmr.best_move));
}

// Reductions may cost a move the search would otherwise have found, but never
// material it can see -- a skipped re-search shows up as declining free pieces.
TEST_CASE("LMR: reductions do not hide a free piece", "[search][lmr][research]") {
	search::SearchTuning reduced;

	for (int depth = 3; depth <= 8; depth++) {
		INFO("depth: " << depth);
		// h1h8 wins an undefended rook; nothing else on the board comes close.
		search::SearchResult result = search_with("4k2r/8/8/8/8/8/8/4K2R w - - 0 1", depth, reduced);
		CHECK(move_to_uci(result.best_move) == "h1h8");
	}
}

// Mate scores are where being one ply short changes the answer categorically, so
// they are checked across a range of depths rather than at one.
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
