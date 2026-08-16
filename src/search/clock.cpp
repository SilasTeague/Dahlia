#include "search/clock.h"

#include <algorithm>

namespace search {

namespace {

constexpr int kBaseDivisor = 20;  // spend ~1/20th of the remaining clock per move
constexpr int kIncrementDivisor = 2;  // ...plus half the increment the move earns back
constexpr long long kDefaultBudgetMs = 200;  // no time control at all (e.g. bare "go"): fixed anytime search budget
constexpr long long kUnboundedBudgetMs = 24LL * 60 * 60 * 1000;  // "depth"/"infinite" cap the search instead of time

// Answer before the deadline, not on it: everything after the search returns
// happens on the GUI's clock, and a zero time margin scores that as a forfeit.
// Never yields less than 1 ms -- the engine must still return something.
long long minus_overhead(long long budget, long long overhead) {
	return std::max(1LL, budget - overhead);
}

}  // namespace

long long time_budget_ms(const SearchLimits& limits, Color side_to_move) {
	long long overhead = std::max(0LL, limits.move_overhead_ms);

	if (limits.movetime_ms >= 0) return minus_overhead(limits.movetime_ms, overhead);

	long long time_left = side_to_move == WHITE ? limits.wtime_ms : limits.btime_ms;
	if (time_left < 0) {
		// Neither branch is a GUI-imposed deadline, so neither needs the reserve.
		return (limits.infinite || limits.depth > 0) ? kUnboundedBudgetMs : kDefaultBudgetMs;
	}

	long long increment = side_to_move == WHITE ? limits.winc_ms : limits.binc_ms;
	// `movestogo` only tightens the divisor, so time isn't left unspent at the control.
	int divisor = limits.movestogo > 0 ? std::min(limits.movestogo, kBaseDivisor) : kBaseDivisor;
	long long budget = std::min(time_left / divisor + increment / kIncrementDivisor, time_left / 2);
	return minus_overhead(budget, overhead);
}

}  // namespace search
