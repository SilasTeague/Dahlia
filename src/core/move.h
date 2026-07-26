#pragma once

#include "core/types.h"

struct Move {
	Square from;
	Square to;
	Promotion promotion = NO_PROMOTION;
};
