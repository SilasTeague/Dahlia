#include "perft.h"
#include "movegen.h"
#include "board_diff.h"
#include "make_move.h"
#include "unmake_move.h"

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
