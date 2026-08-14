#pragma once

#include <cstdint>

#include "core/types.h"
#include "position/position.h"

// Material-only evaluation (REFERENCE.md 3.6, v0): enough to make search
// meaningful and testable before PSTs or other terms exist.
namespace eval {

// Centipawn score from the side-to-move's perspective: positive means the
// side to move is better. Symmetric by construction (see test_eval.cpp).
int16_t evaluate(const Position& pos);

// The centipawn value evaluate() assigns to one `piece`. Exposed because the
// search's delta pruning needs to ask "could winning this piece plausibly
// raise the score to alpha?" -- a question that has to be answered in the same
// units evaluate() returns, or the margin means nothing. KING is 0: it is
// never captured and never counted.
int16_t piece_value(Piece piece);

}  // namespace eval
