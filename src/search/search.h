#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "core/move.h"
#include "core/types.h"
#include "position/position.h"
#include "search/tt.h"

// Negamax alpha-beta search with iterative deepening and a transposition
// table (REFERENCE.md 3.7/3.8, Milestone 4). Move ordering beyond the TT
// move (MVV-LVA, killers, history) and quiescence are still deferred.
namespace search {

// Milliseconds held back from every time-limited search to cover the gap
// between search returning and the GUI seeing `bestmove` (REFERENCE.md 3.9).
// Overridable via the `Move Overhead` UCI option.
constexpr long long kDefaultMoveOverheadMs = 10;

// UCI `go` parameters, already parsed out of protocol tokens. -1 means
// "not given" for the millisecond fields; 0 means "not given" for depth/
// movestogo. `move_overhead_ms` is the exception: it comes from `setoption`,
// not `go`, and is stamped in by the caller when a search is launched.
struct SearchLimits {
	int depth = 0;
	long long movetime_ms = -1;
	long long wtime_ms = -1;
	long long btime_ms = -1;
	long long winc_ms = 0;
	long long binc_ms = 0;
	int movestogo = 0;
	bool infinite = false;
	long long move_overhead_ms = kDefaultMoveOverheadMs;
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

// One fully-formatted UCI `info` line (no trailing newline), emitted once
// per completed depth.
using InfoCallback = std::function<void(const std::string& line)>;

// Iteratively deepens (depth 1..limits.depth, or until the time budget from
// `limits` runs out) and returns the best move found at the last
// fully-completed depth. `stop_requested` is checked on a node-count
// modulus (not per-node) and may be set from another thread -- e.g. a UCI
// `stop` -- to make think() return promptly with the best move found so far
// (REFERENCE.md 3.8/3.9/3.10). Setting it before calling think() (e.g. a
// `stop` that raced a `go`) still returns a legal move, just an unsearched
// one. `on_info`, if given, is called with one complete line per completed
// depth; think() never writes to any stream itself, so the caller decides
// how to serialize output across threads.
SearchResult think(Position& pos, const SearchLimits& limits, TranspositionTable& tt,
                    std::atomic<bool>& stop_requested, const InfoCallback& on_info = nullptr);

}  // namespace search
