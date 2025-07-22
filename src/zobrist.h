#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"
#include <stdint.h>

extern uint64_t psq[12][64];
extern uint64_t enpassant[8];
extern uint64_t castling[16];
extern uint64_t side;

uint64_t random64(uint64_t n);

void init_zobrist();

uint64_t compute_hash(const Board* board);

#endif