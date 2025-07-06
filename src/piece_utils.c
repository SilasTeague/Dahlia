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
