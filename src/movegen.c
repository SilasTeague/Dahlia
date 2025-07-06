#include "movegen.h"
#include "piece_utils.h"
#include <stdlib.h>

const int knight_offsets[8] = {
    -17, -15, -10, -6, 6, 10, 15, 17
};

void generate_knight_moves(const Board* board, MoveList* list, int square) {
    int side = board->side_to_move;

    for (int i = 0; i < 8; i++) {
        int to = square + knight_offsets[i];
        if (to < 0 || to > 63) continue; // Out of bounds

        int from_rank = square / 8, from_file = square % 8;
        int to_rank = to / 8, to_file = to % 8;
        if (abs(from_rank - to_rank) > 2 || abs(from_file - to_file) > 2) continue; // Jumped from one side of the board to the other

        Piece target = board->squares[to];
        if (target == EMPTY || is_opponent_piece(side, target)) {
            Move move = {.from = square, .to = to, .promotion = NO_PROMOTION};
            add_move(list, move);
        }
    } 
}

void generate_all_moves(const Board* board, MoveList* list) {
    list->count = 0;

    for (int square = 0; square < 64; square++) {
        Piece piece = board->squares[square];
        int side = board->side_to_move;

        if (piece == EMPTY || !is_own_piece(side, piece)) continue;

        switch (piece) {
            case wP: case bP: break;
            case wN: case bN: generate_knight_moves(board, list, square); break;
            case wB: case bB: break;
            case wR: case bR: break;
            case wQ: case bQ: break;
            case wK: case bK: break;
            default: break;
        }
    }
}