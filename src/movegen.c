#include "movegen.h"
#include "piece_utils.h"
#include "make_move.h"
#include "unmake_move.h"
#include <stdlib.h>
#include <stdio.h>

const int KNIGHT_OFFSETS[8] = {
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

const int KING_OFFSETS[8] = {
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
        int to = square + KNIGHT_OFFSETS[i];
        if (to < 0 || to > 63) continue; // Out of bounds

        int from_rank = square / 8, from_file = square % 8;
        int to_rank = to / 8, to_file = to % 8;
        if (abs(from_file - to_file) > 2) continue; // Jumped from one side of the board to the other

        Piece target = board->squares[to];
        if (target == EMPTY || is_opponent_piece(side, target)) {
            Move move = {.from = square, .to = to, .promotion = NO_PROMOTION};
            add_move(list, move);
        }
    } 
}

void generate_sliding_moves(const Board* board, MoveList* list, int square, const int* directions, int direction_count) {
    int from_rank = square / 8;
    int from_file = square % 8;
    for (int i = 0; i < direction_count; i++) {
        int dir = directions[i];
        int current_square = square;
        while (1) {
            int target_square = current_square + dir;
            if (target_square < 0 || target_square > 63) break;

            int to_rank = target_square / 8;
            int to_file = target_square % 8;
            if (abs(to_file - (current_square % 8)) > 1 && (dir == -1 || dir == 1 || dir == -9 || dir == 7 || dir == -7 || dir == 9)) break;

            Piece target_piece = board->squares[target_square];
            if (is_own_piece(board->side_to_move, target_piece)) break;

            Move move = {.from = square, .to = target_square, .promotion = NO_PROMOTION};
            add_move(list, move);

            if (target_piece != EMPTY) break;

            current_square = target_square;
        }
    }
}

void generate_king_moves(const Board* board, MoveList* list, int square) {
    for (int i = 0; i < 8; i++) {
        int target_square = square + KING_OFFSETS[i];
        if (target_square < 0 || target_square >= 64) continue;

        int from_file = square % 8;
        int to_file = target_square % 8;
        if (abs(to_file - from_file) > 1 || abs((square / 8) - (target_square / 8)) > 1) continue;
        
        Piece piece = board->squares[target_square];
        if (is_own_piece(board->side_to_move, piece) || square_is_attacked(board, target_square, board->side_to_move)) {
            continue;
        }
        Move move = {.from = square, .to = target_square, .promotion = NO_PROMOTION};
        add_move(list, move);
    }

    // Castling
    int side = board->side_to_move;

    if (side == 0) {
        if (board->castling_rights & CASTLE_W_KINGSIDE) {
            if (board->squares[5] == EMPTY && board->squares[6] == EMPTY) {
                if (!square_is_attacked(board, 4, 0) &&
                    !square_is_attacked(board, 5, 0) &&
                    !square_is_attacked(board, 6, 0)) {
                    Move castle = {.from = 4, .to = 6, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }

        if (board->castling_rights & CASTLE_W_QUEENSIDE) {
            if (board->squares[3] == EMPTY && board->squares[2] == EMPTY && board->squares[1] == EMPTY) {
                if (!square_is_attacked(board, 4, 0) &&
                    !square_is_attacked(board, 3, 0) &&
                    !square_is_attacked(board, 2, 0)) {
                    Move castle = {.from = 4, .to = 2, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }
    } else {
        if (board->castling_rights & CASTLE_B_KINGSIDE) {
            if (board->squares[61] == EMPTY && board->squares[62] == EMPTY) {
                if (!square_is_attacked(board, 60, 1) &&
                    !square_is_attacked(board, 61, 1) &&
                    !square_is_attacked(board, 62, 1)) {
                    Move castle = {.from = 60, .to = 62, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }

        if (board->castling_rights & CASTLE_B_QUEENSIDE) {
            if (board->squares[59] == EMPTY && board->squares[58] == EMPTY && board->squares[57] == EMPTY) {
                if (!square_is_attacked(board, 60, 1) &&
                    !square_is_attacked(board, 59, 1) &&
                    !square_is_attacked(board, 58, 1)) {
                    Move castle = {.from = 60, .to = 58, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }
    }
}

int square_is_attacked(const Board* board, int square, int side) {

    int file = square % 8;
    int pawn_dir = (side == 0) ? 8 : -8;
    int left = square + pawn_dir - 1;
    int right = square + pawn_dir + 1;
    Piece pawn = side == 0 ? bP : wP;

    if (file > 0 && left >= 0 && left < 64 && is_opponent_piece(side, board->squares[left]) && board->squares[left] == pawn) return 1;
    if (file < 7 && right >= 0 && right < 64 && is_opponent_piece(side, board->squares[right]) && board->squares[right] == pawn) return 1;

    for (int i = 0; i < 8; i++) {
        int target_square = square + KNIGHT_OFFSETS[i];
        if (target_square < 0 || target_square >= 64) continue;

        int from_file = square % 8;
        int to_file = target_square % 8;
        if (abs(to_file - from_file) > 2) continue;

        Piece piece = board->squares[target_square];
        if (is_opponent_piece(side, piece) && is_knight(piece)) return 1;
    }

    for (int i = 0; i < 4; i++) {
        for (int distance = 1; distance < 8; distance++) {
            int target_square = square + (distance * BISHOP_DIRS[i]);

            int rank_diff = (target_square / 8) - (square / 8);
            int file_diff = (target_square % 8) - (square % 8);

            if (abs(rank_diff) != abs(file_diff)) break;
            if (target_square < 0 || target_square >= 64) break;

            Piece piece = board->squares[target_square];
            if (is_own_piece(side, piece)) break;
            if (is_opponent_piece(side, piece) && ((is_bishop(piece) || is_queen(piece))
                        || (distance == 1 && is_king(piece)))) {
                return 1;
            }
            if (piece != EMPTY) break;
        } 
    }

    for (int i = 0; i < 4; i++) {
        for (int distance = 1; distance < 8; distance++) {
            int target_square = square + (distance * ROOK_DIRS[i]);
            
            if ((ROOK_DIRS[i] == 1 || ROOK_DIRS[i] == -1) && (square / 8 != target_square / 8 && square % 8 != target_square % 8)) break;
            if (target_square < 0 || target_square >= 64) break;

            Piece piece = board->squares[target_square];
            if (is_own_piece(side, piece)) break;
            if (is_opponent_piece(side, piece) && ((is_rook(piece) || is_queen(piece))
                        || (distance == 1 && is_king(piece)))) {
                return 1;
            }
            if (piece != EMPTY) break;
        } 
    }

    return 0;
}

int find_king_square(const Board* board, int side) {
    Piece king = (side == 0) ? wK : bK;
    for (int i = 0; i < 64; i++) {
        if (board->squares[i] == king)
            return i;
    }
    return -1;
}

int is_check(const Board* board, int side) {
    int king_square = find_king_square(board, side);
    return square_is_attacked(board, king_square, side);
}

void generate_pseudo_legal_moves(const Board* board, MoveList* list) {
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
            case wK: case bK: generate_king_moves(board, list, square); break;
            default: break;
        }
    }
}

void generate_legal_moves(Board* board, MoveList* pl_moves, MoveList* legal_moves) {
    legal_moves->count = 0;
    for (int i = 0; i < pl_moves->count; i++) {
        Move move = pl_moves->moves[i];
        BoardDiff diff;

        int side = board->side_to_move;
        make_move(board, move, &diff);

        if (!is_check(board, side)) {
            legal_moves->moves[legal_moves->count++] = move;
        } 

        unmake_move(board, diff);
    }
}