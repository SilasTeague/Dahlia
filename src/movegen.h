#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "board.h"
#include "move_list.h"

void generate_all_moves(const Board* board, MoveList* list);

void generate_pawn_moves(const Board* board, MoveList* list, int square);
void generate_knight_moves(const Board* board, MoveList* list, int square);
void generate_king_moves(const Board* board, MoveList* list, int square);

void generate_sliding_moves(const Board* board, MoveList* list, int square, const int* directions, int direction_count);

#endif