#include "search/search.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <utility>

#include "eval/evaluate.h"
#include "movegen/movegen.h"
#include "search/clock.h"
#include "search/ordering.h"

// Technique-by-technique rationale, constants and measurements: docs/search.md.
namespace search {

namespace {

// Far enough from kMateScore that no ordinary material/positional score reaches it.
constexpr int16_t kMateThreshold = kMateScore - kMaxPly;

bool is_mate_score(int16_t score) {
	return score >= kMateThreshold || score <= -kMateThreshold;
}

constexpr int16_t kDrawScore = 0;

// Per-think() mutable state. `stop_requested` is the caller's atomic, not an
// owned flag: a UCI `stop` on another thread has to reach it mid-search.
struct SearchState {
	std::atomic<bool>& stop_requested;
	std::chrono::steady_clock::time_point deadline;
	uint64_t nodes = 0;
	PositionHistory history;
	Move root_best_move{NULL_SQUARE, NULL_SQUARE};

	// Per-search, unlike the caller-owned per-game history table.
	KillerEntry killers[kMaxPly];
	HistoryTable& move_history;

	// Copied once per think(), so no search reads a switch that changes under it.
	SearchTuning tuning;

	SearchState(std::atomic<bool>& stop, HistoryTable& history_table)
		: stop_requested(stop), move_history(history_table) {}
};

bool time_up(const SearchState& state) {
	return std::chrono::steady_clock::now() >= state.deadline;
}

// TT scores are stored root-relative and read back node-relative.
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

// Two pawns of slack before delta pruning discards a capture.
constexpr int16_t kDeltaMargin = 200;

// Resolves pending captures at the horizon; termination is structural, not
// depth-limited, since every move here removes a piece or promotes a pawn.
int16_t quiescence(Position& pos, int16_t alpha, int16_t beta, int ply, SearchState& state) {
	state.nodes++;

	if ((state.nodes & 0x7FF) == 0 && time_up(state)) state.stop_requested.store(true, std::memory_order_relaxed);
	if (state.stop_requested.load(std::memory_order_relaxed)) return eval::evaluate(pos);

	// Stand pat: the side to move is never obliged to capture, so the static score is a lower bound.
	int16_t stand_pat = eval::evaluate(pos);
	if (ply >= kMaxPly) return stand_pat;
	if (stand_pat >= beta) return stand_pat;
	if (stand_pat > alpha) alpha = stand_pat;

	// Pseudo-legal on purpose: legality is proved by the make_move below, which
	// the recursion pays for anyway (docs/adr/0005-quiescence-pseudo-legal-movegen.md).
	MoveList moves;
	generate_pseudo_legal_moves(moves, pos);

	MoveScores scores;
	// No TT move and no killers: this node isn't stored, and killers are quiets, which are filtered out below.
	score_moves(moves, pos, Move{NULL_SQUARE, NULL_SQUARE}, KillerEntry{}, state.move_history, scores);

	const Color us = pos.side_to_move;
	int16_t best = stand_pat;
	for (int i = 0; i < moves.count; i++) {
		select_next_move(moves, scores, i);
		Move m = moves.moves[i];

		// Ordering bands are disjoint, so the first quiet move means no captures remain.
		bool is_promotion = m.promotion != NO_PROMOTION;
		if (!is_promotion && !is_capture(pos, m)) break;

		// Promotions are exempt: their gain isn't the captured piece.
		if (state.tuning.delta_pruning && !is_promotion) {
			Piece victim = pos.board[m.to] != NULL_PIECE ? pos.board[m.to] : PAWN;
			if (stand_pat + eval::piece_value(victim) + kDeltaMargin <= alpha) continue;
		}

		StateInfo undo;
		make_move(pos, m, undo);
		// One attack query after the fact, versus a second make/unmake to prove it beforehand.
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

// Aspiration and LMR constants; both sweeps are in docs/adr/0006-aspiration-lmr-constants.md.
constexpr int kAspirationInitialDelta = 25;
constexpr int kAspirationMinDepth = 4;
constexpr int kLmrMinDepth = 3;
constexpr int kLmrFullDepthMoves = 3;

// 218 is the theoretical maximum move count for a legal position.
constexpr size_t kLmrTableDepth = kMaxPly;
constexpr size_t kLmrTableMoves = 256;

// R, the depth taken off null-move pruning's verification search.
constexpr int kNullMoveReduction = 2;
// Below this the null search runs at depth <= 0, which is a bare quiescence.
constexpr int kNullMoveMinDepth = 3;

// 0.75 + ln(depth) * ln(move_index) / 2.25, built once on first use.
int lmr_reduction(int depth, int move_index) {
	static const auto table = [] {
		std::array<std::array<uint8_t, kLmrTableMoves>, kLmrTableDepth> t{};
		for (size_t d = 1; d < kLmrTableDepth; d++) {
			for (size_t m = 1; m < kLmrTableMoves; m++) {
				double r = 0.75 + std::log(static_cast<double>(d)) * std::log(static_cast<double>(m)) / 2.25;
				t[d][m] = static_cast<uint8_t>(std::max(0.0, r));
			}
		}
		return t;
	}();

	size_t d = std::min(static_cast<size_t>(depth), kLmrTableDepth - 1);
	size_t m = std::min(static_cast<size_t>(move_index), kLmrTableMoves - 1);
	return table[d][m];
}

// `allow_null` is false inside the subtree of a null move, so the search never passes twice in a row.
int16_t negamax(Position& pos, int depth, int16_t alpha, int16_t beta, int ply, SearchState& state,
                TranspositionTable& tt, bool allow_null = true) {
	state.nodes++;

	// Node-count modulus, not every node -- too slow per-node, too rare misses the time control.
	if ((state.nodes & 0x7FF) == 0 && time_up(state)) state.stop_requested.store(true, std::memory_order_relaxed);
	if (state.stop_requested.load(std::memory_order_relaxed)) return eval::evaluate(pos);

	// Before the TT probe, since the table may hold a real score for this key from
	// a line where it wasn't a repetition. The root is exempt: it must return a move.
	if (ply > 0 && is_repetition_draw(state.history, pos, ply)) return kDrawScore;

	int16_t orig_alpha = alpha;
	Move tt_move{NULL_SQUARE, NULL_SQUARE};
	TTEntry tt_entry;
	if (tt.probe(pos.zobrist_key, tt_entry)) {
		// The move is taken either way: it's ordering information, not a score claim.
		tt_move = tt_entry.best_move;
		if (state.tuning.transposition_cutoffs && ply > 0 && tt_entry.depth >= depth) {
			int16_t tt_score = score_from_tt(tt_entry.score, ply);
			if (tt_entry.bound == Bound::Exact) return tt_score;
			if (tt_entry.bound == Bound::LowerBound && tt_score > alpha) alpha = tt_score;
			else if (tt_entry.bound == Bound::UpperBound && tt_score < beta) beta = tt_score;
			if (alpha >= beta) return tt_score;
		}
	}

	if (depth == 0) return quiescence(pos, alpha, beta, ply, state);

	// Computed once and read three times below: null-move pruning, the mate/stalemate test, and LMR.
	const bool in_check = is_in_check(pos, pos.side_to_move);

	// Null-move pruning. has_non_pawn_material is the zugzwang guard -- removing
	// it misjudges the K+P benchmark position by 700cp (test_nullmove.cpp).
	if (state.tuning.null_move_pruning && allow_null && ply > 0 && depth >= kNullMoveMinDepth &&
	    beta < kMateThreshold && !in_check && has_non_pawn_material(pos, pos.side_to_move)) {
		StateInfo undo;
		make_null_move(pos, undo);
		// Pushed like a real move: the child counts entries back from the end of the line.
		state.history.push(undo.zobrist_key);
		int16_t null_score = static_cast<int16_t>(
			-negamax(pos, depth - 1 - kNullMoveReduction, static_cast<int16_t>(-beta),
			         static_cast<int16_t>(-beta + 1), ply + 1, state, tt, false));
		state.history.pop();
		unmake_null_move(pos, undo);

		if (null_score >= beta) {
			// A mate found for a side handed a free move says nothing about the real game.
			return null_score >= kMateThreshold ? beta : null_score;
		}
	}

	MoveList moves;
	generate_legal_moves(moves, pos);
	if (moves.count == 0) {
		// Mate score shrinks by one per ply toward the root, so a faster mate outscores a slower one.
		if (in_check) return static_cast<int16_t>(-kMateScore + ply);
		return 0;
	}

	// Scored once here; the loop pulls them out in descending order, so a node
	// that cuts off on move 1 never ranks the rest.
	MoveScores scores;
	const KillerEntry& killers = state.killers[ply < kMaxPly ? ply : kMaxPly - 1];
	score_moves(moves, pos, tt_move, killers, state.move_history, scores);

	// Fail-soft: `best` tracks the true best score rather than clamping to [alpha, beta].
	int16_t best = static_cast<int16_t>(-kInfiniteScore);
	Move best_move = moves.moves[0];
	// Make/unmake leaves this key untouched, so it goes on the line once, not once per move.
	state.history.push(pos.zobrist_key);
	for (int i = 0; i < moves.count; i++) {
		select_next_move(moves, scores, i);

		// Asked before the move is made: is_capture reads the square the move is about to overwrite.
		const bool is_quiet = !is_capture(pos, moves.moves[i]) && moves.moves[i].promotion == NO_PROMOTION;

		StateInfo undo;
		make_move(pos, moves.moves[i], undo);

		// PVS: full window on the first move, null window on the rest. The
		// re-search condition includes `score < beta` so that a fail-high inside
		// an enclosing scout is the answer rather than a question to ask again.
		int16_t score;
		if (i == 0) {
			score = static_cast<int16_t>(
				-negamax(pos, depth - 1, static_cast<int16_t>(-beta), static_cast<int16_t>(-alpha), ply + 1, state, tt));
		} else {
			// Captures, promotions, checking moves and evasions are all excluded:
			// none of them carry the ordering's "late means unpromising" claim.
			int reduction = 0;
			if (state.tuning.late_move_reductions && depth >= kLmrMinDepth && i >= kLmrFullDepthMoves &&
			    is_quiet && !in_check && !is_in_check(pos, pos.side_to_move)) {
				reduction = lmr_reduction(depth, i);
				// Never reduced into quiescence, which judges a quiet move by whatever tactics are pending.
				reduction = std::min(reduction, depth - 2);
			}

			// Three questions of increasing cost -- reduced scout, full-depth null
			// window, then PVS's full-window re-search -- so only the moves that
			// survive both cheap ones reach the expensive one.
			score = static_cast<int16_t>(
				-negamax(pos, depth - 1 - reduction, static_cast<int16_t>(-alpha - 1), static_cast<int16_t>(-alpha),
				         ply + 1, state, tt));
			if (reduction > 0 && score > alpha) {
				score = static_cast<int16_t>(
					-negamax(pos, depth - 1, static_cast<int16_t>(-alpha - 1), static_cast<int16_t>(-alpha), ply + 1, state, tt));
			}
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
			// Captures are excluded: MVV-LVA already ranks them above every quiet.
			if (is_quiet) {
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

// Searches one depth inside a narrow window centred on the previous iteration's
// score, widening and re-searching whenever the guess was wrong.
int16_t search_root(Position& pos, int depth, int16_t prev_score, SearchState& state,
                    TranspositionTable& tt) {
	int alpha = -kInfiniteScore;
	int beta = kInfiniteScore;
	int delta = kAspirationInitialDelta;

	// Mate scores are excluded: they jump by whole plies, so a centipawn window around one always fails.
	if (state.tuning.aspiration_windows && depth >= kAspirationMinDepth && !is_mate_score(prev_score)) {
		alpha = std::max(-static_cast<int>(kInfiniteScore), prev_score - delta);
		beta = std::min(static_cast<int>(kInfiniteScore), prev_score + delta);
	}

	while (true) {
		int16_t score = negamax(pos, depth, static_cast<int16_t>(alpha), static_cast<int16_t>(beta), 0, state, tt);

		// An aborted search returns whatever it reached; the caller discards the iteration.
		if (state.stop_requested.load(std::memory_order_relaxed)) return score;

		// Inside the window: the same score a full-window search would have returned.
		if (score > alpha && score < beta) return score;

		// Unreachable in practice, but this is the loop's termination guarantee.
		if (alpha <= -kInfiniteScore && beta >= kInfiniteScore) return score;

		// One-sided, since a fail-low says nothing new about beta, and taken from
		// the returned score rather than the old bound -- what fail-soft buys here.
		if (score <= alpha) {
			alpha = std::max(-static_cast<int>(kInfiniteScore), score - delta);
		} else {
			beta = std::min(static_cast<int>(kInfiniteScore), score + delta);
		}

		// Geometric, so a badly wrong first guess costs a handful of re-searches rather than many.
		delta += delta / 2;
	}
}

}  // namespace

SearchResult think(Position& pos, const SearchLimits& limits, TranspositionTable& tt,
                    HistoryTable& move_history, std::atomic<bool>& stop_requested,
                    const InfoCallback& on_info, const PositionHistory& history,
                    const SearchTuning& tuning) {
	tt.new_search();
	SearchState state(stop_requested, move_history);
	state.tuning = tuning;
	state.history = history;  // the search pushes/pops its own moves on top of the played game
	auto start = std::chrono::steady_clock::now();
	state.deadline = start + std::chrono::milliseconds(time_budget_ms(limits, pos.side_to_move));

	int max_depth = limits.depth > 0 ? limits.depth : kMaxPly;

	SearchResult result;
	// Guarantee some legal move even if depth 1 is aborted mid-search.
	MoveList root_moves;
	generate_legal_moves(root_moves, pos);
	if (root_moves.count > 0) result.best_move = root_moves.moves[0];
	if (root_moves.count == 0) return result;  // checkmate/stalemate: no move to make

	// Depth d-1's answer is the aspiration window's guess at depth d's.
	int16_t prev_score = 0;

	for (int depth = 1; depth <= max_depth; depth++) {
		int16_t score = search_root(pos, depth, prev_score, state, tt);
		prev_score = score;

		// Depth 1's partial result is kept (alpha-beta sets root_best_move as
		// soon as any move beats -infinity); deeper aborted iterations are discarded.
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
				int moves_to_mate = (plies_to_mate + 1) / 2;  // UCI wants moves, not plies
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
