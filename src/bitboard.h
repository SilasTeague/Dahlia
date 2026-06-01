#pragma once

#include <cstdint>
#include "types.h"

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

