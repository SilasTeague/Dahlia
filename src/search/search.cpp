#include "search/search.h"

#include <chrono>
#include <sstream>
#include <utility>

#include "eval/evaluate.h"
#include "movegen/movegen.h"
#include "search/clock.h"

namespace search {

namespace {

// A score counts as "mate in N" once it's within kMaxPly of kMateScore --
// far enough that no ordinary material/positional score could reach it.
constexpr int16_t kMateThreshold = kMateScore - kMaxPly;

bool is_mate_score(int16_t score) {
	return score >= kMateThreshold || score <= -kMateThreshold;
}

// Mutable state threaded through negamax for a single think() call: node
// count, the deadline/stop flag, and the move chosen at the root.
// `stop_requested` is a reference to the caller's atomic, not an owned flag
// -- a UCI `stop` on another thread needs to reach it mid-search.
struct SearchState {
	std::atomic<bool>& stop_requested;
	std::chrono::steady_clock::time_point deadline;
	uint64_t nodes = 0;
	Move root_best_move{NULL_SQUARE, NULL_SQUARE};

	explicit SearchState(std::atomic<bool>& stop) : stop_requested(stop) {}
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

bool same_move(Move a, Move b) { return a.from == b.from && a.to == b.to && a.promotion == b.promotion; }

int16_t negamax(Position& pos, int depth, int16_t alpha, int16_t beta, int ply, SearchState& state,
                TranspositionTable& tt) {
	state.nodes++;

	// Node-count modulus, not every node -- too slow per-node, too rare misses the time control.
	if ((state.nodes & 0x7FF) == 0 && time_up(state)) state.stop_requested.store(true, std::memory_order_relaxed);
	if (state.stop_requested.load(std::memory_order_relaxed)) return eval::evaluate(pos);

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

	if (depth == 0) return eval::evaluate(pos);

	MoveList moves;
	generate_legal_moves(moves, pos);
	if (moves.count == 0) {
		// Mate score shrinks by one per ply toward the root, so a faster mate outscores a slower one.
		if (is_in_check(pos, pos.side_to_move)) return static_cast<int16_t>(-kMateScore + ply);
		return 0;
	}

	// TT move first: cheap, high-value ordering ahead of Milestone 4's MVV-LVA/killers/history.
	if (tt_move.from != NULL_SQUARE) {
		for (int i = 0; i < moves.count; i++) {
			if (same_move(moves.moves[i], tt_move)) {
				std::swap(moves.moves[0], moves.moves[i]);
				break;
			}
		}
	}

	// Fail-soft: `best` tracks the true best score rather than clamping to [alpha, beta].
	int16_t best = static_cast<int16_t>(-kInfiniteScore);
	Move best_move = moves.moves[0];
	for (int i = 0; i < moves.count; i++) {
		StateInfo undo;
		make_move(pos, moves.moves[i], undo);
		int16_t score = static_cast<int16_t>(
			-negamax(pos, depth - 1, static_cast<int16_t>(-beta), static_cast<int16_t>(-alpha), ply + 1, state, tt));
		unmake_move(pos, moves.moves[i], undo);

		if (score > best) {
			best = score;
			best_move = moves.moves[i];
			if (ply == 0) state.root_best_move = moves.moves[i];
		}
		if (best > alpha) alpha = best;
		if (alpha >= beta) break;

		if (state.stop_requested.load(std::memory_order_relaxed)) break;
	}

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
                    std::atomic<bool>& stop_requested, const InfoCallback& on_info) {
	tt.new_search();
	SearchState state(stop_requested);
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
