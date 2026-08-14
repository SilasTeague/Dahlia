#include "search/clock.h"

#include <algorithm>

namespace search {

namespace {

constexpr int kBaseDivisor = 20;  // spend ~1/20th of the remaining clock per move
constexpr int kIncrementDivisor = 2;  // ...plus half the increment the move earns back
constexpr long long kDefaultBudgetMs = 200;  // no time control at all (e.g. bare "go"): fixed anytime search budget
constexpr long long kUnboundedBudgetMs = 24LL * 60 * 60 * 1000;  // "depth"/"infinite" cap the search instead of time

// Hold back `overhead` ms so the engine answers *before* its deadline rather
// than exactly on it: everything after the search returns -- serializing the
// output, the write and flush, transport to the GUI -- happens on the GUI's
// clock, and a GUI with a zero time margin (cutechess-cli's default) scores
// an on-the-deadline reply as a forfeit. Never yields less than 1 ms: an
// overhead larger than the budget means the engine is in desperate time
// trouble and must still return something.
long long minus_overhead(long long budget, long long overhead) {
	return std::max(1LL, budget - overhead);
}

}  // namespace

long long time_budget_ms(const SearchLimits& limits, Color side_to_move) {
	long long overhead = std::max(0LL, limits.move_overhead_ms);

	if (limits.movetime_ms >= 0) return minus_overhead(limits.movetime_ms, overhead);

	long long time_left = side_to_move == WHITE ? limits.wtime_ms : limits.btime_ms;
	if (time_left < 0) {
		// Neither branch is a GUI-imposed deadline, so neither needs the
		// reserve: kUnboundedBudgetMs exists only to keep the deadline
		// comparison well-formed until `stop` arrives, and kDefaultBudgetMs is
		// a budget Dahlia picked for itself.
		return (limits.infinite || limits.depth > 0) ? kUnboundedBudgetMs : kDefaultBudgetMs;
	}

	long long increment = side_to_move == WHITE ? limits.winc_ms : limits.binc_ms;
	// base/20 + inc/2. `movestogo` only tightens the divisor: with fewer than
	// 20 moves left before the next time control, 1/20th per move would leave
	// time unspent at the control and risk flagging near it.
	int divisor = limits.movestogo > 0 ? std::min(limits.movestogo, kBaseDivisor) : kBaseDivisor;
	long long budget = std::min(time_left / divisor + increment / kIncrementDivisor, time_left / 2);
	return minus_overhead(budget, overhead);
}

}  // namespace search
