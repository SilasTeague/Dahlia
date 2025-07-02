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
    // TODO castling rights, en pessant target square, 50 move rule, etc.
} Board;

void init_board(Board* b);
void print_board(const Board* b);
void set_starting_position(Board* b);

#endif