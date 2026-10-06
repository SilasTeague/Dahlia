#include <bit>

#include <catch2/catch_test_macros.hpp>

#include "core/types.h"
#include "movegen/attacks.h"
#include "movegen/magics.h"

// Magic bitboards replace a computation with a table, so the thing to prove is
// that the table says what the computation said. The ray walker it replaced is
// still in the binary, and these tests use it as the oracle -- exhaustively, over
// every occupancy any lookup can ever be handed. See docs/movegen.md.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// Walks the subsets of `mask` in the same carry-rippler order init_magics uses.
Bitboard next_subset(Bitboard subset, Bitboard mask) { return (subset - mask) & mask; }

}  // namespace

TEST_CASE("magics: rook lookup matches the ray walker for every relevant occupancy", "[magics]") {
	long long checked = 0;

	for (int square = 0; square < 64; square++) {
		const Bitboard mask = rook_relevant_mask(square);
		Bitboard occupied = 0;
		do {
			const Bitboard expected = ray_rook_attacks(static_cast<Square>(square), occupied);
			const Bitboard actual = rook_attacks(static_cast<Square>(square), occupied);
			if (expected != actual) {
				// Reporting outside the hot loop keeps a passing run from
				// building 102,400 Catch2 assertion records.
				INFO("square " << square << " occupancy " << occupied);
				REQUIRE(expected == actual);
			}
			checked++;
			occupied = next_subset(occupied, mask);
		} while (occupied);
	}

	REQUIRE(checked == 102400);
}

TEST_CASE("magics: bishop lookup matches the ray walker for every relevant occupancy", "[magics]") {
	long long checked = 0;

	for (int square = 0; square < 64; square++) {
		const Bitboard mask = bishop_relevant_mask(square);
		Bitboard occupied = 0;
		do {
			const Bitboard expected = ray_bishop_attacks(static_cast<Square>(square), occupied);
			const Bitboard actual = bishop_attacks(static_cast<Square>(square), occupied);
			if (expected != actual) {
				INFO("square " << square << " occupancy " << occupied);
				REQUIRE(expected == actual);
			}
			checked++;
			occupied = next_subset(occupied, mask);
		} while (occupied);
	}

	REQUIRE(checked == 5248);
}

// The exhaustive tests above only ever pass a subset of the mask. Real callers
// pass a full board, whose bits outside the mask must make no difference -- that
// is the whole reason the mask exists, and getting it wrong would be invisible
// above.
TEST_CASE("magics: occupancy outside the mask cannot change the answer", "[magics]") {
	// A dense, irregular board rather than a legal position: what matters is
	// that it sets bits on the board edges, which is exactly what the mask drops.
	constexpr Bitboard kDenseBoard = 0xFFFF24000024FFFFULL;

	for (int square = 0; square < 64; square++) {
		const Square sq = static_cast<Square>(square);

		REQUIRE(rook_attacks(sq, kDenseBoard) == ray_rook_attacks(sq, kDenseBoard));
		REQUIRE(bishop_attacks(sq, kDenseBoard) == ray_bishop_attacks(sq, kDenseBoard));
		REQUIRE(queen_attacks(sq, kDenseBoard) ==
		        (ray_rook_attacks(sq, kDenseBoard) | ray_bishop_attacks(sq, kDenseBoard)));

		// An empty board and a full board are the two extremes the ray walker
		// handles by walking to the edge and by stopping immediately.
		REQUIRE(rook_attacks(sq, 0) == ray_rook_attacks(sq, 0));
		REQUIRE(bishop_attacks(sq, ~0ULL) == ray_bishop_attacks(sq, ~0ULL));
	}
}

TEST_CASE("magics: masks exclude the far edge of every ray", "[magics]") {
	constexpr Bitboard kEdges = 0xFF818181818181FFULL;

	for (int square = 0; square < 64; square++) {
		const Bitboard rook_mask = rook_relevant_mask(square);
		const Bitboard bishop_mask = bishop_relevant_mask(square);

		// A bishop never needs an edge square: every one of its rays ends there.
		REQUIRE((bishop_mask & kEdges) == 0);
		// A rook's mask keeps the rank and file it stands on, so it may touch an
		// edge -- but never a corner, which is the end of two rays at once.
		REQUIRE((rook_mask & 0x8100000000000081ULL) == 0);

		// The square itself is never in its own mask.
		REQUIRE((rook_mask & (1ULL << square)) == 0);
		REQUIRE((bishop_mask & (1ULL << square)) == 0);

		// The shift is what turns a product into an index; if it disagrees with
		// the mask by even one bit the table is either overrun or half-used.
		REQUIRE(rook_magics[square].shift == static_cast<unsigned>(64 - std::popcount(rook_mask)));
		REQUIRE(bishop_magics[square].shift == static_cast<unsigned>(64 - std::popcount(bishop_mask)));
		REQUIRE(rook_magics[square].mask == rook_mask);
		REQUIRE(bishop_magics[square].mask == bishop_mask);
	}
}
