#include "search/ordering.h"

#include <utility>

namespace search {

namespace {

// Ordering ranks, not evaluation values: only their relative order matters, so
// a compact 1..6 scale keeps the whole MVV-LVA score inside a small range and
// well clear of the band boundaries below. KING is listed for completeness --
// it is never a victim, since a position where the king can be captured is
// never searched.
constexpr int32_t kPieceRank[6] = {1, 2, 3, 4, 5, 6};

// Score bands, ordered so that no member of a lower band can ever outrank a
// member of a higher one. Keeping them this far apart is what makes the bands
// independently tunable: changing how captures score against each other cannot
// accidentally push one above the TT move.
constexpr int32_t kTTMoveScore = 1 << 30;
constexpr int32_t kCaptureBase = 1 << 20;
constexpr int32_t kKillerBase = 1 << 18;

// History scores are unbounded in principle, so they are clamped into the band
// below the killers rather than being allowed to climb into it. A quiet move
// with a long record of cutoffs is still only a guess; a killer is evidence
// from this exact ply in the current search, which is the stronger signal.
constexpr int32_t kMaxHistoryScore = kKillerBase - 1;

// A promotion changes material as much as a capture does, so it belongs in the
// capture band whether or not it also takes a piece. Ranked by the piece
// promoted to, so queening is searched before the underpromotions that are
// almost never the point.
constexpr int32_t kPromotionBase = kCaptureBase;
constexpr int32_t kPromotionRank[5] = {0, 2, 3, 4, 5};  // indexed by Promotion

bool same_move(Move a, Move b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

}  // namespace

bool is_capture(const Position& pos, Move m) {
	if (pos.board[m.to] != NULL_PIECE) return true;
	// en_passant_square is NULL_SQUARE (255) when there is no en passant
	// target, which no real destination square can equal -- so this comparison
	// needs no separate guard.
	return pos.board[m.from] == PAWN && m.to == pos.en_passant_square;
}

int32_t mvv_lva_score(const Position& pos, Move m) {
	Piece attacker = pos.board[m.from];
	Piece victim = pos.board[m.to];
	// En passant: the victim is a pawn sitting beside the destination square,
	// not on it.
	if (victim == NULL_PIECE) victim = PAWN;

	// Victim dominates: the multiplier is larger than any attacker rank can be,
	// so every capture of a queen sorts above every capture of a rook,
	// regardless of what is doing the capturing.
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

	// Halve the whole side's table when any single entry saturates, preserving
	// the relative order that is the only thing the ordering reads. Without
	// this, a long game's early favourites pin themselves at the ceiling and
	// the table stops distinguishing anything -- the "stale saturation"
	// REFERENCE.md 3.8 warns about.
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
