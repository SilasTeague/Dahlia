#include <stdio.h>
#include "board.h"

void init_board(Board* b) {
    Piece starting_squares[64] = {
        wR, wN, wB, wQ, wK, wB, wN, wR,
        wP, wP, wP, wP, wP, wP, wP, wP,
        EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
        EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
        EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
        EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
        bP, bP, bP, bP, bP, bP, bP, bP,
        bR, bN, bB, bQ, bK, bB, bN, bR
    };

    for (int i = 0; i < 64; i++) {
        b->squares[i] = starting_squares[i];
    }
    b->side_to_move = 0; // White to move
    b->castling_rights = 15;
    b->en_passant_square = -1;
    b->fullmove_counter = 1;
    b->halfmove_clock = 0;
}

void print_board(const Board* b) {
    
    for (int row = 7; row >= 0; row--) {
        printf("%d ", row + 1);
        for (int col = 0; col < 8; col++) {
            int idx = row * 8 + col;
            Piece p = b->squares[idx];
            char c;
            switch (p) {
                case wP: c = 'P'; break;
                case wN: c = 'N'; break;
                case wB: c = 'B'; break;
                case wR: c = 'R'; break;
                case wQ: c = 'Q'; break;
                case wK: c = 'K'; break;
                case bP: c = 'p'; break;
                case bN: c = 'n'; break;
                case bB: c = 'b'; break;
                case bR: c = 'r'; break;
                case bQ: c = 'q'; break;
                case bK: c = 'k'; break;
                default: c = '.'; break;
            }
            printf("%c ", c);
        }
        printf("\n");
    }
    printf("  a b c d e f g h");
}

void clear_board(Board* b) {
    for (int i = 0; i < 64; i++) {
        b->squares[i] = EMPTY;
    }
    b->side_to_move = 0; // White to move
    b->en_passant_square = -1;
    b->castling_rights = CASTLE_W_KINGSIDE | CASTLE_W_QUEENSIDE | CASTLE_B_KINGSIDE | CASTLE_B_QUEENSIDE;
    b->fullmove_counter = 1;
    b->halfmove_clock = 0;  
}