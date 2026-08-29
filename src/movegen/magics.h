#pragma once

#include "core/types.h"

// Magic bitboards: a perfect-hash lookup that turns sliding-piece attack
// generation into a mask, a multiply, a shift and one load. The per-square
// multipliers are checked in (magics.cpp) rather than searched at startup;
// everything else about the hash is derived. See docs/movegen.md.

struct Magic {
	Bitboard mask;			 // the squares whose occupancy can change this square's attacks
	Bitboard magic;			 // multiplier that maps every subset of `mask` to a distinct index
	const Bitboard* attacks; // this square's slice of the shared attack table
	unsigned shift;			 // 64 - popcount(mask), so the product's top bits are the index

	unsigned index(Bitboard occupied) const {
		return static_cast<unsigned>(((occupied & mask) * magic) >> shift);
	}
};

extern Magic rook_magics[64];
extern Magic bishop_magics[64];

// Derives the masks and shifts and fills the shared attack table from the ray
// walker. Called by init_attack_tables(); until it has run, every magic lookup
// reads a zeroed table.
void init_magics();

// The relevant-occupancy mask for a slider on `square`: the squares its rays
// pass through, with the last square of each ray dropped. A blocker on the far
// edge is never behind anything, so its occupancy cannot change the attack set
// -- dropping it is what keeps a rook index to 12 bits instead of 14.
constexpr Bitboard rook_relevant_mask(int square) {
	const int rank = square / 8;
	const int file = square % 8;
	Bitboard mask = 0;

	for (int r = rank + 1; r <= 6; r++) mask |= 1ULL << (r * 8 + file);
	for (int r = rank - 1; r >= 1; r--) mask |= 1ULL << (r * 8 + file);
	for (int f = file + 1; f <= 6; f++) mask |= 1ULL << (rank * 8 + f);
	for (int f = file - 1; f >= 1; f--) mask |= 1ULL << (rank * 8 + f);

	return mask;
}

constexpr Bitboard bishop_relevant_mask(int square) {
	const int rank = square / 8;
	const int file = square % 8;
	Bitboard mask = 0;

	for (int r = rank + 1, f = file + 1; r <= 6 && f <= 6; r++, f++) mask |= 1ULL << (r * 8 + f);
	for (int r = rank + 1, f = file - 1; r <= 6 && f >= 1; r++, f--) mask |= 1ULL << (r * 8 + f);
	for (int r = rank - 1, f = file + 1; r >= 1 && f <= 6; r--, f++) mask |= 1ULL << (r * 8 + f);
	for (int r = rank - 1, f = file - 1; r >= 1 && f >= 1; r--, f--) mask |= 1ULL << (r * 8 + f);

	return mask;
}

// The checked-in multipliers, one per square, searched offline by
// tools/magicgen -- see docs/movegen.md#where-the-numbers-come-from.
extern const Bitboard rook_magic_numbers[64];
extern const Bitboard bishop_magic_numbers[64];
