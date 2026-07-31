#pragma once

#include <istream>
#include <ostream>
#include <random>

// Minimal UCI protocol loop (REFERENCE.md 3.10 / Milestone 2): enough to be
// GUI-testable — uci/isready/ucinewgame/position/go/stop/quit — before real
// search or evaluation exist. `go` picks a uniformly random legal move
// rather than searching; Milestone 3 replaces that with alpha-beta.
//
// Takes an istream/ostream rather than hardcoding std::cin/std::cout so
// scripted protocol tests can drive it without a real process (see
// tests/unit/test_uci.cpp). `seed` defaults to a real random seed; tests
// pass a fixed value for deterministic `go` output.
void run_uci_loop(std::istream& in, std::ostream& out, unsigned int seed = std::random_device{}());
