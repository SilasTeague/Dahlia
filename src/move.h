#pragma once

#include "types.h"

struct Move {
	Square from;
	Square to;
	Promotion promotion = NO_PROMOTION;
};
