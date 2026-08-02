#pragma once

#include <cstdint>

#include "position/position.h"

// Material-only evaluation (REFERENCE.md 3.6, v0): enough to make search
// meaningful and testable before PSTs or other terms exist.
namespace eval {

// Centipawn score from the side-to-move's perspective: positive means the
// side to move is better. Symmetric by construction (see test_eval.cpp).
int16_t evaluate(const Position& pos);

}  // namespace eval
