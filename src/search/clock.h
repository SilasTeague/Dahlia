#pragma once

#include "core/types.h"
#include "search/search.h"

// Converts UCI `go` time-control parameters into a single-move time budget:
// a twentieth of the remaining clock plus half the increment, capped, minus the
// move-overhead reserve. See docs/search.md#time-management.
namespace search {

long long time_budget_ms(const SearchLimits& limits, Color side_to_move);

}  // namespace search
