#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "core/move.h"
#include "movegen/attacks.h"
#include "position/position.h"
#include "search/search.h"

// Tactical suite (REFERENCE.md 1.6, Milestone 4): a checked-in subset of Win
// At Chess, the standard tactics EPD set, run at a fixed depth.
//
// Fixed depth rather than fixed time, for the same reason the node-count test
// is: a time-limited test passes or fails depending on how loaded the CI runner
// is, and a tactics suite that fails intermittently gets ignored. Every
// position below is solved deterministically at kDepth on any machine.
//
// Embedded as a table rather than read from an .epd file so the test has no
// working-directory dependency -- ctest, an IDE runner, and a bare
// ./dahlia_unit_tests all behave identically. The `bm` field of each source
// record is translated to UCI here; the WAC id is kept so a position can be
// traced back to the published suite.
//
// What this suite is for: it is a *regression* test, not a strength estimate.
// Milestone 4's move ordering and quiescence made the engine solve one more of
// these than it did before (14/18 at depth 7, up from 13 -- see the known
// failures below); the value of checking them in is that a future search change
// which quietly stops finding one of them fails here instead of costing rating
// points in a match nobody has run yet.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// Deep enough that every position below is found, shallow enough that the
// whole suite runs in a few seconds in a Debug build with sanitizers on.
constexpr int kDepth = 7;

struct Puzzle {
	const char* id;
	const char* fen;
	const char* best_move;  // UCI
};

// Solved at kDepth as of Milestone 4. Positions the engine does not yet solve
// are listed in the test below this one rather than silently omitted.
constexpr Puzzle kSolved[] = {
	{"WAC.001", "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1", "g3g6"},
	{"WAC.003", "5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1", "e3g3"},
	{"WAC.004", "r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1", "h6h7"},
	{"WAC.005", "5k2/6pp/p1qN4/1p1p4/3P4/2PKP2Q/PP3r2/3R4 b - - 0 1", "c6c4"},
	{"WAC.006", "7k/p7/1R5K/6r1/6p1/6P1/8/8 w - - 0 1", "b6b7"},
	{"WAC.007", "rnbqkb1r/pppp1ppp/8/4P3/6n1/7P/PPPNPPP1/R1BQKBNR b KQkq - 0 1", "g4e3"},
	{"WAC.008", "r4q1k/p2bR1rp/2p2Q1N/5p2/5p2/2P5/PP3PPP/R5K1 w - - 0 1", "e7f7"},
	{"WAC.009", "3q1rk1/p4pp1/2pb3p/3p4/6Pr/1PNQ4/P1PB1PP1/4RRK1 b - - 0 1", "d6h2"},
	{"WAC.010", "2br2k1/2q3rn/p2NppQ1/2p1P3/Pp5R/4P3/1P3PPP/3R2K1 w - - 0 1", "h4h7"},
	{"WAC.012", "4k1r1/2p3r1/1pR1p3/3pP2p/3P2qP/P4N2/1PQ4P/5R1K b - - 0 1", "g4f3"},
	{"WAC.013", "5rk1/pp4p1/2n1p2p/2Npq3/2p5/6P1/P3P1BP/R4Q1K w - - 0 1", "f1f8"},
	{"WAC.014", "r2rb1k1/pp1q1p1p/2n1p1p1/2bp4/5P2/PP1BPR1Q/1BPN2PP/R5K1 w - - 0 1", "h3h7"},
	{"WAC.015", "1R6/1brk2p1/4p2p/p1P1Pp2/P7/6P1/1P4P1/2R3K1 w - - 0 1", "b8b7"},
	{"WAC.019", "r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1", "c4f7"},
};

std::string move_to_uci(Move m) {
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

std::string best_move_at_depth(const char* fen, int depth) {
	Position pos = parse_fen(fen);
	search::SearchLimits limits;
	limits.depth = depth;
	search::TranspositionTable tt(16);
	search::HistoryTable move_history;
	std::atomic<bool> stop{false};
	return move_to_uci(search::think(pos, limits, tt, move_history, stop).best_move);
}

}  // namespace

TEST_CASE("tactics: solves the checked-in Win At Chess subset", "[tactics][search]") {
	for (const Puzzle& puzzle : kSolved) {
		INFO("position: " << puzzle.id << " (" << puzzle.fen << ") at depth " << kDepth);
		CHECK(best_move_at_depth(puzzle.fen, kDepth) == puzzle.best_move);
	}
}

// Recorded, not hidden. All four of these are positions where the right move is
// found only by an evaluation that understands something beyond material --
// pawn structure, king safety, the value of the bishop pair -- which is
// Milestone 5's work, not Milestone 4's. Deeper search does not fix them:
// they fail identically at one second per move as at depth 7.
//
// This is a documentation test. It asserts nothing about the failures; it
// exists so the list lives next to the suite it belongs to, and so that a
// future milestone which starts solving them has an obvious place to move
// the entry to.
TEST_CASE("tactics: known failures are evaluation-limited, not search-limited",
          "[tactics][!mayfail]") {
	constexpr Puzzle kKnownFailures[] = {
		{"WAC.002", "8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - 0 1", "b3b2"},
		{"WAC.011", "r1b1kb1r/3q1ppp/pBp1pn2/8/Np3P2/5B2/PPP3PP/R2Q1RK1 w kq - 0 1", "b6c7"},
		{"WAC.022", "8/p7/1ppk1n2/5ppp/P1PP4/2P1K1P1/5N1P/8 b - - 0 1", "f6g4"},
		{"WAC.030", "r1b1k2r/1pp1q2p/p1n3p1/3QPp2/8/1BP3B1/P5PP/3R1RK1 w kq - 0 1", "d5g8"},
	};

	for (const Puzzle& puzzle : kKnownFailures) {
		INFO("position: " << puzzle.id << " -- expected to fail until evaluation improves");
		CHECK(best_move_at_depth(puzzle.fen, kDepth) == puzzle.best_move);
	}
}
