#include "search.h"
#include "movegen.h"
#include "board_diff.h"
#include "make_move.h"
#include "eval.h"
#include <limits.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

Move find_best_move(Board* board, int depth) {
    int side = board->side_to_move;
    MoveList moves;
    generate_legal_moves(board, &moves);

    int best_eval = (side == 0) ? INT_MIN : INT_MAX;
    Move best_move = {0};

    for (int i = 0; i < moves.count; i++) {
        Move move = moves.moves[i];
        Board copy = *board;
        BoardDiff diff;
        make_move(&copy, move, &diff);

        int eval = minimax(&copy, depth - 1, side);

        if (side == 0 && eval > best_eval) {
            best_eval = eval;
            best_move = move;
        } else if (side == 1 && eval < best_eval) {
            best_eval = eval;
            best_move = move;
        }
    }

    return best_move;
}

int minimax(Board* board, int depth, int maximizingSide) {
    if (depth == 0) return evaluate_position(board);

    MoveList moves;
    generate_legal_moves(board, &moves);

    if (maximizingSide) {
        int max_eval = INT_MIN;
        for (int i = 0; i < moves.count; i++) {
            Move move = moves.moves[i];
            Board copy = *board;
            BoardDiff diff;
            make_move(&copy, move, &diff);

            int eval = minimax(board, depth - 1, 0);
            max_eval = MAX(eval, max_eval);   
        }
        return max_eval;
    } else {
        int min_eval = INT_MAX;
        for (int i = 0; i < moves.count; i++) {
            Move move = moves.moves[i];
            Board copy = *board;
            BoardDiff diff;
            make_move(&copy, move, &diff);
            int eval = minimax(board, depth - 1, 1);
            min_eval = MIN(eval, min_eval);
        }
        return min_eval;
    }
}