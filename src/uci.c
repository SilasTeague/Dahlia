#include <stdio.h>
#include <string.h>
#include "board.h"
#include "movegen.h"

void uci_loop() {
    char line[512];
    while (fgets(line, 512, stdin)) {
        if (strncmp(line, "uci", 3) == 0) {
            printf("id name Dahlia");
            printf("id author Silas Teague");
            printf("uciok");
        } else if (strncmp(line, "isready", 7)) {
            printf("readyok");
        }
    }
}