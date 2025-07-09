#ifndef PIECE_UTILS_H
#define PIECE_UTILS_H

#include "board.h"

int is_white(Piece piece);
int is_black(Piece piece);
int is_pawn(Piece piece);
int is_knight(Piece piece);
int is_bishop(Piece piece);
int is_rook(Piece piece);
int is_queen(Piece piece);
int is_king(Piece piece);
int is_sliding_piece(Piece piece);
int is_own_piece(int side, Piece piece);
int is_opponent_piece(int side, Piece piece);


#endif
