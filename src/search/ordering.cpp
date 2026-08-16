#include "search/ordering.h"

#include <utility>

namespace search {

namespace {

// Ranks, not values: only the relative order matters, and KING is never a victim.
constexpr int32_t kPieceRank[6] = {1, 2, 3, 4, 5, 6};

// Bands kept far enough apart that no member of a lower one can outrank a higher
// one, which is what makes each independently tunable.
constexpr int32_t kTTMoveScore = 1 << 30;
constexpr int32_t kCaptureBase = 1 << 20;
constexpr int32_t kKillerBase = 1 << 18;

// History is unbounded in principle, so it is clamped below the killer band.
constexpr int32_t kMaxHistoryScore = kKillerBase - 1;

// Promotions change material like captures do, so they share that band, ranked
// by the piece promoted to.
constexpr int32_t kPromotionBase = kCaptureBase;
constexpr int32_t kPromotionRank[5] = {0, 2, 3, 4, 5};  // indexed by Promotion

bool same_move(Move a, Move b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

}  // namespace

bool is_capture(const Position& pos, Move m) {
	if (pos.board[m.to] != NULL_PIECE) return true;
	// NULL_SQUARE (255) can't equal a real destination, so this needs no guard.
	return pos.board[m.from] == PAWN && m.to == pos.en_passant_square;
}

int32_t mvv_lva_score(const Position& pos, Move m) {
	Piece attacker = pos.board[m.from];
	Piece victim = pos.board[m.to];
	// En passant: the victim sits beside the destination square, not on it.
	if (victim == NULL_PIECE) victim = PAWN;

	// The multiplier exceeds any attacker rank, so the victim always dominates.
	return kPieceRank[victim] * 8 - kPieceRank[attacker];
}

void KillerEntry::store(Move m) {
	if (same_move(m, first)) return;
	second = first;
	first = m;
}

void HistoryTable::clear() {
	for (int side = 0; side < 2; side++) {
		for (int from = 0; from < 64; from++) {
			for (int to = 0; to < 64; to++) scores[side][from][to] = 0;
		}
	}
}

void HistoryTable::update(Color side, Move m, int depth) {
	int32_t& entry = scores[side][m.from][m.to];
	entry += depth * depth;

	// Halve rather than clamp on saturation, preserving the relative order that
	// is the only thing the ordering reads.
	if (entry >= kMaxHistoryScore) {
		for (int from = 0; from < 64; from++) {
			for (int to = 0; to < 64; to++) scores[side][from][to] /= 2;
		}
	}
}

void score_moves(const MoveList& moves, const Position& pos, Move tt_move,
                 const KillerEntry& killers, const HistoryTable& history, MoveScores& scores) {
	for (int i = 0; i < moves.count; i++) {
		Move m = moves.moves[i];

		if (tt_move.from != NULL_SQUARE && same_move(m, tt_move)) {
			scores.values[i] = kTTMoveScore;
			continue;
		}

		if (m.promotion != NO_PROMOTION) {
			scores.values[i] = kPromotionBase + kPromotionRank[m.promotion] * 8;
			continue;
		}

		if (is_capture(pos, m)) {
			scores.values[i] = kCaptureBase + mvv_lva_score(pos, m);
			continue;
		}

		if (same_move(m, killers.first)) {
			scores.values[i] = kKillerBase + 1;
			continue;
		}
		if (same_move(m, killers.second)) {
			scores.values[i] = kKillerBase;
			continue;
		}

		int32_t score = history.get(pos.side_to_move, m);
		scores.values[i] = score < kMaxHistoryScore ? score : kMaxHistoryScore;
	}
}

void select_next_move(MoveList& moves, MoveScores& scores, int index) {
	int best = index;
	for (int i = index + 1; i < moves.count; i++) {
		if (scores.values[i] > scores.values[best]) best = i;
	}
	if (best != index) {
		std::swap(moves.moves[index], moves.moves[best]);
		std::swap(scores.values[index], scores.values[best]);
	}
}

}  // namespace search
