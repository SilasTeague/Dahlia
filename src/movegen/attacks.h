#pragma once

#include "core/types.h"
#include "movegen/magics.h"
#include "position/position.h"

// Attack sets for every piece type. Leapers come from small precomputed tables;
// sliders come from the magic bitboards in magics.h. See docs/movegen.md.

extern Bitboard knight_attacks[64];
extern Bitboard king_attacks[64];
extern Bitboard pawn_attacks[2][64];

// Fills the leaper tables and the magic attack table. Must run once before any
// lookup below; every entry point into the engine calls it.
void init_attack_tables();

// The reference implementation: walk each ray a square at a time over live
// occupancy, stopping after the first blocker. Kept because init_magics() fills
// the magic table from it and test_magics.cpp checks every magic lookup against
// it. Not on any search path -- the lookups below are.
Bitboard ray_rook_attacks(Square square, Bitboard occupied);
Bitboard ray_bishop_attacks(Square square, Bitboard occupied);

// Inline by design: the body is a mask, a multiply, a shift and a load, which a
// call would cost more than it performs.
inline Bitboard rook_attacks(Square square, Bitboard occupied) {
	const Magic& m = rook_magics[square];
	return m.attacks[m.index(occupied)];
}

inline Bitboard bishop_attacks(Square square, Bitboard occupied) {
	const Magic& m = bishop_magics[square];
	return m.attacks[m.index(occupied)];
}

inline Bitboard queen_attacks(Square square, Bitboard occupied) {
	return rook_attacks(square, occupied) | bishop_attacks(square, occupied);
}

bool is_attacked(const Position& pos, Square square, Color by);
