#include "unmake_move.h"

void unmake_move(Board* board, const BoardDiff diff) {
    Move move = diff.move;
    Piece moved_piece = board->squares[move.to];

    board->side_to_move = diff.side_to_move;
    board->en_passant_square = diff.en_passant_square;
    board->castling_rights = diff.castling_rights;
    board->halfmove_clock = diff.halfmove_clock;
    board->fullmove_counter = diff.fullmove_counter;

    // Undo en passant
    if ((moved_piece == wP || moved_piece == bP) && move.to == diff.en_passant_square) {
        int capture_square = move.to + (board->side_to_move == 0 ? 8 : -8);
        board->squares[capture_square] = diff.captured_piece;
        board->squares[move.to] = EMPTY;
    } else {
        board->squares[move.to] = diff.captured_piece;
    }

    // Undo promotions
    if (move.promotion != NO_PROMOTION) {
        moved_piece = (board->side_to_move == 0) ? wP : bP;
    }

    board->squares[move.from] = moved_piece;

    // Undo castling
    if (moved_piece == wK && move.from == 4 && move.to == 6) {
        // White kingside
        board->squares[7] = wR;
        board->squares[5] = EMPTY;
    } else if (moved_piece == wK && move.from == 4 && move.to == 2) {
        // White queenside
        board->squares[0] = wR;
        board->squares[3] = EMPTY;
    } else if (moved_piece == bK && move.from == 60 && move.to == 62) {
        // Black kingside
        board->squares[63] = bR;
        board->squares[61] = EMPTY;
    } else if (moved_piece == bK && move.from == 60 && move.to == 58) {
        // Black queenside
        board->squares[56] = bR;
        board->squares[59] = EMPTY;
    }
}
