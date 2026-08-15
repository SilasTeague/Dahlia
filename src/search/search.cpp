#include "search/search.h"

#include <chrono>
#include <sstream>
#include <utility>

#include "eval/evaluate.h"
#include "movegen/movegen.h"
#include "search/clock.h"
#include "search/ordering.h"

namespace search {

namespace {

// A score counts as "mate in N" once it's within kMaxPly of kMateScore --
// far enough that no ordinary material/positional score could reach it.
constexpr int16_t kMateThreshold = kMateScore - kMaxPly;

bool is_mate_score(int16_t score) {
	return score >= kMateThreshold || score <= -kMateThreshold;
}

// A draw is worth exactly as much as an equal position: neither side gains
// by steering into one. (A contempt factor -- preferring a fight over a draw
// against weaker opposition -- is a later, separately measurable change.)
constexpr int16_t kDrawScore = 0;

// Mutable state threaded through negamax for a single think() call: node
// count, the deadline/stop flag, the line of positions reached so far, and
// the move chosen at the root. `stop_requested` is a reference to the
// caller's atomic, not an owned flag -- a UCI `stop` on another thread needs
// to reach it mid-search.
struct SearchState {
	std::atomic<bool>& stop_requested;
	std::chrono::steady_clock::time_point deadline;
	uint64_t nodes = 0;
	PositionHistory history;
	Move root_best_move{NULL_SQUARE, NULL_SQUARE};

	// Killers are per-search, not per-game: they are claims about the shape of
	// *this* tree, and the position has moved on by the next `go`. The history
	// table is the opposite -- it belongs to the caller and survives until
	// `ucinewgame` -- which is why it isn't here.
	KillerEntry killers[kMaxPly];
	HistoryTable& move_history;

