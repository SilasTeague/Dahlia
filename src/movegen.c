#include "movegen.h"
#include "piece_utils.h"
#include <stdlib.h>

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

void generate_king_moves(const Board* board, MoveList* list, int square) {
    for (int i = 0; i < 8; i++) {
        int target_square = square + KING_OFFSETS[i];
        if (target_square < 0 || target_square >= 64) continue;

        int from_file = square % 8;
        int to_file = target_square % 8;
        if (abs(to_file - from_file) != 1) continue;
        
        Piece piece = board->squares[target_square];
        if (is_own_piece(board->side_to_move, piece) || square_is_attacked(board, target_square, 1 - board->side_to_move)) {
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
                if (!square_is_attacked(board, 4, 1) &&
                    !square_is_attacked(board, 5, 1) &&
                    !square_is_attacked(board, 6, 1)) {
                    Move castle = {.from = 4, .to = 6, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }

        if (board->castling_rights & CASTLE_W_QUEENSIDE) {
            if (board->squares[3] == EMPTY && board->squares[2] == EMPTY && board->squares[1] == EMPTY) {
                if (!square_is_attacked(board, 4, 1) &&
                    !square_is_attacked(board, 3, 1) &&
                    !square_is_attacked(board, 2, 1)) {
                    Move castle = {.from = 4, .to = 2, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }
    } else {
        if (board->castling_rights & CASTLE_B_KINGSIDE) {
            if (board->squares[61] == EMPTY && board->squares[62] == EMPTY) {
                if (!square_is_attacked(board, 60, 0) &&
                    !square_is_attacked(board, 61, 0) &&
                    !square_is_attacked(board, 62, 0)) {
                    Move castle = {.from = 60, .to = 62, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }

        if (board->castling_rights & CASTLE_B_QUEENSIDE) {
            if (board->squares[59] == EMPTY && board->squares[58] == EMPTY && board->squares[57] == EMPTY) {
                if (!square_is_attacked(board, 60, 0) &&
                    !square_is_attacked(board, 59, 0) &&
                    !square_is_attacked(board, 58, 0)) {
                    Move castle = {.from = 60, .to = 58, .promotion = NO_PROMOTION};
                    add_move(list, castle);
                }
            }
        }
    }
}

int square_is_attacked(const Board* board, int square, int attacking_side) {

    int file = square % 8;
    int pawn_dir = (attacking_side == 0) ? -8 : 8;
    int left = square + pawn_dir - 1;
    int right = square + pawn_dir + 1;
    Piece pawn = attacking_side == 0 ? wP : bP;

    if (file > 0 && left >= 0 && left < 64 && is_own_piece(attacking_side, board->squares[left]) && board->squares[left] == pawn) return 1;
    if (file < 7 && right >= 0 && right < 64 && is_own_piece(attacking_side, board->squares[right]) && board->squares[right] == pawn) return 1;

    for (int i = 0; i < 8; i++) {
        int target_square = square + KNIGHT_OFFSETS[i];
        if (target_square < 0 || target_square >= 64) continue;

        int from_file = square % 8;
        int to_file = target_square % 8;
        if (abs(to_file - from_file) > 2) continue;

        Piece piece = board->squares[target_square];
        if (is_own_piece(attacking_side, piece) && is_knight(piece)) return 1;
    }

    for (int i = 0; i < 4; i++) {
        for (int distance = 1; distance < 8; distance++) {
            int target_square = square + (distance * BISHOP_DIRS[i]);

            int from_file = square % 8;
            int to_file = target_square % 8;
            if (abs(to_file - from_file) != distance && BISHOP_DIRS[i] % 8 != 0) break;
            if (target_square < 0 || target_square >= 64) break;

            Piece piece = board->squares[target_square];
            if (is_opponent_piece(attacking_side, piece)) break;
            if (is_own_piece(attacking_side, piece) && ((is_bishop(piece) || is_queen(piece))
                        || (distance == 1 && is_king(piece)))) {
                return 1;
            }
            if (piece != EMPTY) break;
        } 
    }

    for (int i = 0; i < 4; i++) {
        for (int distance = 1; distance < 8; distance++) {
            int target_square = square + (distance * ROOK_DIRS[i]);
            
            int from_file = square % 8;
            int to_file = target_square % 8;
            if (abs(to_file - from_file) != distance && ROOK_DIRS[i] % 8 != 0) break;
            if (target_square < 0 || target_square >= 64) break;

            Piece piece = board->squares[target_square];
            if (is_opponent_piece(attacking_side, piece)) break;
            if (is_own_piece(attacking_side, piece) && ((is_rook(piece) || is_queen(piece))
                        || (distance == 1 && is_king(piece)))) {
                return 1;
            }
            if (piece != EMPTY) break;
        } 
    }

    return 0;
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
            case wK: case bK: generate_king_moves(board, list, square); break;
            default: break;
        }
    }
}