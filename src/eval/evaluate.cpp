#include "eval/evaluate.h"

#include <bit>

#include "core/types.h"

namespace eval {

namespace {

// Indexed by Piece (PAWN..KING); KING is never counted (it's on the board
// exactly once per side, so it would just be a constant offset).
constexpr int16_t kPieceValue[6] = {100, 320, 330, 500, 900, 0};

}  // namespace

int16_t evaluate(const Position& pos) {
	int material = 0;
	for (int piece = PAWN; piece <= QUEEN; piece++) {
		int white_count = std::popcount(pos.pieces[WHITE][piece]);
		int black_count = std::popcount(pos.pieces[BLACK][piece]);
		material += kPieceValue[piece] * (white_count - black_count);
	}
	return static_cast<int16_t>(pos.side_to_move == WHITE ? material : -material);
}

}  // namespace eval
