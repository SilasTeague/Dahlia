#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"

void add_hash(Board* board);
void remove_hash(Board* board);

Move find_best_move(Board* board, int depth);
int minimax(Board* board, int depth, int maximizingPlayer);

#endif