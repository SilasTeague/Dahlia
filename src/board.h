#ifndef BOARD_H
#define BOARD_H

#define CASTLE_W_KINGSIDE 1
#define CASTLE_W_QUEENSIDE 2
#define CASTLE_B_KINGSIDE 4
#define CASTLE_B_QUEENSIDE 8

typedef enum {
    EMPTY=0,
    wP, wN, wB, wR, wQ, wK,
    bP, bN, bB, bR, bQ, bK
} Piece;

typedef struct {
    Piece squares[64];
    int side_to_move;
    int en_passant_square;
    int castling_rights;
    // 50 move rule, etc. 
} Board;

void init_board(Board* b);
void print_board(const Board* b);
void set_starting_position(Board* b);

#endif