#pragma once

#include <istream>
#include <ostream>

// UCI protocol loop (REFERENCE.md 3.10): responds to
// uci/isready/ucinewgame/position/go/stop/quit. `go` runs the
// iterative-deepening negamax alpha-beta search (search/search.h) on its
// own thread, so the loop stays responsive to `stop`/`isready`/`quit` for
// the duration of a search -- see docs/adr/0003-async-search-stop.md.
//
// Takes an istream/ostream rather than hardcoding std::cin/std::cout so
// scripted protocol tests can drive it without a real process (see
// tests/unit/test_uci.cpp).
void run_uci_loop(std::istream& in, std::ostream& out);
