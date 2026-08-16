#pragma once

#include <cstdint>

#include "core/move.h"
#include "core/types.h"
#include "movegen/movegen.h"
#include "position/position.h"

// Move ordering: nothing here changes what the search concludes, only how fast
// it gets there. See docs/search.md#move-ordering.
namespace search {

// One ordering score per move in a MoveList, parallel to MoveList::moves.
struct MoveScores {
	int32_t values[256];
};

// The two quiet moves that produced a beta cutoff at this ply in a sibling
// subtree. Two slots so a single cutoff can't evict a killer that was working
// across the whole node; captures are excluded, MVV-LVA already ranks them higher.
struct KillerEntry {
	Move first{NULL_SQUARE, NULL_SQUARE};
	Move second{NULL_SQUARE, NULL_SQUARE};

	// Demotes the previous first rather than duplicating a move across both slots.
	void store(Move m);
};

// Butterfly history: how often a quiet from-to has caused a beta cutoff anywhere
// in the search. Caller-owned and cleared on `ucinewgame`, since its value comes
// from accumulating across one game -- carried into another it is stale bias.
struct HistoryTable {
	int32_t scores[2][64][64] = {};

	void clear();

	// Depth-squared bonus, so the far more numerous shallow nodes don't drown out the deep ones.
	void update(Color side, Move m, int depth);

	int32_t get(Color side, Move m) const { return scores[side][m.from][m.to]; }
};

// True if `m` removes an enemy piece, including en passant -- where the victim
// is not on the destination square, so a bare board[m.to] check misses it.
bool is_capture(const Position& pos, Move m);

// Most Valuable Victim / Least Valuable Attacker -- a static ordering rather than
// a static evaluation, saying nothing about whether the victim is defended.
int32_t mvv_lva_score(const Position& pos, Move m);

// Ranks moves into numerically disjoint bands: TT move, then captures and
// promotions by MVV-LVA, then this ply's killers, then quiets by history. A
// `tt_move` of NULL_SQUARE simply matches nothing, as does an empty `killers`.
void score_moves(const MoveList& moves, const Position& pos, Move tt_move,
                 const KillerEntry& killers, const HistoryTable& history, MoveScores& scores);

// Swaps the highest-scoring move in [index, moves.count) into slot `index`,
// keeping `scores` in step. Selection sort rather than an up-front sort: a
// well-ordered node cuts off early and never ranks the rest.
void select_next_move(MoveList& moves, MoveScores& scores, int index);

}  // namespace search
