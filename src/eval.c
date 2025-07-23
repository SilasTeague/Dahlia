#include "eval.h"
#include <stdlib.h>

#define PAWN_VALUE 100
#define KNIGHT_VALUE 300
#define BISHOP_VALUE 320
#define ROOK_VALUE 500
#define QUEEN_VALUE 900
#define KING_VALUE 6400 // King should be protected at all costs

const int pawnEvalWhite[64] = {
0, 0, 0, 0, 0, 0, 0, 0,
5, 10, 10, -20, -20, 10, 10, 5,
5, -5, -10, 0, 0, -10, -5, 5,
0, 0, 0, 20, 20, 0, 0, 0,
5, 5, 10, 25, 25, 10, 5, 5,
10, 10, 20, 30, 30, 20, 10, 10,
50, 40, 40, 40, 40, 40, 40, 50,
0, 0, 0, 0, 0, 0, 0, 0
};

const int pawnEvalBlack[64] = {
0, 0, 0, 0, 0, 0, 0, 0,
50, 40, 40, 40, 40, 40, 40, 50,
10, 10, 20, 30, 30, 20, 10, 10,
5, 5, 10, 25, 25, 10, 5, 5,
0, 0, 0, 20, 20, 0, 0, 0,
5, -5, -10, 0, 0, -10, -5, 5,
5, 10, 10, -20, -20, 10, 10, 5,
0, 0, 0, 0, 0, 0, 0, 0 };

const int knightEval[64] = {
-50, -40, -30, -30, -30, -30, -40, -50,
-40, -20, 0, 0, 0, 0, -20, -40,
-30, 0, 10, 15, 15, 10, 0, -30,
-30, 5, 15, 20, 20, 15, 5, -30,
-30, 0, 15, 20, 20, 15, 0, -30,
-30, 5, 10, 15, 15, 10, 5, -30,
-40, -20, 0, 5, 5, 0, -20, -40,
-50, -40, -30, -30, -30, -30, -40, -50
};

const int bishopEvalWhite[64] = {
-20, -10, -10, -10, -10, -10, -10, -20,
-10, 5, 0, 0, 0, 0, 5, -10,
-10, 10, 10, 10, 10, 10, 10, -10,
-10, 0, 10, 10, 10, 10, 0, -10,
-10, 5, 5, 10, 10, 5, 5, -10,
-10, 0, 5, 10, 10, 5, 0, -10,
-10, 0, 0, 0, 0, 0, 0, -10,
-20, -10, -10, -10, -10, -10, -10, -20
};

const int bishopEvalBlack[64] = {
-20, -10, -10, -10, -10, -10, -10, -20,
-10, 0, 0, 0, 0, 0, 0, -10,
-10, 0, 5, 10, 10, 5, 0, -10,
-10, 5, 5, 10, 10, 5, 5, -10,
-10, 0, 10, 10, 10, 10, 0, -10,
-10, 10, 10, 10, 10, 10, 10, -10,
-10, 5, 0, 0, 0, 0, 5, -10,
-20, -10, -10, -10, -10, -10, -10, -20
};

const int rookEvalWhite[64] = {
0, 0, 0, 5, 5, 0, 0, 0,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
5, 10, 10, 10, 10, 10, 10, 5,
0, 0, 0, 0, 0, 0, 0, 0
};

const int rookEvalBlack[64] = {
0, 0, 0, 0, 0, 0, 0, 0,
5, 10, 10, 10, 10, 10, 10, 5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
-5, 0, 0, 0, 0, 0, 0, -5,
0, 0, 0, 5, 5, 0, 0, 0
};

const int queenEval[64] = {
-20, -10, -10, -5, -5, -10, -10, -20,
-10, 0, 0, 0, 0, 0, 0, -10,
-10, 0, 5, 5, 5, 5, 0, -10,
-5, 0, 5, 5, 5, 5, 0, -5,
0, 0, 5, 5, 5, 5, 0, -5,
-10, 5, 5, 5, 5, 5, 0, -10,
-10, 0, 5, 0, 0, 0, 0, -10,
-20, -10, -10, -5, -5, -10, -10, -20
};

const int kingEvalWhite[64] = {
20, 30, 10, 0, 0, 10, 30, 20,
20, 20, 0, 0, 0, 0, 20, 20,
-10, -20, -20, -20, -20, -20, -20, -10,
20, -30, -30, -40, -40, -30, -30, -20,
-30, -40, -40, -50, -50, -40, -40, -30,
-30, -40, -40, -50, -50, -40, -40, -30,
-30, -40, -40, -50, -50, -40, -40, -30,
-30, -40, -40, -50, -50, -40, -40, -30 
};
const int kingEvalBlack[64] = {
-30, -40, -40, -50, -50, -40, -40, -30,
-30, -40, -40, -50, -50, -40, -40, -30,
-30, -40, -40, -50, -50, -40, -40, -30,
-30, -40, -40, -50, -50, -40, -40, -30,
-20, -30, -30, -40, -40, -30, -30, 20,
-10, -20, -20, -20, -20, -20, -20, -10,
20, 20, 0, 0, 0, 0, 20, 20,
20, 30, 10, 0, 0, 10, 30, 20
};

int evaluate_position(const Board* board) {
    int evaluation = 0;

    for (int i = 0; i < 64; i++) {
        Piece piece = board->squares[i];
        switch (piece) {
            case wP: evaluation += PAWN_VALUE + pawnEvalWhite[i]; break;
            case bP: evaluation -= PAWN_VALUE + pawnEvalBlack[i]; break;
            case wN: evaluation += KNIGHT_VALUE + knightEval[i]; break;
            case bN: evaluation -= KNIGHT_VALUE + knightEval[i]; break;
            case wB: evaluation += BISHOP_VALUE + bishopEvalWhite[i]; break;
            case bB: evaluation -= BISHOP_VALUE + bishopEvalBlack[i]; break;
            case wR: evaluation += ROOK_VALUE + rookEvalWhite[i]; break;
            case bR: evaluation -= ROOK_VALUE + rookEvalBlack[i]; break;
            case wQ: evaluation += QUEEN_VALUE + queenEval[i]; break;
            case bQ: evaluation -= QUEEN_VALUE + queenEval[i]; break;
            case wK: evaluation += KING_VALUE + kingEvalWhite[i]; break;
            case bK: evaluation -= KING_VALUE + kingEvalBlack[i]; break;
            default: break;
        }
    }

    if (rand() % 2 == 0) {
        evaluation += (rand() % 5) + 1;
    } else {
        evaluation -= (rand() % 5) + 1;
    }

    

    return evaluation;
}