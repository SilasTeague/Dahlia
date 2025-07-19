#include <stdio.h>
#include <stdint.h>
#include "../src/perft.h"
#include <time.h>

int main() {
    Board board;
    init_board(&board);
    
    board.squares[12] = wK;
    board.squares[33] = bB;
    board.side_to_move = 0;
    
    int result = perft(&board, 1);
    printf("\n%d\n", result);

    if (result == 6) {
        printf("PASS");
    } else {
        printf("FAIL");
    }

    board.squares[36] = wR;

    result = perft(&board, 1);
    printf("\n%d\n", result);

    if (result == 7) {
        printf("PASS");
    } else {
        printf("FAIL");
    }

    board.squares[10] = wP;

    result = perft(&board, 1);
    printf("\n%d\n", result);

    if (result == 8) {
        printf("PASS");
    } else {
        printf("FAIL");
    }

    return 1;
}