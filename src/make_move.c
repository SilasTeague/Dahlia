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
    // Remove castling rights when a king or rook moves
    if (piece == wK) board->castling_rights &= ~(CASTLE_W_KINGSIDE | CASTLE_W_QUEENSIDE);
    if (piece == bK) board->castling_rights &= ~(CASTLE_B_KINGSIDE | CASTLE_B_QUEENSIDE);

    if (piece == wR && move.from == 0) board->castling_rights &= ~CASTLE_W_QUEENSIDE;
    if (piece == wR && move.from == 7) board->castling_rights &= ~CASTLE_W_KINGSIDE;
    if (piece == bR && move.from == 56) board->castling_rights &= ~CASTLE_B_QUEENSIDE;
    if (piece == bR && move.from == 63) board->castling_rights &= ~CASTLE_B_KINGSIDE;

    if (diff->captured_piece == wR && move.to == 0) board->castling_rights &= ~CASTLE_W_QUEENSIDE;
    if (diff->captured_piece == wR && move.to == 7) board->castling_rights &= ~CASTLE_W_KINGSIDE;
    if (diff->captured_piece == bR && move.to == 56) board->castling_rights &= ~CASTLE_B_QUEENSIDE;
    if (diff->captured_piece == bR && move.to == 63) board->castling_rights &= ~CASTLE_B_KINGSIDE;

    // Castling movement
    if (piece == wK && move.from == 4 && move.to == 6) { // White kingside
        board->squares[5] = wR;
        board->squares[7] = EMPTY;
    } else if (piece == wK && move.from == 4 && move.to == 2) { // White queenside
        board->squares[3] = wR;
        board->squares[0] = EMPTY;
    } else if (piece == bK && move.from == 60 && move.to == 62) { // Black kingside
        board->squares[61] = bR;
        board->squares[63] = EMPTY;
    } else if (piece == bK && move.from == 60 && move.to == 58) { // Black queenside
        board->squares[59] = bR;
        board->squares[56] = EMPTY;
    }

    if (move.promotion != NO_PROMOTION) {
        switch (move.promotion) {
            case PROMOTE_Q: board->squares[move.to] = (board->side_to_move == 0 ? wQ: bQ); break;
            case PROMOTE_R: board->squares[move.to] = (board->side_to_move == 0 ? wR: bR); break;
            case PROMOTE_B: board->squares[move.to] = (board->side_to_move == 0 ? wB: bB); break;
            case PROMOTE_N: board->squares[move.to] = (board->side_to_move == 0 ? wN: bN); break;
        }
    } else {
        board->squares[move.to] = board->squares[move.from];
    }

    board->squares[move.from] = EMPTY;
    board->side_to_move = 1 - board->side_to_move;
}