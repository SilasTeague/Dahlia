#include "movegen.h"
#include "piece_utils.h"
#include <stdlib.h>

const int knight_offsets[8] = {
    -17, -15, -10, -6, 6, 10, 15, 17
};

const int bishop_dirs[4] = {
    -9, -7, 7, 9
};

const int rook_dirs[4] = {
    -8, -1, 1, 8
};

const int queen_dirs[8] = {
    -9, -8, -7, -1, 1, 7, 8, 9
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

void generate_sliding_moves(const Board* board, MoveList* list, int square, const int* directions, int direction_count) {
    for (int i = 0; i < direction_count; i++) {
        int current_square = square;
        while (1) {
            int target_square = current_square + directions[1];

            if (target_square < 0 || target_square > 63) break;

            Piece target_piece = board->squares[target_square];
            if (is_own_piece(board->side_to_move, target_piece)) break;

            Move move = {.from = square, .to = target_square, .promotion = NO_PROMOTION };
            add_move(list, move);

            if (target_piece != EMPTY) break;

            current_square = target_square;
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