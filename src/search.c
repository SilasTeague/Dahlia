#include "search.h"
#include "movegen.h"
#include "board_diff.h"
#include "make_move.h"
#include "unmake_move.h"
#include "eval.h"
#include "zobrist.h"
#include "search.h"
#include <limits.h>
#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

void add_hash(Board* board) {
    uint64_t hash = compute_hash(board);
    board->zobrist_history[board->zobrist_count++] = hash;
}

void remove_hash(Board* board) {
    board->zobrist_count--;
}

int is_draw_by_repetition(const Board* board) {
    uint64_t hash = compute_hash(board);
    int count = 0;
    for (int i = 0; i < board->zobrist_count; i++) {
        if (board->zobrist_history[i] == hash) {
            count++;
            if (count >= 3) return 1;
        }
    }
    return 0;
}

Move find_best_move(Board* board, int depth) {
    int side = board->side_to_move;
    MoveList moves;
    generate_legal_moves(board, &moves);

    int best_eval = (side == 0) ? INT_MIN : INT_MAX;
    Move best_move = {0};

    for (int i = 0; i < moves.count; i++) {
        Move move = moves.moves[i];
        BoardDiff diff;
        make_move(board, move, &diff);
        add_hash(board);

        int eval = minimax(board, depth - 1, side);

        if ((side == 0 && eval > best_eval) || (side == 1 && eval < best_eval)) {
            best_eval = eval;
            best_move = move;
        } 
        unmake_move(board, diff);
        remove_hash(board);
    }

    return best_move;
}

int minimax(Board* board, int depth, int maximizingPlayer) {
    if (depth == 0) return evaluate_position(board);

    // Check for repetition draw
    if (is_draw_by_repetition(board)) {
        return maximizingPlayer ? -100 : 100;
    }

    //Check for 50 move rule draw
    if (board->halfmove_clock >= 100) {
        return maximizingPlayer ? -100 : 100;
    }

    MoveList moves;
    generate_legal_moves(board, &moves);

    // Check for checkmate and stalemate
    if (moves.count == 0) {
        if (is_check(board, board->side_to_move)) {
            return maximizingPlayer ? INT_MIN + 1 : INT_MAX - 1;
        } else {
            return 0;
        }
    }

    if (maximizingPlayer) {
        int max_eval = INT_MIN + 1;
        for (int i = 0; i < moves.count; i++) {
            Move move = moves.moves[i];
            BoardDiff diff;
            make_move(board, move, &diff);
            add_hash(board);

            int eval = (moves.count == 0) ? INT_MIN + 1 : minimax(board, depth - 1, 0);
            max_eval = MAX(eval, max_eval);   
            unmake_move(board, diff);
            remove_hash(board);
        }
        return max_eval;
    } else {
        int min_eval = INT_MAX - 1;
        for (int i = 0; i < moves.count; i++) {
            Move move = moves.moves[i];
            BoardDiff diff;
            make_move(board, move, &diff);
            add_hash(board);

            int eval = (moves.count == 0) ? INT_MAX - 1 : minimax(board, depth - 1, 1);
            min_eval = MIN(eval, min_eval);
            unmake_move(board, diff);
            remove_hash(board);
        }
        return min_eval;
    }
}



