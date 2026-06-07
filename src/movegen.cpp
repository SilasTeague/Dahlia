#include "movegen.h"
#include "types.h"

void generate_pawn_moves(MoveList &move_list, const Position &position) {
	Color side = position.side_to_move;
	const Bitboard pawn_bb = position.pieces[side][PAWN];

	// Generate attacks:
	static constexpr Bitboard NOT_A_FILE = 0xFEFEFEFEFEFEFEFEULL;
	static constexpr Bitboard NOT_H_FILE = 0x7F7F7F7F7F7F7F7FULL;

	static constexpr Bitboard RANK_8 = 0xFF00000000000000ULL;
	static constexpr Bitboard RANK_1 = 0x00000000000000FFULL;

	Bitboard left_possible_attacks = 
		(side == WHITE) ?	
		(pawn_bb & NOT_A_FILE) << 7 : (pawn_bb & NOT_A_FILE) >> 9;
	Bitboard right_possible_attacks = 
		(side == WHITE) ?
		(pawn_bb & NOT_H_FILE) << 9 : (pawn_bb & NOT_H_FILE) >> 7;

	Bitboard ep_bb = (position.en_passant_square == NULL_SQUARE) ?
		0 : (1ULL << position.en_passant_square);
	Bitboard targets = position.aggregates[side ^ 1] | ep_bb;

	Bitboard left_attacks = left_possible_attacks & targets;
	Bitboard right_attacks = right_possible_attacks & targets;

	// Promotion attacks
	Bitboard left_promotion_attacks = (side == WHITE) ?
		left_attacks & RANK_8 :
		left_attacks & RANK_1;

	Bitboard right_promotion_attacks = (side == WHITE) ?
		right_attacks & RANK_8 :
		right_attacks & RANK_1;

	left_attacks &= (side == WHITE) ? ~RANK_8 : ~RANK_1;
	right_attacks &= (side == WHITE) ? ~RANK_8 : ~RANK_1;

	// Generate push moves:
	static constexpr Bitboard RANK_5 = 0x000000ff00000000ULL;
	static constexpr Bitboard RANK_4 = 0x00000000ff000000ULL;

	const Bitboard all_bb = position.aggregates[ALL];
	// Single push
	Bitboard single_push = (side == WHITE) ?
		((pawn_bb << 8) & ~all_bb) : ((pawn_bb >> 8) & ~all_bb);
	// Promotion
	Bitboard promotion_push = (side == WHITE) ?
		single_push & RANK_8:
		single_push & RANK_1;

	single_push &= (side == WHITE) ? ~RANK_8 : ~RANK_1;
	// Double push
	Bitboard double_push = (side == WHITE) ?
		(single_push << 8 & ~all_bb & RANK_4) :
		(single_push >> 8 & ~all_bb & RANK_5);
	
	// Add moves to movelist
	while (left_attacks) {
		Square to = (Square) __builtin_ctzll(left_attacks);
		Square from = (side == WHITE) ? (Square) (to - 7) 
			: (Square) (to + 9);
		Move move = {from, to};
		move_list.push(move);

		left_attacks &= left_attacks - 1;
	}

	while (left_promotion_attacks) {
		Square to = (Square) __builtin_ctzll(left_promotion_attacks);
		Square from = (side == WHITE) ? (Square) (to - 7) 
			: (Square) (to + 9);
		Move move = {from, to, PROMOTION_BISHOP};
		move_list.push(move);
		move = {from, to, PROMOTION_KNIGHT};
		move_list.push(move);
		move = {from, to, PROMOTION_ROOK};
		move_list.push(move);
		move = {from, to, PROMOTION_QUEEN};
		move_list.push(move);

		left_promotion_attacks &= left_promotion_attacks - 1;
	}

	while (right_attacks) {
		Square to = (Square) __builtin_ctzll(right_attacks);
		Square from = (side == WHITE) ? (Square) (to - 9) 
			: (Square) (to + 7);
		Move move = {from, to};
		move_list.push(move);

		right_attacks &= right_attacks - 1;
	}

	while (right_promotion_attacks) {
		Square to = (Square) __builtin_ctzll(right_promotion_attacks);
		Square from = (side == WHITE) ? (Square) (to - 9) 
			: (Square) (to + 7);
		Move move = {from, to, PROMOTION_BISHOP};
		move_list.push(move);
		move = {from, to, PROMOTION_KNIGHT};
		move_list.push(move);
		move = {from, to, PROMOTION_ROOK};
		move_list.push(move);
		move = {from, to, PROMOTION_QUEEN};
		move_list.push(move);

		right_promotion_attacks &= right_promotion_attacks - 1;
	}

	while (single_push) {
		Square to = (Square) __builtin_ctzll(single_push);
		Square from = (side == WHITE) ? (Square) (to - 8)
			: (Square) (to + 8);
		Move move = {from, to};
		move_list.push(move);

		single_push &= single_push - 1;
	}

	while (promotion_push) {
		Square to = (Square) __builtin_ctzll(promotion_push);
		Square from = (side == WHITE) ? (Square) (to - 8)
			: (Square) (to + 8);
		Move move = {from, to, PROMOTION_BISHOP};
		move_list.push(move);
		move = {from, to, PROMOTION_KNIGHT};
		move_list.push(move);
		move = {from, to, PROMOTION_ROOK};
		move_list.push(move);
		move = {from, to, PROMOTION_QUEEN};
		move_list.push(move);

		promotion_push &= promotion_push - 1;
	}

	while (double_push) {
		Square to = (Square) __builtin_ctzll(double_push);
		Square from = (side == WHITE) ? (Square) (to - 16)
			: (Square) (to + 16);
		Move move = {from, to};
		move_list.push(move);

		double_push &= double_push - 1;
	}

}
