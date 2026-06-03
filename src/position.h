#pragma once

#include "types.h"

struct Position {
	Bitboard pieces[2][6];	// [color][piece]
	Bitboard aggregates[3]; // [WHITE][BLACK[ALL] 

	Color side_to_move;
	CastlingRights castling_rights;
	Square en_passant_square;

	uint8_t halfmove_clock;
	uint16_t fullmove_count;
};
