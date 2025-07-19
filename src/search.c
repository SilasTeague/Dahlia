#include "search.h"
#include "movegen.h"
#include "board_diff.h"
#include "make_move.h"
#include "eval.h"
#include <limits.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

Move find_best_move(Board* board, int depth) {
    MoveList pl_moves;
    generate_pseudo_legal_moves(board, &pl_moves);

    MoveList legal_moves;
    generate_legal_moves(board, &pl_moves, &legal_moves);

    int best_eval = INT_MIN;
    Move best_move;

    for (int i = 0; i < legal_moves.count; i++) {
        Move move = legal_moves.moves[i];
        Board copy = *board;
        BoardDiff diff;
        make_move(&copy, move, &diff);

        int eval = minimax(&copy, depth - 1, 0);

        if (eval > best_eval) {
            best_eval = eval;
            best_move = move;
        }
    }

    return best_move;
}

int minimax(Board* board, int depth, int maximizingSide) {
    if (depth == 0) return evaluate_position(board);

    MoveList pl_moves;
    generate_pseudo_legal_moves(board, &pl_moves);

    MoveList legal_moves;
    generate_legal_moves(board, &pl_moves, &legal_moves);

    int best = (maximizingSide == 1) ? INT_MIN : INT_MAX;

    for (int i = 0; i < legal_moves.count; i++) {
        Move move = legal_moves.moves[i];
        Board copy = *board;
        BoardDiff diff;
        make_move(&copy, move, &diff);

        int score = minimax(&copy, depth - 1, 1 - maximizingSide);

        best = (maximizingSide == 1) ? MAX(best, score) : MIN(best, score);
    }

    return best;
}