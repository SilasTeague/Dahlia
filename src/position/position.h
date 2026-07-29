#pragma once

#include <string>
#include <string_view>

#include "core/types.h"

struct Position {
	Bitboard pieces[2][6];	// [color][piece]
	Bitboard aggregates[3]; // [WHITE][BLACK][ALL]

	Piece board[64];

	Color side_to_move;
	CastlingRights castling_rights;
	Square en_passant_square = NULL_SQUARE;

	uint8_t halfmove_clock;
	uint16_t fullmove_count;
};

// Accepts FENs that omit the halfmove/fullmove fields (defaults to 0/1),
// since not every FEN source includes them.
Position parse_fen(std::string_view fen);

// Converts a Position into FEN.
std::string to_fen(const Position& pos);
