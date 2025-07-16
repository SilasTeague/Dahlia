#ifndef FEN_H
#define FEN_H

#include "board.h"

void fen_to_board(Board* board, const char* fen);

char* board_to_fen(const Board* board);

#endif