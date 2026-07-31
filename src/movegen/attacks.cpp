#include "movegen/attacks.h"
#include "core/types.h"

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

static Bitboard sliding_attacks(Square square, Bitboard occupied, const int* offsets, int num_offsets) {
	Bitboard attacks = 0;
	int start_file = square % 8;

	for (int d = 0; d < num_offsets; d++) {
		int offset = offsets[d];
		int prev_file = start_file;
		int s = square;

		while (true) {
			s += offset;
			if (s < 0 || s > 63) break;

			int file = s % 8;
			// A legal single ray step changes file by at most one square;
			// anything else means the step wrapped around a board edge.
			int file_diff = file - prev_file;
			if (file_diff < 0) file_diff = -file_diff;
			if (file_diff > 1) break;
			prev_file = file;

			Bitboard bit = 1ULL << s;
			attacks |= bit;
			if (occupied & bit) break;
		}
	}

	return attacks;
}

constexpr int ROOK_OFFSETS[]   = {8, -8, 1, -1};
constexpr int BISHOP_OFFSETS[] = {9, -9, 7, -7};

Bitboard rook_attacks(Square square, Bitboard occupied) {
	return sliding_attacks(square, occupied, ROOK_OFFSETS, 4);
}

Bitboard bishop_attacks(Square square, Bitboard occupied) {
	return sliding_attacks(square, occupied, BISHOP_OFFSETS, 4);
}

Bitboard queen_attacks(Square square, Bitboard occupied) {
	return rook_attacks(square, occupied) | bishop_attacks(square, occupied);
}

bool is_attacked(const Position &pos, Square square, Color by) {
	Color defender = static_cast<Color>(by ^ 1);
	Bitboard occupied = pos.aggregates[ALL];

	if (knight_attacks[square] & pos.pieces[by][KNIGHT]) return true;
	if (king_attacks[square] & pos.pieces[by][KING]) return true;
	if (pawn_attacks[defender][square] & pos.pieces[by][PAWN]) return true;
	if (bishop_attacks(square, occupied) & (pos.pieces[by][BISHOP] | pos.pieces[by][QUEEN])) return true;
	if (rook_attacks(square, occupied) & (pos.pieces[by][ROOK] | pos.pieces[by][QUEEN])) return true;

	return false;
}
