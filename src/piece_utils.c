#include "piece_utils.h"

int is_white(Piece piece) {
    return piece >= wP && piece <= wK;
}

int is_black(Piece piece) {
    return piece >= bP && piece <= bK;
}

int is_own_piece(int side, Piece piece) {
    return (side == 0 && is_white(piece)) || (side == 1 && is_black(piece));
}

int is_opponent_piece(int side, Piece piece) {
    return (side == 0 && is_black(piece)) || (side == 1 && is_white(piece));
}

int is_sliding_piece(Piece piece) {
    return (piece >= wB && piece <= wQ) || (piece >= bB && piece <= bQ);
}

int is_pawn(Piece piece) {
    return (piece == wP || piece == bP);
}

int is_knight(Piece piece) {
    return (piece == wN || piece == bN);
}

int is_bishop(Piece piece) {
    return (piece == wB || piece == bB);
}

int is_rook(Piece piece) {
    return (piece == wR || piece == bR);
}

int is_queen(Piece piece) {
    return (piece == wQ || piece == bQ);
}

int is_king(Piece piece) {
    return (piece == wK || piece == bK);
}
