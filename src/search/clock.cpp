#include "search/clock.h"

#include <algorithm>

namespace search {

namespace {

constexpr int kBaseDivisor = 20;  // spend ~1/20th of the remaining clock per move
constexpr int kIncrementDivisor = 2;  // ...plus half the increment the move earns back
constexpr long long kDefaultBudgetMs = 200;  // no time control at all (e.g. bare "go"): fixed anytime search budget
constexpr long long kUnboundedBudgetMs = 24LL * 60 * 60 * 1000;  // "depth"/"infinite" cap the search instead of time

}  // namespace

long long time_budget_ms(const SearchLimits& limits, Color side_to_move) {
	if (limits.movetime_ms >= 0) return limits.movetime_ms;

	long long time_left = side_to_move == WHITE ? limits.wtime_ms : limits.btime_ms;
	if (time_left < 0) {
		return (limits.infinite || limits.depth > 0) ? kUnboundedBudgetMs : kDefaultBudgetMs;
	}

	long long increment = side_to_move == WHITE ? limits.winc_ms : limits.binc_ms;
	// base/20 + inc/2. `movestogo` only tightens the divisor: with fewer than
	// 20 moves left before the next time control, 1/20th per move would leave
	// time unspent at the control and risk flagging near it.
	int divisor = limits.movestogo > 0 ? std::min(limits.movestogo, kBaseDivisor) : kBaseDivisor;
	long long budget = time_left / divisor + increment / kIncrementDivisor;
	return std::min(budget, time_left / 2);
}

}  // namespace search
