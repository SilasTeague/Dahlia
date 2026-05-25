#include "bitboard.h"

Board::Board() {
	white_pawns = 0x000000000000FF00ULL;
	black_pawns = 0x00FF000000000000ULL;

	white_bishops = 0x0000000000000024ULL;
	black_bishops = 0x2400000000000000ULL;

	white_knights = 0x0000000000000042ULL;
	black_knights = 0x4200000000000000ULL;


	white_rooks = 0x0000000000000081ULL;
	black_rooks = 0x8100000000000000ULL;

	white_queens = 0x0000000000000010ULL;
	black_queens = 0x1000000000000000ULL;
	
	white_kings = 0x0000000000000008ULL;
	black_kings = 0x8000000000000000ULL;

	white_pieces = white_pawns | white_bishops | white_knights 
		| white_rooks | white_kings | white_queens;
	black_pieces = black_pawns | black_bishops | black_knights 
		| black_rooks | black_kings | black_queens;
	all_pieces = white_pieces | black_pieces;
}
