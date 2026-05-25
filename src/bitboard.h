#pragma once

#include <cstdint>

// The chess board is 64 squares, so it can be represented by a 64 bit unisgned int
// We will use LSB = a1 and MSB = h8
using Bitboard = uint64_t;

class Board {
	public:

	Bitboard white_pawns;
	Bitboard black_pawns;

	Bitboard white_bishops;
	Bitboard black_bishops;

	Bitboard white_knights;
	Bitboard black_knights;

	Bitboard white_rooks;
	Bitboard black_rooks;

	Bitboard white_queens;
	Bitboard black_queens;

	Bitboard white_kings;
	Bitboard black_kings;

	Bitboard white_pieces;
	Bitboard black_pieces;
	Bitboard all_pieces;

	Board();
};

