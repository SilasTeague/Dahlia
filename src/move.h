#pragma once

#include "types.h"

struct MoveFlags {
	Promotion promotion = NO_PROMOTION;	
	Square en_passant_square = NULL_SQUARE;
	bool double_pawn_push;
	bool is_capture;
	Piece captured_piece;
};

struct Move {
	Square from;
	Square to;
	MoveFlags move_flags;
};
