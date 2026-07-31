#include "search/search.h"

#include <chrono>

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
struct SearchState {
	bool stop_requested = false;
	std::chrono::steady_clock::time_point deadline;
	uint64_t nodes = 0;
	Move root_best_move{NULL_SQUARE, NULL_SQUARE};
};

bool time_up(const SearchState& state) {
	return std::chrono::steady_clock::now() >= state.deadline;
}

int16_t negamax(Position& pos, int depth, int16_t alpha, int16_t beta, int ply, SearchState& state) {
	state.nodes++;

	// Check the clock via a node-count modulus, not every node (REFERENCE.md
	// 3.8 pitfall: checking every node is too slow, too rarely blows the time
	// control).
	if ((state.nodes & 0x7FF) == 0 && time_up(state)) state.stop_requested = true;
	if (state.stop_requested) return eval::evaluate(pos);

	if (depth == 0) return eval::evaluate(pos);

	MoveList moves;
	generate_legal_moves(moves, pos);
	if (moves.count == 0) {
		// Mate score shrinks by one per ply on the way back to the root, so
		// a faster mate (found at a shallower ply) always scores better than
		// a slower one (REFERENCE.md 3.8 pitfall on mate-distance handling).
		if (is_in_check(pos, pos.side_to_move)) return static_cast<int16_t>(-kMateScore + ply);
		return 0;
	}

	// Fail-soft: `best` tracks the true best score found, rather than being
	// clamped to [alpha, beta]. Chosen because Milestone 4's planned PVS
	// re-search logic expects fail-soft scores (REFERENCE.md 3.8 pitfall on
	// not mixing fail-soft/fail-hard).
	int16_t best = static_cast<int16_t>(-kInfiniteScore);
	for (int i = 0; i < moves.count; i++) {
		StateInfo undo;
		make_move(pos, moves.moves[i], undo);
		int16_t score = static_cast<int16_t>(
			-negamax(pos, depth - 1, static_cast<int16_t>(-beta), static_cast<int16_t>(-alpha), ply + 1, state));
		unmake_move(pos, moves.moves[i], undo);

		if (score > best) {
			best = score;
			if (ply == 0) state.root_best_move = moves.moves[i];
		}
		if (best > alpha) alpha = best;
		if (alpha >= beta) break;

		if (state.stop_requested) break;
	}
	return best;
}

}  // namespace

SearchResult think(Position& pos, const SearchLimits& limits, std::ostream* info_out) {
	SearchState state;
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
		int16_t score = negamax(pos, depth, static_cast<int16_t>(-kInfiniteScore), kInfiniteScore, 0, state);

		// Depth 1 always gets its partial best move used (alpha-beta already
		// updates root_best_move as soon as any move improves on -infinity),
		// but deeper aborted iterations are discarded in favor of the last
		// fully-completed depth's result.
		if (state.stop_requested && depth > 1) break;

		result.score = score;
		result.depth_reached = depth;
		result.nodes = state.nodes;
		if (state.root_best_move.from != NULL_SQUARE) result.best_move = state.root_best_move;

		if (info_out) {
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - start).count();
			uint64_t nps = elapsed_ms > 0 ? state.nodes * 1000 / static_cast<uint64_t>(elapsed_ms) : state.nodes * 1000;

			*info_out << "info depth " << depth;
			if (is_mate_score(score)) {
				int plies_to_mate = kMateScore - (score > 0 ? score : static_cast<int16_t>(-score));
				int moves_to_mate = (plies_to_mate + 1) / 2;
				*info_out << " score mate " << (score > 0 ? moves_to_mate : -moves_to_mate);
			} else {
				*info_out << " score cp " << score;
			}
			*info_out << " nodes " << state.nodes << " nps " << nps << " time " << elapsed_ms << "\n";
		}

		if (state.stop_requested || time_up(state)) break;
	}

	return result;
}

}  // namespace search
