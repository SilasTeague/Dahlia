#pragma once

#include "types.h"
#include "position.h"
#include "move.h"

Move* generate_pawn_moves(Position position);

Move* generate_knight_moves(Position position);

Move* generate_sliding_moves(Position position, int* directions);
