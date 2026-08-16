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

// Negamax alpha-beta search with iterative deepening, a transposition table,
// staged move ordering, and quiescence at the leaves (REFERENCE.md 3.7/3.8,
// Milestone 4).
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

// Switches for the search's inexact machinery, all defaulting to on.
// Deliberately not part of SearchLimits: those are the GUI's instructions,
// while these are the engine's own settings, and no UCI command sets them.
//
// They exist because Milestone 6 asks for re-search correctness tests against a
// "safe mode" search, and that only means something if the safe search is
// reachable from the same build as the real one. `exact()` below is that safe
// mode. tests/unit/test_research.cpp is the only caller that changes any of
// these; nothing in src/ does.
//
// The line these switches draw is between the parts of the search that are
// *exact* -- alpha-beta itself, PVS, iterative deepening, quiescence's stand-pat
// -- and the parts that are allowed to be wrong in exchange for speed. Only the
// second kind is switchable, and the list is short enough to state: aspiration
// windows and the transposition table are exact in their own arithmetic but
// change results through the table; late move reductions, null-move pruning and
// delta pruning can each discard a line that mattered.
struct SearchTuning {
	bool aspiration_windows = true;
	bool late_move_reductions = true;

	// Null-move pruning (search.cpp) and quiescence's delta pruning. Both are
	// heuristics that read the current window -- null-move searches against
	// beta, delta pruning discards captures that cannot reach alpha -- so both
	// prune differently when a window narrows, and both can prune something
	// real. They are the reason a narrower window changes this engine's scores
	// even with the table's cutoffs disabled, and therefore the reason they
	// need switches at all: without them, "does the aspiration window change
	// the answer?" cannot be asked in isolation.
	bool null_move_pruning = true;
	bool delta_pruning = true;

	// Whether a transposition table hit may cut a search short, as opposed to
	// only supplying a move to try first. Off, the table still fills, still
	// orders moves, and still costs what it costs -- it just stops answering
	// questions on the search's behalf.
	//
	// This is here because "exact" is not the same claim as "returns the same
	// score", and the difference is entirely the table. A narrow-window search
	// stores *bounds* where a full-window search would have stored exact
	// scores; both are correct, but a later probe can only cut off on the
	// second kind, so the engine reads more effective depth out of a table
	// filled by wide windows than by narrow ones. Every narrowing technique in
	// this engine therefore changes results through the table while changing
	// nothing about its own arithmetic.
	//
	// Milestone 5 established that for PVS by rebuilding the engine twice by
	// hand and comparing. This switch makes the same verification a test that
	// runs in CI -- see tests/unit/test_research.cpp.
	bool transposition_cutoffs = true;

	// Every inexact technique off: what is left is alpha-beta with PVS,
	// quiescence and move ordering, none of which may change a score. Slow by
	// design -- this is a reference to compare against, not a way to play.
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
//
// `move_history` is the butterfly history table used for move ordering. Like
// the transposition table it is caller-owned rather than rebuilt per search,
// because its value comes from accumulating across the moves of one game; the
// caller is responsible for clearing it on `ucinewgame`. Unlike the TT it is
// written to on every beta cutoff, so it is passed by mutable reference.
//
// `history` is the line of positions played before `pos`, which the search
// needs to score threefold repetitions correctly; the default -- no history
// -- means "the game starts at `pos`", which only costs the search the
// repetitions it can't see anyway. think() copies it and leaves the caller's
// copy untouched.
//
// `tuning` switches off the heuristics that are allowed to be wrong (see
// SearchTuning); the default -- everything on -- is what the engine plays with,
// and only the tests pass anything else.
SearchResult think(Position& pos, const SearchLimits& limits, TranspositionTable& tt,
                    HistoryTable& move_history, std::atomic<bool>& stop_requested,
                    const InfoCallback& on_info = nullptr,
                    const PositionHistory& history = PositionHistory{},
                    const SearchTuning& tuning = SearchTuning{});

}  // namespace search
