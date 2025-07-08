#include "unmake_move.h"

void unmake_move(Board* board, const BoardDiff diff) {
    board->squares[diff.move.from] = board->squares[diff.move.to];
    board->squares[diff.move.to] = diff.captured_piece;
    board->side_to_move = diff.side_to_move;
    board->en_passant_square = diff.en_passant_square;
}