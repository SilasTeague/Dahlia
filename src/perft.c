#include "perft.h"
#include "movegen.h"
#include "board_diff.h"
#include "make_move.h"
#include "unmake_move.h"
#include "move.h"
#include <stdio.h>

uint64_t perft(Board* board, int depth) {
    if (depth == 0) return 1;

    MoveList pseudo, legal;
    generate_pseudo_legal_moves(board, &pseudo);
    legal.count = 0;
    generate_legal_moves(board, &pseudo, &legal);

    uint64_t nodes = 0;
    for (int i = 0; i < legal.count; i++) {
        Move move = legal.moves[i];
        BoardDiff diff;
        make_move(board, move, &diff);
        nodes += perft(board, depth - 1);
        unmake_move(board, diff);
    }

    return nodes;
}

void perft_divide(Board* board, int depth) {
    MoveList pl_moves, legal_moves;
    generate_pseudo_legal_moves(board, &pl_moves);
    generate_legal_moves(board, &pl_moves, &legal_moves);

    int total = 0;
    for (int i = 0; i < legal_moves.count; i++) {
        Move move = legal_moves.moves[i];
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
