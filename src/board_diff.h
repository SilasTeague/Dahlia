#ifndef BOARD_STATE_H
#define BOARD_STATE_H

#include "board.h"
#include "move.h"

typedef struct {
    Move move;
    Piece captured_piece;
    int side_to_move;
    int en_passant_square;
} BoardDiff;

#endif