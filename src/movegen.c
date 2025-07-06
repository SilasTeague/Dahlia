#include "movegen.h"
#include "piece_utils.h"
#include <stdlib.h>

const int knight_offsets[8] = {
    -17, -15, -10, -6, 6, 10, 15, 17
};

const int BISHOP_DIRS[4] = {
    -9, -7, 7, 9
};

const int ROOK_DIRS[4] = {
    -8, -1, 1, 8
};

const int QUEEN_DIRS[8] = {
    -9, -8, -7, -1, 1, 7, 8, 9
};

void generate_pawn_moves(const Board* board, MoveList* list, int square) {
    int side = board->side_to_move;
    int dir = (side == 0) ? 8 : -8;
    int col = square % 8;
    int start_rank = (side == 0) ? 1 : 6;
    int promote_rank = (side == 0) ? 6 : 1;

    int forward = square + dir;

    if (forward >= 0 && forward < 64 && board->squares[forward] == EMPTY) {
        if ((square / 8) == promote_rank) {
            for (int promote = PROMOTE_Q; promote <= PROMOTE_N; promote++) {
                Move move = {.from = square, .to = forward, .promotion = promote};
                add_move(list, move);
            }
        } else {
            Move move = {.from = square, .to = forward, .promotion = NO_PROMOTION};
            add_move(list, move);
        }

        int double_forward = square + 2 * dir;
        if ((square / 8) == start_rank && board->squares[double_forward] == EMPTY) {
            Move move = {.from = square, .to = double_forward, .promotion = NO_PROMOTION};
            add_move(list, move);
        }
    }

    // Left capture
    if (col > 0) {
        int target = square + dir - 1;
        if (target >= 0 && target < 64) {
            Piece target_piece = board->squares[target];
            if (target_piece != EMPTY && is_opponent_piece(side, target_piece)) {
                if ((square / 8) == promote_rank) {
                    for (int promo = PROMOTE_Q; promo <= PROMOTE_N; promo++) {
                        Move move = {.from = square, .to = target, .promotion = promo};
                        add_move(list, move);
                    }
                } else {
                    Move move = {.from = square, .to = target, .promotion = NO_PROMOTION};
                    add_move(list, move);
                }
            } else if (target == board->en_passant_square) {
                Move move = {.from = square, .to = target, .promotion = NO_PROMOTION};
                add_move(list, move);
            }
        }
    }

    // Right capture
    if (col < 7) {
        int target = square + dir + 1;
        if (target >= 0 && target < 64) {
            Piece target_piece = board->squares[target];
            if (target_piece != EMPTY && is_opponent_piece(side, target_piece)) {
                if ((square / 8) == promote_rank) {
                    for (int promo = PROMOTE_Q; promo <= PROMOTE_N; promo++) {
                        Move move = {.from = square, .to = target, .promotion = promo};
                        add_move(list, move);
                    }
                } else {
                    Move move = {.from = square, .to = target, .promotion = NO_PROMOTION};
                    add_move(list, move);
                }
            } else if (target == board->en_passant_square) {
                Move move = {.from = square, .to = target, .promotion = NO_PROMOTION};
                add_move(list, move);
            }
        }
    }
}


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
            int target_square = current_square + directions[i];

            if (target_square < 0 || target_square > 63) break;

            Piece target_piece = board->squares[target_square];
            if (is_own_piece(board->side_to_move, target_piece)) break;

            Move move = {.from = square, .to = target_square, .promotion = NO_PROMOTION};
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
            case wP: case bP: generate_pawn_moves(board, list, square); break;
            case wN: case bN: generate_knight_moves(board, list, square); break;
            case wB: case bB: generate_sliding_moves(board, list, square, BISHOP_DIRS, 4); break;
            case wR: case bR: generate_sliding_moves(board, list, square, ROOK_DIRS, 4); break;
            case wQ: case bQ: generate_sliding_moves(board, list, square, QUEEN_DIRS, 8); break;
            case wK: case bK: break;
            default: break;
        }
    }
}