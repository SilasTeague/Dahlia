#include <catch2/catch_test_macros.hpp>

#include "core/types.h"
#include "search/clock.h"
#include "search/search.h"

using search::SearchLimits;
using search::time_budget_ms;

TEST_CASE("clock: movetime is used verbatim", "[clock]") {
	SearchLimits limits;
	limits.movetime_ms = 1500;
	limits.wtime_ms = 60000;  // ignored in favour of the explicit movetime
	CHECK(time_budget_ms(limits, WHITE) == 1500);
}

TEST_CASE("clock: budget is base/20 + increment/2", "[clock]") {
	SearchLimits limits;
	limits.wtime_ms = 60000;
	limits.winc_ms = 1000;
	CHECK(time_budget_ms(limits, WHITE) == 3500);
}

TEST_CASE("clock: budget uses the side-to-move's clock", "[clock]") {
	SearchLimits limits;
	limits.wtime_ms = 60000;
	limits.winc_ms = 1000;
	limits.btime_ms = 20000;
	limits.binc_ms = 0;
	CHECK(time_budget_ms(limits, BLACK) == 1000);
}

TEST_CASE("clock: no increment given means no increment term", "[clock]") {
	SearchLimits limits;
	limits.wtime_ms = 60000;
	CHECK(time_budget_ms(limits, WHITE) == 3000);
}

TEST_CASE("clock: movestogo at or above 20 doesn't change the divisor", "[clock]") {
	SearchLimits limits;
	limits.wtime_ms = 60000;
	limits.movestogo = 40;
	CHECK(time_budget_ms(limits, WHITE) == 3000);
}

TEST_CASE("clock: movestogo below 20 tightens the divisor", "[clock]") {
	SearchLimits limits;
	limits.wtime_ms = 60000;
	limits.winc_ms = 1000;
	limits.movestogo = 5;
	CHECK(time_budget_ms(limits, WHITE) == 12500);
}

TEST_CASE("clock: budget never exceeds half the remaining clock", "[clock]") {
	SearchLimits limits;
	limits.wtime_ms = 1000;
	limits.winc_ms = 30000;  // huge increment relative to the clock
	CHECK(time_budget_ms(limits, WHITE) == 500);
}

TEST_CASE("clock: bare go gets a fixed anytime budget", "[clock]") {
	SearchLimits limits;
	CHECK(time_budget_ms(limits, WHITE) == 200);
}

TEST_CASE("clock: depth and infinite searches aren't time-limited in practice", "[clock]") {
	SearchLimits depth_limits;
	depth_limits.depth = 6;
	CHECK(time_budget_ms(depth_limits, WHITE) > 60LL * 60 * 1000);

	SearchLimits infinite_limits;
	infinite_limits.infinite = true;
	CHECK(time_budget_ms(infinite_limits, WHITE) > 60LL * 60 * 1000);
}
