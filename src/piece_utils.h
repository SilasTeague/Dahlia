#ifndef PIECE_UTILS_H
#define PIECE_UTILS_H

#include "board.h"

int is_white(Piece piece);
int is_black(Piece piece);
int is_own_piece(int side, Piece piece);
int is_opponent_piece(int side, Piece piece);

#endif
