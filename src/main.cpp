#include <cstddef>
#include <iostream>
#include <string>

#include "movegen/movegen.h"


int main() {
	Position startpos = {
		.pieces = {
			{0x000000000000FF00, 0x0000000000000042, 0x0000000000000024,0x0000000000000081,0x0000000000000008,0x0000000000000010},
			{0x00FF000000000000,0x4200000000000000,0x2400000000000000,0x8100000000000000,0x0800000000000000,0x1000000000000000}
		},
		.aggregates = {0x000000000000FFFF,0xFFFF000000000000,0xFFFF00000000FFFF},
		.side_to_move = WHITE,
		.castling_rights = ANY_CASTLING,
		.en_passant_square = NULL_SQUARE,
		.halfmove_clock = 0,
		.fullmove_count = 0,
	};

	MoveList list;

	generate_pawn_moves(list, startpos);

	std::cout << "Move count: " << list.count << '\n';		
	for (int i = 0; i < list.count; i++) {
		Move curr = list.moves[i];
	
		std::cout
			<< static_cast<int>(curr.from)
			<< " "
			<< static_cast<int>(curr.to)
			<< " ("
			<< static_cast<int>(curr.promotion)
			<< ")\n";
	}

	return 0;
}
