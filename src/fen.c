#include "board.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

void fen_to_board(Board* board, const char* fen) {
    init_board(board);
    clear_board(board);

    int i = 0;
    int square = 56;

    // Parse board/pieces
    while (fen[i] && fen[i] != ' ') {
        char c = fen[i];

        if (isdigit(c)) {
            square += (c - '0');
        } else if (c == '/') {
             square -= 16;
        } else {
            switch (c) {
                case 'p': board->squares[square] = bP; break;
                case 'P': board->squares[square] = wP; break;
                case 'n': board->squares[square] = bN; break;
                case 'N': board->squares[square] = wN; break;
                case 'b': board->squares[square] = bB; break;
                case 'B': board->squares[square] = wB; break;
                case 'r': board->squares[square] = bR; break;
                case 'R': board->squares[square] = wR; break;
                case 'q': board->squares[square] = bQ; break;
                case 'Q': board->squares[square] = wQ; break;
                case 'k': board->squares[square] = bK; break;
                case 'K': board->squares[square] = wK; break;
            }
            square++;
        }
        i++;
    }

    i++;

    // Parse side to move
    board->side_to_move = (fen[i] == 'w') ? 0 : 1;
    i += 2;

    // Parse castling rights
    while (fen[i] != ' ') {
        switch (fen[i]) {
            case 'K': board->castling_rights |= CASTLE_W_KINGSIDE; break;
            case 'Q': board->castling_rights |= CASTLE_W_QUEENSIDE; break;
            case 'k': board->castling_rights |= CASTLE_B_KINGSIDE; break;
            case 'q': board->castling_rights |= CASTLE_B_QUEENSIDE; break;
            case '-': break;
        }
        i++;
    }
    i++;

    // Parse en passant square
    if (fen[i] == '-') {
        board->en_passant_square = -1;
        i += 2;
    } else {
        int file = fen[i] - 'a';
        int rank = fen[i + 1] - '1';
        board->en_passant_square = rank * 8 + file;
        i += 3;
    }

    // Parse halfmove clock
    board->halfmove_clock = atoi(&fen[i]);
    while (fen[i] && fen[i] != ' ') i++;
    i++;

    // Parse fullmove counter
    board-> fullmove_counter = atoi(&fen[i]);

}

// char* board_to_fen(const Board board) {
    
// }