#include "search/clock.h"

#include <algorithm>

namespace search {

namespace {

constexpr int kAssumedMovesToGo = 30;  // classical-time-control heuristic when movestogo isn't given
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
	int moves_to_go = limits.movestogo > 0 ? limits.movestogo : kAssumedMovesToGo;
	long long budget = time_left / moves_to_go + increment;
	return std::min(budget, time_left / 2);
}

}  // namespace search
