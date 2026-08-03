#pragma once

#include <cstdint>
#include <ostream>

#include "core/move.h"
#include "core/types.h"
#include "position/position.h"
#include "search/tt.h"

// Negamax alpha-beta search with iterative deepening and a transposition
// table (REFERENCE.md 3.7/3.8, Milestone 4). Move ordering beyond the TT
// move (MVV-LVA, killers, history) and quiescence are still deferred.
namespace search {

// UCI `go` parameters, already parsed out of protocol tokens. -1 means
// "not given" for the millisecond fields; 0 means "not given" for depth/
// movestogo.
struct SearchLimits {
	int depth = 0;
	long long movetime_ms = -1;
	long long wtime_ms = -1;
	long long btime_ms = -1;
	long long winc_ms = 0;
	long long binc_ms = 0;
	int movestogo = 0;
	bool infinite = false;
};

struct SearchResult {
	Move best_move{NULL_SQUARE, NULL_SQUARE};
	int16_t score = 0;
	int depth_reached = 0;
	uint64_t nodes = 0;
};

// Score magnitude reserved for mate detection; a returned score is a "mate
// in N plies" score once it's within kMaxPly of kMateScore (see
// is_mate_score in search.cpp). Kept well below int16_t's range so ply
// adjustments (+/- up to kMaxPly) never overflow.
constexpr int16_t kMateScore = 32000;
constexpr int16_t kInfiniteScore = kMateScore + 1;
constexpr int kMaxPly = 64;

// Iteratively deepens (depth 1..limits.depth, or until the time budget from
// `limits` runs out) and returns the best move found at the last
// fully-completed depth. Emits one UCI `info` line per completed depth to
// `info_out` when given.
SearchResult think(Position& pos, const SearchLimits& limits, TranspositionTable& tt, std::ostream* info_out = nullptr);

}  // namespace search
