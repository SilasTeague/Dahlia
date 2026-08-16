#pragma once

#include <cstdint>

#include "core/types.h"
#include "position/position.h"

// Perft, the movegen + make/unmake correctness oracle, using the real
// make_move/unmake_move rather than copying Position per node.

// Total leaf-node count at `depth`, filtering out pseudo-legal moves that
// leave the moving side's king in check.
uint64_t perft(Position& pos, int depth);

// Same total as perft(), bucketed by which piece type made the root move, which
// catches "same total, wrong shape" regressions a bare count can't.
struct PerftBreakdown {
	uint64_t nodes = 0;
	uint64_t by_piece[6] = {};  // indexed by Piece (PAWN..KING)
};

PerftBreakdown perft_divide_by_piece_type(Position& pos, int depth);
