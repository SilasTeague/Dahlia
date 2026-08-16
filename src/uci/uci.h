#pragma once

#include <istream>
#include <ostream>

// UCI protocol loop; `go` runs the search on its own thread so the loop stays
// responsive to stop/isready/quit (docs/uci.md). Takes streams rather than
// std::cin/std::cout so scripted tests can drive it without a real process.
void run_uci_loop(std::istream& in, std::ostream& out);
