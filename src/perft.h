#ifndef PERFT_H
#define PERFT_H

#include <stdint.h>
#include "board.h"

uint64_t perft(Board* board, int depth);

#endif