#pragma once

#include <cstdint>

#include "core/types.h"
#include "position/position.h"

// Material + piece-square tables tapered by the material left on the board, with
// no mobility, pawn structure or king safety terms. See docs/evaluation.md.
namespace eval {

// Centipawns from the side-to-move's perspective, exactly antisymmetric under a
// colour-swapped mirror -- which test_eval.cpp asserts directly.
int16_t evaluate(const Position& pos);

// The phase-independent value of one piece: the larger of its midgame and
// endgame values, so a caller reasoning about "at most this much material" is
// never told a piece is worth less than it might be. KING is 0.
int16_t piece_value(Piece piece);

}  // namespace eval
