#pragma once

#include <string>
#include <string_view>

#include "core/move.h"
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

	uint64_t zobrist_key;
};

// Accepts FENs that omit the halfmove/fullmove fields (defaults to 0/1),
// since not every FEN source includes them.
Position parse_fen(std::string_view fen);

// Converts a Position into FEN.
std::string to_fen(const Position& pos);

// Irreversible state needed for unmake_move.
struct StateInfo {
	Piece captured_piece = NULL_PIECE;
	CastlingRights castling_rights = NO_CASTLING;
	Square en_passant_square = NULL_SQUARE;
	uint8_t halfmove_clock = 0;
	uint64_t zobrist_key = 0;
};

// Makes a move from `pos`, applying pseudo-legal move `m` and filling `undo`
// with enough state for a matching unmake_move() to restore `pos` exactly.
void make_move(Position& pos, Move m, StateInfo& undo);

// Inverts a prior make_move(pos, m, undo), which `m` and `undo` must come from.
void unmake_move(Position& pos, Move m, const StateInfo& undo);

// Passes the turn without touching a piece, for null-move pruning. Only the
// side to move, en passant square, halfmove clock and Zobrist key change. Must
// not be called while the side to move is in check.
void make_null_move(Position& pos, StateInfo& undo);

// Inverts a prior make_null_move(pos, undo) call.
void unmake_null_move(Position& pos, const StateInfo& undo);

// True if `color`'s king is currently attacked.
bool is_in_check(const Position& pos, Color color);

// True if `color` has a knight, bishop, rook or queen. The search's null-move
// pruning uses this as its zugzwang guard: a side down to pawns and a king can
// run out of moves it is happy to make, since pawns cannot move backwards.
bool has_non_pawn_material(const Position& pos, Color color);
