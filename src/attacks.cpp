#include "attacks.h"
#include "types.h"

Bitboard knight_attacks[64];
Bitboard king_attacks[64];
Bitboard pawn_attacks[2][64];

constexpr Bitboard NOT_A_FILE  = 0xFEFEFEFEFEFEFEFEULL;
constexpr Bitboard NOT_H_FILE  = 0x7F7F7F7F7F7F7F7FULL;
constexpr Bitboard NOT_AB_FILE = 0xFCFCFCFCFCFCFCFCULL;
constexpr Bitboard NOT_GH_FILE = 0x3F3F3F3F3F3F3F3FULL;

constexpr int SLIDING_DIRS[] = {-9, -8, -7, 1, 9, 8, 7, -1};
constexpr int KNIGHT_OFFSETS[] = {-10, -17, -15, -6, 10, 17, 15, 6};

void init_attack_tables() {
	for (int square = 0; square < 64; square++) {
		// Knight attacks
		Bitboard location = 1ULL << square; 

		knight_attacks[square] = 
			((location << 17) & NOT_A_FILE) |
            ((location << 15) & NOT_H_FILE) |
            ((location << 10) & NOT_AB_FILE) |
            ((location <<  6) & NOT_GH_FILE) |
            ((location >> 17) & NOT_H_FILE) |
            ((location >> 15) & NOT_A_FILE) |
            ((location >> 10) & NOT_GH_FILE) |
			((location >>  6) & NOT_AB_FILE);

		king_attacks[square] =
			((location >> 7) & NOT_A_FILE) |
			(location >> 8) |
			((location >> 9) & NOT_H_FILE) |
			((location >> 1) & NOT_H_FILE) |
			((location << 1) & NOT_A_FILE) |
			((location << 7) & NOT_H_FILE) |
			(location << 8) |
			((location << 9) & NOT_A_FILE);

		pawn_attacks[WHITE][square] =
			((location << 7) & NOT_H_FILE) |
			((location << 9) & NOT_A_FILE);
		pawn_attacks[BLACK][square] =
			((location >> 7) & NOT_A_FILE) |
			((location >> 9) & NOT_H_FILE);
	}
}

bool is_attacked(const Position &pos, Square square) {
	Bitboard location = 1ULL << square;
	Color attacking_side = static_cast<Color>(pos.side_to_move ^ 1);

	Bitboard knight_offsets = knight_attacks[square];
	if (knight_offsets & pos.pieces[attacking_side][KNIGHT]) return true;


	
}
