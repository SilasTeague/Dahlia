#pragma once

#include <cstdint>

#include "core/types.h"
#include "position/position.h"

// Perft (performance test / node-count enumeration), the movegen + make/
// unmake correctness oracle described in REFERENCE.md 3.5. Uses the real
// make_move/unmake_move (mutate pos in place, restore via undo) rather than
// copying Position per node, unlike the throwaway Milestone-1 harness this
// replaces.

// Total leaf-node count at `depth`, filtering out pseudo-legal moves that
// leave the moving side's king in check.
uint64_t perft(Position& pos, int depth);

// Same node count as perft(), additionally bucketed by which piece type made
// the *root* move of each line (e.g. by_piece[PAWN] is the total leaf-node
// count reachable by pawn moves at the root). Useful for spotting "same
// total, wrong shape" regressions that a bare node count can't catch.
struct PerftBreakdown {
	uint64_t nodes = 0;
	uint64_t by_piece[6] = {};  // indexed by Piece (PAWN..KING)
};

PerftBreakdown perft_divide_by_piece_type(Position& pos, int depth);
