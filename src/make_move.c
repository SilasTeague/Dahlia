#include "make_move.h"
#include "piece_utils.h"

void make_move(Board* board, Move move, BoardDiff* diff) {
    // Populate BoardDiff
    diff->move = move;
    diff->captured_piece = board->squares[move.to];
    diff->side_to_move = board->side_to_move;
    diff->en_passant_square = board->en_passant_square;
    diff->castling_rights = board->castling_rights;

    Piece piece = board->squares[move.from];
    int side = board->side_to_move;

    if (piece == wP || piece == bP) {
        int start_rank = (piece == wP) ? 1 : 6;
        int direction = (piece == wP) ? 8 : -8;

        if (move.from / 8 == start_rank && move.to == move.from + 2 * direction) {
            board->en_passant_square = move.from + direction;
        } else {
            board->en_passant_square = -1;
        }

        if (move.to == diff->en_passant_square) {
            int ep_capture_square = move.to + (side == 0 ? -8 : 8);
            diff->captured_piece = board->squares[ep_capture_square];
            board->squares[ep_capture_square] = EMPTY;
        }
    } else {
        board->en_passant_square = -1;
    }

    // Castling
    if (is_king(piece)) {
        // White kingside
        if (move.from == 4 && move.to == 6) {
            board->squares[5] = wR;
            board->squares[6] = wK;
            board->squares[4] = EMPTY;
            board->squares[7] = EMPTY;
            board->side_to_move = 1 - board->side_to_move;
            return;
        }
        // Black kingside
        if (move.from == 60 && move.to == 62) {
            board->squares[61] = bR;
            board->squares[62] = bK;
            board->squares[60] = EMPTY;
            board->squares[63] = EMPTY;
            board->side_to_move = 1 - board->side_to_move;
            return;
        }
        // White queenside
        if (move.from == 4 && move.to == 2) {
            board->squares[3] = wR;
            board->squares[2] = wK;
            board->squares[0] = EMPTY;
            board->squares[4] = EMPTY;
            board->side_to_move = 1 - board->side_to_move;
            return;
        }
        // Black queenside
        if (move.from == 60 && move.to == 58) {
            board->squares[59] = bR;
            board->squares[58] = bK;
            board->squares[56] = EMPTY;
            board->squares[60] = EMPTY;
            board->side_to_move = 1 - board->side_to_move;
            return;
        }
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