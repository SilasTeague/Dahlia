#include <catch2/catch_test_macros.hpp>

#include "movegen/attacks.h"
#include "perft.h"

// Perft correctness suite (REFERENCE.md 3.5), using the real make_move/
// unmake_move rather than the throwaway copy-based Milestone-1 harness.
//
// Two things are checked at every depth:
//  - Total node count against the standard chess-programming-wiki reference
//    values (https://www.chessprogramming.org/Perft_Results) — the real
//    correctness oracle for movegen + make/unmake.
//  - Node count broken down by which piece type made the *root* move
//    (PerftBreakdown::by_piece). At depth 1 this is hand-computable and
//    checked as an independent oracle. At depth >=2 the same partition
//    necessarily has the same nonzero categories as depth 1 (it's still a
//    root-move partition), so those are checked too; the values themselves
//    were captured from this implementation only *after* the matching total
//    was independently verified against the reference count, so a future
//    regression that changes the shape of the tree without changing the
//    total (e.g. pawn/knight moves miscounted in offsetting ways) still
//    gets caught.
//
// Shallow depths run in CI by default; depth 6 (and Kiwipete depth 5) are
// tagged [.] (Catch2 "hidden") for manual/nightly runs per REFERENCE.md 1.6
// — Kiwipete depth 6 alone is ~8 billion nodes.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

constexpr const char* kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* kKiwipeteFen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

void check_breakdown(const PerftBreakdown& b, uint64_t nodes,
                      uint64_t pawn, uint64_t knight, uint64_t bishop,
                      uint64_t rook, uint64_t queen, uint64_t king) {
	CHECK(b.nodes == nodes);
	CHECK(b.by_piece[PAWN] == pawn);
	CHECK(b.by_piece[KNIGHT] == knight);
	CHECK(b.by_piece[BISHOP] == bishop);
	CHECK(b.by_piece[ROOK] == rook);
	CHECK(b.by_piece[QUEEN] == queen);
	CHECK(b.by_piece[KING] == king);
}

}  // namespace

// --- Start position ---
// Depth 1 is hand-computable: only pawns and knights have legal first moves
// (bishops/rooks/queen/king are all still blocked by their own pawns), so
// 8 pawns x 2 (single/double push) = 16, and both knights x 2 destinations
// each = 4; 16 + 4 = 20, matching the known depth-1 total.

TEST_CASE("perft: start position, depth 1-3", "[perft]") {
	Position pos = parse_fen(kStartFen);
	check_breakdown(perft_divide_by_piece_type(pos, 1), 20, 16, 4, 0, 0, 0, 0);
	check_breakdown(perft_divide_by_piece_type(pos, 2), 400, 320, 80, 0, 0, 0, 0);
	check_breakdown(perft_divide_by_piece_type(pos, 3), 8902, 7222, 1680, 0, 0, 0, 0);
}

TEST_CASE("perft: start position, depth 4", "[perft]") {
	Position pos = parse_fen(kStartFen);
	check_breakdown(perft_divide_by_piece_type(pos, 4), 197281, 160012, 37269, 0, 0, 0, 0);
}

TEST_CASE("perft: start position, depth 5", "[perft][.]") {
	Position pos = parse_fen(kStartFen);
	check_breakdown(perft_divide_by_piece_type(pos, 5), 4865609, 4000388, 865221, 0, 0, 0, 0);
}

TEST_CASE("perft: start position, depth 6", "[perft][.]") {
	Position pos = parse_fen(kStartFen);
	check_breakdown(perft_divide_by_piece_type(pos, 6), 119060324, 97894668, 21165656, 0, 0, 0, 0);
}

// --- Kiwipete (position 2): open position, all piece types have root moves,
// castling/en passant/promotion rich (the reason CPW uses it as the second
// standard perft position). ---

TEST_CASE("perft: Kiwipete, depth 1-3", "[perft]") {
	Position pos = parse_fen(kKiwipeteFen);
	check_breakdown(perft_divide_by_piece_type(pos, 1), 48, 8, 11, 11, 5, 9, 4);
	check_breakdown(perft_divide_by_piece_type(pos, 2), 2039, 344, 466, 458, 215, 384, 172);
	check_breakdown(perft_divide_by_piece_type(pos, 3), 97862, 16226, 22208, 22141, 9764, 19828, 7695);
}

TEST_CASE("perft: Kiwipete, depth 4", "[perft]") {
	Position pos = parse_fen(kKiwipeteFen);
	check_breakdown(perft_divide_by_piece_type(pos, 4), 4085603, 679368, 928058, 911172, 412745, 829606, 324654);
}

TEST_CASE("perft: Kiwipete, depth 5", "[perft][.]") {
	Position pos = parse_fen(kKiwipeteFen);
	check_breakdown(perft_divide_by_piece_type(pos, 5), 193690690, 31976200, 43739443, 43485470, 18885211, 40996690, 14607676);
}

// TEST_CASE("perft: Kiwipete, depth 6", "[perft][.]") {
// 	Position pos = parse_fen(kKiwipeteFen);
// 	// Total (8031647685) is the standard CPW reference value. ~4 minutes to
// 	// run — manual only
// 	check_breakdown(perft_divide_by_piece_type(pos, 6), 8031647685ULL,
// 	                 1324240331ULL, 1824746608ULL, 1779999021ULL,
// 	                 789758964ULL, 1703924183ULL, 608978578ULL);
// }
