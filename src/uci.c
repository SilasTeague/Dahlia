#include <stdio.h>
#include <string.h>
#include "board.h"
#include "movegen.h"

void uci_loop() {
    char line[512];

    Board board;

    while (fgets(line, 512, stdin)) {
        if (strncmp(line, "uci", 3) == 0) {
            printf("id name Dahlia");
            printf("id author Silas Teague");
            printf("uciok");
        } else if (strncmp(line, "isready", 7) == 0) {
            printf("readyok");
        } else if (strncmp(line, "position", 8) == 0) {
            char* pointer = line + 9;
            while (*pointer == ' ') pointer++;

            if (strncmp(pointer, "fen", 3) == 0) {
                
            } else if (strncmp(pointer, "startpos", 8) == 0) {
                init_board(&board);
            }
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
    }
}