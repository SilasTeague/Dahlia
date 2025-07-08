#include "make_move.h"

void make_move(Board* board, Move move, BoardDiff* diff) {
    // Populate BoardDiff
    diff->move = move;
    diff->captured_piece = board->squares[move.to];
    diff->side_to_move = board->side_to_move;
    diff->en_passant_square = board->en_passant_square;

    Piece piece = board->squares[move.from];
    if (piece == wP || piece == bP) {
        int start_rank = (piece == wP) ? 1 : 6;
        int direction = (piece == wP) ? 8 : -8;

        if (move.from / 8 == start_rank && move.to == move.from + 2 * direction) {
            board->en_passant_square = move.from + direction;
        } else {
            board->en_passant_square = -1;
        }
    } else {
        board->en_passant_square = -1;
    }

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