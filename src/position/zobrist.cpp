#include "position/zobrist.h"

#include "position/position.h"

uint64_t compute_zobrist_key(const Position& pos) {
	uint64_t key = 0;

	for (int color = 0; color < 2; color++) {
		for (int piece = PAWN; piece <= KING; piece++) {
			Bitboard bb = pos.pieces[color][piece];
			while (bb) {
				Square sq = static_cast<Square>(__builtin_ctzll(bb));
				key ^= zobrist::tables.piece_square[color][piece][sq];
				bb &= bb - 1;
			}
		}
	}

	key ^= zobrist::tables.castling[pos.castling_rights];
	if (pos.en_passant_square != NULL_SQUARE) {
		key ^= zobrist::tables.en_passant_file[pos.en_passant_square % 8];
	}
	if (pos.side_to_move == BLACK) {
		key ^= zobrist::tables.side_to_move;
	}

	return key;
}
