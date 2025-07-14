#include <stdio.h>
#include <stdint.h>
#include "../src/perft.h"
#include <time.h>

int main() {
    clock_t start_time = clock();
    Board board;
    set_starting_position(&board);
    for (int depth = 1; depth <= 4; depth++) {
        uint64_t result = perft(&board, depth);
        printf("Perft(%d): %llu\n", depth, result);
    }
    clock_t end_time = clock();
    double time_spent = (double) (end_time - start_time) / CLOCKS_PER_SEC * 1000;
    printf("Time spent: %.3fms\n", time_spent);
}

