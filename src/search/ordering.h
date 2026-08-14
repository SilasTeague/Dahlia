#pragma once

#include <cstdint>

#include "core/move.h"
#include "core/types.h"
#include "movegen/movegen.h"
#include "position/position.h"

// Move ordering (REFERENCE.md 3.8 responsibility 4, Milestone 4). Alpha-beta's
// node count is dominated by how early the best move is tried: search the best
// move first at every node and the tree approaches its theoretical minimum;
// search it last and alpha-beta degenerates toward plain minimax. Nothing here
// changes what the search concludes -- only how fast it gets there, which is
// why every function in this header is a heuristic with no correctness
// obligation beyond "returns a permutation of the moves it was given".
namespace search {

// One ordering score per move in a MoveList, parallel to MoveList::moves.
struct MoveScores {
	int32_t values[256];
};

// The two killer moves recorded for one ply: quiet moves that produced a beta
// cutoff at this distance from the root, in a *sibling* subtree. The heuristic
// is that positions at the same ply tend to share threats -- if ...Qxh2 refuted
// one move at ply 4, it very often refutes the next one too, even though the
// two nodes are otherwise unrelated and share no transposition.
//
// Two slots rather than one: a single slot gets overwritten by every cutoff and
// so remembers only the most recent sibling, which throws away a killer that
// was working across the whole node. Two is the conventional depth -- the
// measured gain from a third is small enough that engines differ on whether to
// bother.
//
// Only quiet moves are stored. A capture that causes a cutoff is already
// ordered by MVV-LVA, so recording it here would just duplicate an entry that
// the capture band ranks better anyway.
struct KillerEntry {
	Move first{NULL_SQUARE, NULL_SQUARE};
	Move second{NULL_SQUARE, NULL_SQUARE};

	// Pushes `m` into the first slot, demoting the previous first to second. A
	// move already in the first slot is left alone rather than duplicated
	// across both slots, which would waste the second one.
	void store(Move m);
};

// Butterfly history (REFERENCE.md 3.8 responsibility 7), indexed
// [side][from][to]: how often a quiet move from-to has caused a beta cutoff
// anywhere in the search so far. Where killers are a per-ply memory, history is
// a global one -- "this move keeps being good in this game", regardless of
// depth or ply.
//
// Owned by the caller and cleared on `ucinewgame`, not per search: the whole
// value of the table is that it accumulates across the moves of a game. Carried
// into an unrelated position, though, it is stale bias, which is the pitfall
// REFERENCE.md 3.8 names.
struct HistoryTable {
	int32_t scores[2][64][64] = {};

	void clear();

	// Depth-squared bonus: a cutoff found at depth 9 is evidence from a much
	// larger subtree than one found at depth 2, and weighting them equally lets
	// the shallow nodes -- which are far more numerous -- drown out the deep
	// ones.
	void update(Color side, Move m, int depth);

	int32_t get(Color side, Move m) const { return scores[side][m.from][m.to]; }
};

// True if `m` removes an enemy piece from the board, including en passant --
// where the captured pawn is not on the destination square, so a bare
// board[m.to] check misses it.
bool is_capture(const Position& pos, Move m);

// Most Valuable Victim / Least Valuable Attacker: a capture's score rises with
// what it takes and falls with what it risks. PxQ is searched before QxP
// because if the queen is defended, PxQ still wins material outright while QxP
// loses it -- so PxQ is far likelier to be the move that causes a cutoff.
//
// This is a static ordering, not a static evaluation: it says nothing about
// whether the victim is defended. Ordering QxP ahead of a losing capture it
// can't distinguish from a winning one costs nodes, never correctness. (SEE --
// static exchange evaluation -- is the usual fix and is deliberately out of
// scope here; it is a separately measurable change.)
int32_t mvv_lva_score(const Position& pos, Move m);

// Fills `scores` with one ordering score per move in `moves`, ranking them:
// TT move, then captures and promotions by MVV-LVA, then this ply's killers,
// then the remaining quiets by history.
//
// `tt_move` is the best move a previous, possibly shallower search of this
// position found; it outranks everything, since a move that was best at depth
// d-1 is best at depth d far more often than any static heuristic can predict.
// A `tt_move` of NULL_SQUARE (no hit) simply matches nothing, as does an empty
// `killers`.
void score_moves(const MoveList& moves, const Position& pos, Move tt_move,
                 const KillerEntry& killers, const HistoryTable& history, MoveScores& scores);

// Swaps the highest-scoring move in [index, moves.count) into slot `index`,
// keeping `scores` in step.
//
// A selection sort one move at a time, rather than sorting the list up front:
// a well-ordered node takes a cutoff on the first or second move, and every
// comparison spent ranking the moves after it is wasted. The cost is O(n) per
// move taken instead of O(n log n) once, which only loses when a node searches
// nearly all of its moves -- exactly the nodes where ordering has already
// failed and the sort is not the expensive part.
void select_next_move(MoveList& moves, MoveScores& scores, int index);

}  // namespace search
