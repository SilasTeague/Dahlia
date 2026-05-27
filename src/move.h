#pragma once

#include "types.h"

struct Move {
	Square from;
	Square to;
	Piece promotion = Piece::EMPTY;
};
