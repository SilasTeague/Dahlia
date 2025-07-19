#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "movegen.h"
#include "fen.h"
#include "search.h"
#include "make_move.h"

void uci_loop() {
    char line[512];

    Board board;

    while (fgets(line, 512, stdin)) {
        if (strncmp(line, "uci", 3) == 0) {
            printf("id name Dahlia\n");
            printf("id author Silas Teague\n");
            printf("uciok\n");
        } else if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
        } else if (strncmp(line, "position", 8) == 0) {
            char* pointer = line + 9;
            while (*pointer == ' ') pointer++;

            if (strncmp(pointer, "fen", 3) == 0) {
                pointer += 3;
                while (*pointer == ' ') pointer++;

                char fen_string[92];
                int count = 0;
                int spaces = 0;

                while (*pointer && spaces < 6 && count < 91) {
                    fen_string[count] = *pointer;
                    if (*pointer == ' ') spaces++;
                    count++;
                    pointer++;
                }
                fen_string[count] = '\0';
                fen_to_board(&board, fen_string);
                print_board(&board);
            } else if (strncmp(pointer, "startpos", 8) == 0) {
                init_board(&board);
                pointer += 8;
            }

            if (strstr(pointer, "moves")) {
                pointer = strstr(pointer, "moves") + 5;
                while (*pointer == ' ') pointer++;

                while (*pointer) {
                    char move_str[6];
                    int i = 0;
                    while (*pointer && *pointer != ' ') {
                        move_str[i++] = *pointer++;
                    }
                    move_str[i] = '\0';

                    Move move = text_to_move(&board, move_str);
                    BoardDiff diff;
                    make_move(&board, move, &diff);
                    while (*pointer == ' ') pointer++;
                }
            }
        } else if (strncmp(line, "go", 2) == 0) {
            Move best_move = find_best_move(&board, 4);
            char* move_text = move_to_text(best_move);
            printf("bestmove %s\n", move_text);
        }
            else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
    }
}