	SearchState(std::atomic<bool>& stop, HistoryTable& history_table)
		: stop_requested(stop), move_history(history_table) {}
};

bool time_up(const SearchState& state) {
	return std::chrono::steady_clock::now() >= state.deadline;
}

// TT scores are stored root-relative (ply-independent) and read back
// node-relative, per REFERENCE.md 3.8's mate-distance pitfall.
int16_t score_to_tt(int16_t score, int ply) {
	if (score >= kMateThreshold) return static_cast<int16_t>(score + ply);
	if (score <= -kMateThreshold) return static_cast<int16_t>(score - ply);
	return score;
}

int16_t score_from_tt(int16_t score, int ply) {
	if (score >= kMateThreshold) return static_cast<int16_t>(score - ply);
	if (score <= -kMateThreshold) return static_cast<int16_t>(score + ply);
	return score;
}

// Anything less than this much material swing can't lift a hopeless node to
// alpha, so delta pruning doesn't bother searching the capture that would
// produce it. Two pawns: loose enough that no ordinary tactic is pruned, tight
// enough to cut the long tails of pointless captures in lost positions.
constexpr int16_t kDeltaMargin = 200;

// Quiescence search (REFERENCE.md 3.8 responsibility 3): at the horizon,
// keep searching until the position is quiet.
//
// The problem it fixes: a fixed-depth search evaluates whatever position it
// lands on, so QxP at the last ply scores as "won a pawn" even when the
// obvious recapture is waiting one ply deeper. That is the horizon effect, and
// on a material-only evaluation it is the dominant source of blunders --
// the engine is systematically fooled by every capture sequence that resolves
// just past the leaf.
//
// The fix is to stop only at positions where no capture is pending, by
// searching captures (and promotions) and nothing else.
//
// Termination: every move searched here either removes a piece from the board
// or promotes a pawn. Both are strictly monotone and bounded -- 30 capturable
// pieces and 16 pawns -- so no line can run past ~46 plies even in principle,
// and the kMaxPly guard bounds it regardless. There is no depth counter
// because there does not need to be one; this is the property the termination
// unit tests pin.
//
// Deliberately not done here: check evasions (a node in check gets the same
// stand-pat treatment as any other, which is unsound in the strict sense but
// standard for a first implementation), mate detection at the horizon, and TT
// probes. All three are separately measurable changes. Mate detection in
// particular would need the full legal move list, which is exactly the cost
// the pseudo-legal path below exists to avoid -- and negamax still detects
// every mate at depth 1 or deeper, so only a mate appearing precisely at a
// quiescence leaf is missed.
int16_t quiescence(Position& pos, int16_t alpha, int16_t beta, int ply, SearchState& state) {
	state.nodes++;

	if ((state.nodes & 0x7FF) == 0 && time_up(state)) state.stop_requested.store(true, std::memory_order_relaxed);
	if (state.stop_requested.load(std::memory_order_relaxed)) return eval::evaluate(pos);

	// Stand pat: the side to move is never *obliged* to capture, so the static
	// score of the position as it stands is a lower bound on what this node is
	// worth. Without this the search would be forced to play out every capture
	// available, including the ones that just lose material.
	int16_t stand_pat = eval::evaluate(pos);
	if (ply >= kMaxPly) return stand_pat;
	if (stand_pat >= beta) return stand_pat;
	if (stand_pat > alpha) alpha = stand_pat;

	// Pseudo-legal, not legal. generate_legal_moves() proves legality by
	// make/unmaking every pseudo-legal move, and quiescence throws away roughly
	// nine moves in ten -- so a legal move list here spends most of its cost on
	// quiet moves that are never searched. Measured on the Kiwipete benchmark,
	// that was the difference between 2.5M and 9M nodes/sec.
	//
	// The legality check doesn't disappear; it moves into the loop, where the
	// make_move needed for the recursion doubles as the proof.
	MoveList moves;
	generate_pseudo_legal_moves(moves, pos);

	MoveScores scores;
	// No TT move and no killers: this node isn't stored in the table, and
	// killers are quiet moves, which are all filtered out below anyway.
	score_moves(moves, pos, Move{NULL_SQUARE, NULL_SQUARE}, KillerEntry{}, state.move_history, scores);

	const Color us = pos.side_to_move;
	int16_t best = stand_pat;
	for (int i = 0; i < moves.count; i++) {
		select_next_move(moves, scores, i);
		Move m = moves.moves[i];

		// Every capture and promotion outranks every quiet move (search/
		// ordering.h keeps the bands disjoint, and test_ordering.cpp pins it),
		// so the first quiet move to surface means there are no captures left
		// and the rest of the list can be abandoned unscanned.
		bool is_promotion = m.promotion != NO_PROMOTION;
		if (!is_promotion && !is_capture(pos, m)) break;

		// Delta pruning: even winning the captured piece outright, plus a
		// margin for the positional value this evaluation can't see, wouldn't
		// reach alpha -- so the capture cannot change this node's score.
		// Promotions are exempt: their gain isn't the captured piece.
		if (!is_promotion) {
			Piece victim = pos.board[m.to] != NULL_PIECE ? pos.board[m.to] : PAWN;
			if (stand_pat + eval::piece_value(victim) + kDeltaMargin <= alpha) continue;
		}

		StateInfo undo;
		make_move(pos, m, undo);
		// The move was only pseudo-legal, so it may have left our own king
		// attacked. Checking after the fact costs one attack query; proving it
		// beforehand would have cost a second make/unmake.
		if (is_in_check(pos, us)) {
			unmake_move(pos, m, undo);
			continue;
		}
		int16_t score = static_cast<int16_t>(
			-quiescence(pos, static_cast<int16_t>(-beta), static_cast<int16_t>(-alpha), ply + 1, state));
		unmake_move(pos, m, undo);

		if (score > best) best = score;
		if (best > alpha) alpha = best;
		if (alpha >= beta) break;

		if (state.stop_requested.load(std::memory_order_relaxed)) break;
	}

	return best;
}

// Null-move pruning gives the opponent two moves in a row and searches the
// result shallowly. If the side to move is still winning after that gift, the
// position is so far above beta that the real move list is not worth
// generating.
//
// R, the depth taken off the verification search. 2 is the conventional
// starting value: deep enough that the null search costs a fraction of the
// real one, shallow enough that it still sees the threats that would refute
// the assumption.
constexpr int kNullMoveReduction = 2;

// Below this depth the null search saves nothing worth the risk -- at depth 2
// it would run at depth -1, i.e. a bare quiescence, which answers a different
// question than the search it is standing in for.
constexpr int kNullMoveMinDepth = 3;

// `allow_null` is false only in the subtree of a null move already made, so
// the search can never answer "what if I pass?" with "well, what if we both
// pass?" -- two passes in a row return the same position two plies shallower,
// which proves nothing and costs a subtree.
int16_t negamax(Position& pos, int depth, int16_t alpha, int16_t beta, int ply, SearchState& state,
                TranspositionTable& tt, bool allow_null = true) {
	state.nodes++;

	// Node-count modulus, not every node -- too slow per-node, too rare misses the time control.
	if ((state.nodes & 0x7FF) == 0 && time_up(state)) state.stop_requested.store(true, std::memory_order_relaxed);
	if (state.stop_requested.load(std::memory_order_relaxed)) return eval::evaluate(pos);

	// Before the TT probe: the table may well hold a real score for this key
	// from a line where it wasn't a repetition, and along *this* line the game
	// is over. The root is exempt -- it still has to return a move.
	if (ply > 0 && is_repetition_draw(state.history, pos, ply)) return kDrawScore;

	int16_t orig_alpha = alpha;
	Move tt_move{NULL_SQUARE, NULL_SQUARE};
	TTEntry tt_entry;
	if (tt.probe(pos.zobrist_key, tt_entry)) {
		tt_move = tt_entry.best_move;
		if (ply > 0 && tt_entry.depth >= depth) {
			int16_t tt_score = score_from_tt(tt_entry.score, ply);
			if (tt_entry.bound == Bound::Exact) return tt_score;
			if (tt_entry.bound == Bound::LowerBound && tt_score > alpha) alpha = tt_score;
			else if (tt_entry.bound == Bound::UpperBound && tt_score < beta) beta = tt_score;
			if (alpha >= beta) return tt_score;
		}
	}

	// The leaf is where quiescence takes over: resolve the pending captures
	// rather than scoring a position that is mid-exchange.
	if (depth == 0) return quiescence(pos, alpha, beta, ply, state);

	// Null-move pruning (REFERENCE.md 3.8 responsibility 8): hand the opponent
	// a free move and search the result shallowly. If the position is still
	// above beta after that, it is winning by enough that the real move list
	// isn't worth generating.
	//
	// `has_non_pawn_material` is the zugzwang guard, and it is the difference
	// between a sound heuristic and one that hallucinates in pawn endgames --
	// the assumption "passing is worse than any real move" is false exactly
	// where a side has nothing but pawns, which cannot move backwards. Removing
	// it makes the search misjudge the K+P benchmark position by 700
	// centipawns; test_nullmove.cpp pins that.
	//
	// The conditions are ordered cheapest-first: two integer comparisons and a
	// bitboard test reject most nodes before the in-check test, which is the
	// only one that costs an attack query. Being in check disqualifies the node
	// outright, since passing there would leave a capturable king and the whole
	// subtree would be scoring an impossible position.
	//
	// The mate-score guard keeps the heuristic away from the one place its
	// logic inverts. Near a forced mate, "even if I do nothing I'm still above
	// beta" is not evidence of a won position -- it is evidence that beta is a
	// mate score, and a shallow search that fails to find the mate would prune
	// the line that does.
	//
	// Known and accepted: this runs before the move list is generated, so a
	// *stalemate* position is pruned as though the side to move could pass --
	// which is precisely what a stalemated side would want to do. The guard
	// covers the common case (a stalemated side is usually down to a bare
	// king), and detecting the rest would mean generating the move list here,
	// which is the cost this heuristic exists to avoid.
	if (allow_null && ply > 0 && depth >= kNullMoveMinDepth && beta < kMateThreshold &&
	    has_non_pawn_material(pos, pos.side_to_move) && !is_in_check(pos, pos.side_to_move)) {
		StateInfo undo;
		make_null_move(pos, undo);
		// Pushed for the same reason a real move is: the child counts entries
		// back from the end of the line to tell the search's own moves from the
		// game's, and a null move that skipped the push would shift that count
		// by one for everything below it.
		state.history.push(undo.zobrist_key);
		int16_t null_score = static_cast<int16_t>(
			-negamax(pos, depth - 1 - kNullMoveReduction, static_cast<int16_t>(-beta),
			         static_cast<int16_t>(-beta + 1), ply + 1, state, tt, false));
		state.history.pop();
		unmake_null_move(pos, undo);

		if (null_score >= beta) {
			// Returning beta rather than null_score when the null search claims
			// a mate: it found one for a side that was handed a free move, which
			// says nothing about whether the mate exists in the real game.
			// Propagating it would put a mate score into the TT for a position
			// that isn't mating.
			return null_score >= kMateThreshold ? beta : null_score;
		}
	}

	MoveList moves;
	generate_legal_moves(moves, pos);
	if (moves.count == 0) {
		// Mate score shrinks by one per ply toward the root, so a faster mate outscores a slower one.
		if (is_in_check(pos, pos.side_to_move)) return static_cast<int16_t>(-kMateScore + ply);
		return 0;
	}

	// TT move first, then captures by MVV-LVA (search/ordering.h). Scored once
	// here; the loop below pulls them out one at a time in descending order, so
	// a node that cuts off on move 1 never ranks the rest.
	MoveScores scores;
	const KillerEntry& killers = state.killers[ply < kMaxPly ? ply : kMaxPly - 1];
	score_moves(moves, pos, tt_move, killers, state.move_history, scores);

	// Fail-soft: `best` tracks the true best score rather than clamping to [alpha, beta].
	int16_t best = static_cast<int16_t>(-kInfiniteScore);
	Move best_move = moves.moves[0];
	// This position is now an ancestor of everything the loop searches, and
	// make/unmake leaves its key untouched -- so it goes on the line once, not
	// once per move.
	state.history.push(pos.zobrist_key);
	for (int i = 0; i < moves.count; i++) {
		select_next_move(moves, scores, i);

		StateInfo undo;
		make_move(pos, moves.moves[i], undo);

		// Principal Variation Search (REFERENCE.md 3.8 responsibility 5). The
		// first move gets a full [alpha, beta] window; every move after it is
		// searched against the null window [alpha, alpha+1], which asks only
		// "is this better than what we already have?" and cannot answer with
		// anything but yes or no. That is a far cheaper question -- a window
		// one point wide fails high or low almost immediately, cutting off
		// most of the subtree -- and it is the right question, because move
		// ordering makes the first move the best one at this node 84-96% of
		// the time (measured on the four benchmark positions at depth 7). The
		// remaining few percent pay for it: a scout that fails high proves
		// only that the move beats alpha, not by how much, so the search has
		// to be repeated with the real window.
		//
		// The re-search condition is `score < beta` as well as `score >
		// alpha`, which is what stops the cost compounding: inside a node that
		// is itself being scouted, beta is already alpha+1, so a fail-high
		// there is the answer rather than a question to ask again.
		int16_t score;
		if (i == 0) {
			score = static_cast<int16_t>(
				-negamax(pos, depth - 1, static_cast<int16_t>(-beta), static_cast<int16_t>(-alpha), ply + 1, state, tt));
		} else {
			score = static_cast<int16_t>(
				-negamax(pos, depth - 1, static_cast<int16_t>(-alpha - 1), static_cast<int16_t>(-alpha), ply + 1, state, tt));
			if (score > alpha && score < beta) {
				score = static_cast<int16_t>(
					-negamax(pos, depth - 1, static_cast<int16_t>(-beta), static_cast<int16_t>(-alpha), ply + 1, state, tt));
			}
		}

		unmake_move(pos, moves.moves[i], undo);

		if (score > best) {
			best = score;
			best_move = moves.moves[i];
			if (ply == 0) state.root_best_move = moves.moves[i];
		}
		if (best > alpha) alpha = best;
		if (alpha >= beta) {
			// A quiet move good enough to cut off here is worth trying early at
			// sibling nodes (killers) and, more weakly, anywhere in the search
			// (history). Captures are excluded from both: MVV-LVA already ranks
			// them above every quiet, so recording one would only take up a
			// killer slot that a quiet move could use.
			if (!is_capture(pos, moves.moves[i]) && moves.moves[i].promotion == NO_PROMOTION) {
				state.killers[ply < kMaxPly ? ply : kMaxPly - 1].store(moves.moves[i]);
				state.move_history.update(pos.side_to_move, moves.moves[i], depth);
			}
			break;
		}

		if (state.stop_requested.load(std::memory_order_relaxed)) break;
	}
	state.history.pop();

	// Don't cache a score from a search the clock cut short.
	if (!state.stop_requested.load(std::memory_order_relaxed)) {
		Bound bound = Bound::Exact;
		if (best <= orig_alpha) bound = Bound::UpperBound;
		else if (best >= beta) bound = Bound::LowerBound;
		tt.store(pos.zobrist_key, best_move, score_to_tt(best, ply), depth, bound);
	}
	return best;
}

}  // namespace

SearchResult think(Position& pos, const SearchLimits& limits, TranspositionTable& tt,
                    HistoryTable& move_history, std::atomic<bool>& stop_requested,
                    const InfoCallback& on_info, const PositionHistory& history) {
	tt.new_search();
	SearchState state(stop_requested, move_history);
	state.history = history;  // the search pushes/pops its own moves on top of the played game
	auto start = std::chrono::steady_clock::now();
	state.deadline = start + std::chrono::milliseconds(time_budget_ms(limits, pos.side_to_move));

	int max_depth = limits.depth > 0 ? limits.depth : kMaxPly;

	SearchResult result;
	// Guarantee *some* legal move is returned even if depth 1 gets aborted
	// mid-search (e.g. an absurdly small movetime).
	MoveList root_moves;
	generate_legal_moves(root_moves, pos);
	if (root_moves.count > 0) result.best_move = root_moves.moves[0];
	if (root_moves.count == 0) return result;  // checkmate/stalemate: no move to make

	for (int depth = 1; depth <= max_depth; depth++) {
		int16_t score = negamax(pos, depth, static_cast<int16_t>(-kInfiniteScore), kInfiniteScore, 0, state, tt);

		// Depth 1 always gets its partial best move used (alpha-beta already
		// updates root_best_move as soon as any move improves on -infinity),
		// but deeper aborted iterations are discarded in favor of the last
		// fully-completed depth's result.
		if (state.stop_requested.load(std::memory_order_relaxed) && depth > 1) break;

		result.score = score;
		result.depth_reached = depth;
		result.nodes = state.nodes;
		if (state.root_best_move.from != NULL_SQUARE) result.best_move = state.root_best_move;

		if (on_info) {
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - start).count();
			uint64_t nps = elapsed_ms > 0 ? state.nodes * 1000 / static_cast<uint64_t>(elapsed_ms) : state.nodes * 1000;

			std::ostringstream line;
			line << "info depth " << depth;
			if (is_mate_score(score)) {
				int plies_to_mate = kMateScore - (score > 0 ? score : static_cast<int16_t>(-score));
				int moves_to_mate = (plies_to_mate + 1) / 2;
				line << " score mate " << (score > 0 ? moves_to_mate : -moves_to_mate);
			} else {
				line << " score cp " << score;
			}
			line << " nodes " << state.nodes << " nps " << nps << " time " << elapsed_ms;
			on_info(line.str());
		}

		if (state.stop_requested.load(std::memory_order_relaxed) || time_up(state)) break;
	}

	return result;
}

}  // namespace search
