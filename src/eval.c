#include "eval.h"

#define PAWN_VALUE 100
#define KNIGHT_VALUE 300
#define BISHOP_VALUE 320
#define ROOK_VALUE 500
#define QUEEN_VALUE 900
#define KING_VALUE 6400 // King should be protected at all costs

int evaluate_position(const Board* board) {
    int evaluation = 0;

    for (int i = 0; i < 64; i++) {
        Piece piece = board->squares[i];
        switch (piece) {
            case wP: evaluation += PAWN_VALUE; break;
            case bP: evaluation -= PAWN_VALUE; break;
            case wN: evaluation += KNIGHT_VALUE; break;
            case bN: evaluation -= KNIGHT_VALUE; break;
            case wB: evaluation += BISHOP_VALUE; break;
            case bB: evaluation -= BISHOP_VALUE; break;
            case wR: evaluation += ROOK_VALUE; break;
            case bR: evaluation -= ROOK_VALUE; break;
            case wQ: evaluation += QUEEN_VALUE; break;
            case bQ: evaluation -= QUEEN_VALUE; break;
            case wK: evaluation += KING_VALUE; break;
            case bK: evaluation -= KING_VALUE; break;
            default: break;
        }
    }

    return evaluation;
}