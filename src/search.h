#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"

Move find_best_move(Board* board, int depth);
int minimax(Board* board, int depth, int maximizingSide);

#endif