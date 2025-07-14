#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "board.h"
#include "move_list.h"

void generate_psuedo_legal_moves(const Board* board, MoveList* list);

void generate_legal_moves(Board* board, MoveList* pl_moves, MoveList* legal_moves);

void generate_pawn_moves(const Board* board, MoveList* list, int square);
void generate_knight_moves(const Board* board, MoveList* list, int square);
void generate_king_moves(const Board* board, MoveList* list, int square);
int square_is_attacked(const Board* board, int square, int attacking_side);
int find_king_square(const Board* board, int side);
int is_check(const Board* board, int side);

void generate_sliding_moves(const Board* board, MoveList* list, int square, const int* directions, int direction_count);

#endif