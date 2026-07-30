#include "perft.h"

#include "movegen/movegen.h"

uint64_t perft(Position& pos, int depth) {
	if (depth == 0) return 1;

	MoveList moves;
	generate_pseudo_legal_moves(moves, pos);

	Color us = pos.side_to_move;
	uint64_t nodes = 0;

	for (int i = 0; i < moves.count; i++) {
		StateInfo undo;
		make_move(pos, moves.moves[i], undo);
		if (!is_in_check(pos, us)) {
			nodes += perft(pos, depth - 1);
		}
		unmake_move(pos, moves.moves[i], undo);
	}

	return nodes;
}

PerftBreakdown perft_divide_by_piece_type(Position& pos, int depth) {
	PerftBreakdown result;
	if (depth == 0) {
		result.nodes = 1;
		return result;
	}

	MoveList moves;
	generate_pseudo_legal_moves(moves, pos);

	Color us = pos.side_to_move;

	for (int i = 0; i < moves.count; i++) {
		Move m = moves.moves[i];
		Piece moving = pos.board[m.from];

		StateInfo undo;
		make_move(pos, m, undo);
		if (!is_in_check(pos, us)) {
			uint64_t n = perft(pos, depth - 1);
			result.nodes += n;
			result.by_piece[moving] += n;
		}
		unmake_move(pos, m, undo);
	}

	return result;
}
