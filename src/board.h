#ifndef BOARD_H
#define BOARD_H

typedef enum {
    EMPTY=0,
    wP, wN, wB, wR, wQ, wK,
    bP, bN, bB, bR, bQ, bK
} Piece;

typedef struct {
    Piece squares[64];
    int side_to_move;
    int en_passant_square;
    // TODO castling rights, 50 move rule, etc.
} Board;

void init_board(Board* b);
void print_board(const Board* b);
void set_starting_position(Board* b);

#endif