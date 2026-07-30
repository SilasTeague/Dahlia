#pragma once

#include <cstdint>

#include "core/types.h"

struct Position;

// Zobrist tables, generated at compile time from a fixed seed
// (splitmix64) so they're reproducible across builds/platforms without
// checking in a literal constant table by hand.
namespace zobrist {

struct Tables {
	uint64_t piece_square[2][6][64];  // [color][piece][square]
	uint64_t castling[16];            // indexed directly by CastlingRights bitmask
	uint64_t en_passant_file[8];
	uint64_t side_to_move;
};

namespace detail {

constexpr uint64_t splitmix64_next(uint64_t& state) {
	state += 0x9E3779B97F4A7C15ULL;
	uint64_t z = state;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

constexpr Tables generate_tables() {
	Tables t{};
	uint64_t state = 0x2545F4914F6CDD1DULL;  // arbitrary fixed seed

	for (int color = 0; color < 2; color++) {
		for (int piece = 0; piece < 6; piece++) {
			for (int square = 0; square < 64; square++) {
				t.piece_square[color][piece][square] = splitmix64_next(state);
			}
		}
	}
	for (auto& c : t.castling) c = splitmix64_next(state);
	for (auto& f : t.en_passant_file) f = splitmix64_next(state);
	t.side_to_move = splitmix64_next(state);

	return t;
}

}  // namespace detail

inline constexpr Tables tables = detail::generate_tables();

}  // namespace zobrist

uint64_t compute_zobrist_key(const Position& pos);
