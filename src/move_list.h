#ifndef MOVE_LIST_H
#define MOVE_LIST_H

#include "move.h"

#define MAX_MOVES 218 // Max number of legal moves in a given position: https://www.chessprogramming.org/Chess_Position#cite_note-4

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

void add_move(MoveList* list, const Move move);

#endif