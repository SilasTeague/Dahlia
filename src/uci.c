#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "movegen.h"
#include "fen.h"
#include "search.h"
#include "make_move.h"
#include "eval.h"
#include "zobrist.h"

void uci_loop() {
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    Board board;

    while ((linelen = getline(&line, &linecap, stdin)) != -1) {
        if (strncmp(line, "uci", 3) == 0) {
            printf("id name Dahlia\n");
            fflush(stdout);
            printf("id author Silas Teague\n");
            fflush(stdout);
            printf("uciok\n");
            fflush(stdout);
        } else if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
            fflush(stdout);
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
            } else if (strncmp(pointer, "startpos", 8) == 0) {
                init_board(&board);
                pointer += 8;
            }

            if (strstr(pointer, "moves")) {
                pointer = strstr(pointer, "moves") + strlen("moves");

                while (*pointer == ' ') pointer++;

                Move move;
                BoardDiff diff;

                char *token = strtok(pointer, " ");

                while (token) {
                    move = text_to_move(&board, token);
                    make_move(&board, move, &diff);
                    add_hash(&board);
                    token = strtok(NULL, " ");
                }
            }
        } else if (strncmp(line, "go", 2) == 0) {
            Move best_move = find_best_move(&board, 5);
            int eval = evaluate_position(&board);
            printf("Evaluation: %d\n", eval);
            printf("Hash storage: %d/1024\n", board.zobrist_count + 1);
            char* move_text = move_to_text(best_move);
            printf("bestmove %s\n", move_text);
            fflush(stdout);
        }
            else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
    }
    free(line);
}