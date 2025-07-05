#include "make_move.h"

void make_move(Board* board, Move move, BoardDiff* diff) {
    // Populate BoardDiff diff
    diff->move = move;
    diff->captured_piece = board->squares[move.to];
    diff->side_to_move = board->side_to_move;

    if (move.promotion != NO_PROMOTION) {
        switch (move.promotion) {
            case PROMOTE_Q: board->squares[move.to] = board->side_to_move == 0 ? wQ: bQ;
            break;
            case PROMOTE_R: board->squares[move.to] = board->side_to_move == 0 ? wR: bR;
            break;
            case PROMOTE_B: board->squares[move.to] = board->side_to_move == 0 ? wB: bB;
            break;
            case PROMOTE_N: board->squares[move.to] = board->side_to_move == 0 ? wN: bN;
            break;
        }
    } else {
        board->squares[move.to] = board->squares[move.from];
    }

    board->squares[move.from] = EMPTY;
    board->side_to_move = 1 - board->side_to_move;
}