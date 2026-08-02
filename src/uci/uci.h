#pragma once

#include <istream>
#include <ostream>

// UCI protocol loop (REFERENCE.md 3.10): responds to
// uci/isready/ucinewgame/position/go/stop/quit. `go` runs Milestone 3's
// iterative-deepening negamax alpha-beta search (search/search.h) rather
// than Milestone 2's random legal move.
//
// Takes an istream/ostream rather than hardcoding std::cin/std::cout so
// scripted protocol tests can drive it without a real process (see
// tests/unit/test_uci.cpp).
void run_uci_loop(std::istream& in, std::ostream& out);
