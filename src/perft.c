#include "perft.h"
#include "movegen.h"
#include "board_diff.h"
#include "make_move.h"
#include "unmake_move.h"
#include "move.h"
#include <stdio.h>

uint64_t perft(Board* board, int depth) {
    if (depth == 0) return 1;

    MoveList moves;
    moves.count = 0;
    generate_legal_moves(board, &moves);

    uint64_t nodes = 0;
    for (int i = 0; i < moves.count; i++) {
        Move move = moves.moves[i];
        BoardDiff diff;
        make_move(board, move, &diff);
        nodes += perft(board, depth - 1);
        unmake_move(board, diff);
    }

    return nodes;
}

void perft_divide(Board* board, int depth) {
    MoveList moves;
    generate_legal_moves(board, &moves);

    int total = 0;
    for (int i = 0; i < moves.count; i++) {
        Move move = moves.moves[i];
        BoardDiff diff;
        make_move(board, move, &diff);
        int count = perft(board, depth - 1);
        unmake_move(board, diff);

        print_move(move);
        printf(": %d\n", count);
        total += count;
    }
    printf("Total: %d\n", total);
}
