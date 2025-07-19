#include <stdio.h>
#include "move.h"
#include <string.h>
#include <ctype.h>

const char* square_names[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};

const char* promotion_names[5] = {
    "No Promotion", "q", "r", "b", "n"
};

void print_move(const Move move) {
    if (move.promotion != 0) {
        printf("%s%s (%s)\n", square_names[move.from], square_names[move.to], promotion_names[move.promotion]);
    } else {
        printf("%s%s\n", square_names[move.from], square_names[move.to]);
    }
    
}

char* move_to_text(const Move move) {
    static char move_text[6]; // 2 chars from, 2 chars to, 1 char promotion, 1 null terminator

    const char* from_str = square_names[move.from];
    const char* to_str = square_names[move.to];

    move_text[0] = from_str[0];
    move_text[1] = from_str[1];
    move_text[2] = to_str[0];
    move_text[3] = to_str[1];

    if (move.promotion != 0) {
        move_text[4] = promotion_names[move.promotion][0]; // get 'q', 'r', etc.
        move_text[5] = '\0';
    } else {
        move_text[4] = '\0';
    }

    return move_text;
}

Move text_to_move(const Board* board, char* text) {
    Move move;

    for (int i = 0; i < 64; i++) {
        if (strncmp(text, square_names[i], 2) == 0) {
            move.from = i;
            break;
        }
    }

    for (int i = 0; i < 64; i++) {
        if (strncmp(text + 2, square_names[i], 2) == 0) {
            move.to = i;
            break;
        }
    }

    if (strlen(text) >= 5) {
        switch (text[4]) {
            case 'q': move.promotion = 1; break;
            case 'r': move.promotion = 2; break;
            case 'b': move.promotion = 3; break;
            case 'n': move.promotion = 4; break;
            default: move.promotion = 0; break;
        }
    }

    return move;

}