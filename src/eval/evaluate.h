#pragma once

#include <cstdint>

#include "core/types.h"
#include "position/position.h"

// Material + piece-square tables, tapered between a midgame and an endgame
// score by the material left on the board (REFERENCE.md 3.6, v1; Milestone 5).
// Mobility, pawn structure and king safety are still absent -- those are v2.
namespace eval {

// Centipawn score from the side-to-move's perspective: positive means the
// side to move is better. Exactly antisymmetric under mirroring the position
// and swapping colours, which test_eval.cpp asserts directly rather than
// trusting the table indexing (see test_eval.cpp).
int16_t evaluate(const Position& pos);

// The centipawn value evaluate() assigns to one `piece`, phase-independent:
// the larger of its midgame and endgame values, so a caller reasoning about
// "at most this much material" is never told a piece is worth less than it
// might be. Exposed because the search's delta pruning needs to ask "could
// winning this piece plausibly raise the score to alpha?" -- a question that
// has to be answered in the same units evaluate() returns, or the margin means
// nothing. KING is 0: it is never captured and never counted.
int16_t piece_value(Piece piece);

}  // namespace eval
