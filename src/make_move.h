#ifndef MAKE_MOVE_H
#define MAKE_MOVE_H

#include "move.h"
#include "board.h"
#include "board_diff.h"

void make_move(Board* board, const Move move, BoardDiff* diff);

#endif