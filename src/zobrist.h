#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"
#include <stdint.h>

uint64_t random64(uint64_t n);

void init_zobrist();

uint64_t compute_hash(const Board* board);

#endif