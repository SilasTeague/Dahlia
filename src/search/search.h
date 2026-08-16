#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "core/move.h"
#include "core/types.h"
#include "position/history.h"
#include "position/position.h"
#include "search/ordering.h"
#include "search/tt.h"

// Negamax alpha-beta with iterative deepening, a transposition table, staged
// move ordering and quiescence at the leaves (docs/search.md).
namespace search {

// Held back from every time-limited search to cover the gap between the search
// returning and the GUI seeing `bestmove`; overridable via `Move Overhead`.
constexpr long long kDefaultMoveOverheadMs = 10;

// Parsed `go` parameters. -1 means "not given" for the millisecond fields, 0 for
// depth/movestogo. `move_overhead_ms` comes from `setoption`, not `go`.
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

// Switches for the search's inexact machinery, all defaulting to on. No UCI
// command reaches them; only test_research.cpp changes any (docs/search.md).
struct SearchTuning {
	bool aspiration_windows = true;
	bool late_move_reductions = true;

	// Both read the current window, so both prune differently when one narrows.
	bool null_move_pruning = true;
	bool delta_pruning = true;

	// Whether a TT hit may cut a search short, as opposed to only supplying a move
	// to try first -- off, the table still fills and still orders moves.
	bool transposition_cutoffs = true;

	// Everything inexact off: alpha-beta with PVS, quiescence and ordering, none
	// of which may change a score. Slow by design -- a reference, not a way to play.
	static SearchTuning exact() {
		SearchTuning tuning;
		tuning.aspiration_windows = false;
		tuning.late_move_reductions = false;
		tuning.null_move_pruning = false;
		tuning.delta_pruning = false;
		tuning.transposition_cutoffs = false;
		return tuning;
	}
};

// A score within kMaxPly of kMateScore is a "mate in N plies" score, kept well
// below int16_t's range so ply adjustments never overflow.
constexpr int16_t kMateScore = 32000;
constexpr int16_t kInfiniteScore = kMateScore + 1;
constexpr int kMaxPly = 64;

// One fully-formatted UCI `info` line, no trailing newline.
using InfoCallback = std::function<void(const std::string& line)>;

// Iteratively deepens and returns the best move from the last fully-completed
// depth. Never writes to a stream, so the caller serializes `on_info` output.
// `stop_requested` may be set from another thread; `move_history` is
// caller-owned like the TT; `history` is the line played before `pos`, copied.
SearchResult think(Position& pos, const SearchLimits& limits, TranspositionTable& tt,
                    HistoryTable& move_history, std::atomic<bool>& stop_requested,
                    const InfoCallback& on_info = nullptr,
                    const PositionHistory& history = PositionHistory{},
                    const SearchTuning& tuning = SearchTuning{});

}  // namespace search
