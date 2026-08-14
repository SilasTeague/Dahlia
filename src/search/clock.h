#pragma once

#include "core/types.h"
#include "search/search.h"

// Converts UCI `go` time-control parameters into a single-move time budget
// (REFERENCE.md 3.9). Simple formula-based allocation for now: a twentieth of
// the remaining clock plus half the increment, capped so one move can't spend
// the whole clock, minus `limits.move_overhead_ms` so the reply lands before
// the deadline rather than on it. Soft/hard limit split and instability-based
// extension are deferred until iterative deepening's move-quality signal
// exists to drive them.
namespace search {

long long time_budget_ms(const SearchLimits& limits, Color side_to_move);

}  // namespace search
