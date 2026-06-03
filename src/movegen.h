#pragma once

#include "types.h"
#include "position.h"
#include "move.h"

struct MoveList {
	Move moves[256]; // 218 is theoretical maximum
	int count = 0;

	void push(const Move& m) { moves[count++] = m; }
};

void generate_pawn_moves(MoveList& move_list, const Position& position);

void generate_knight_moves(MoveList& move_list, const Position& position);

void generate_sliding_moves(MoveList& move_list, const Position& position, Piece piece);

void generate_pseudo_legal_moves(MoveList& move_list, const Position& position);


