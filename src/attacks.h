#pragma once

#include "types.h"
#include "position.h"

extern Bitboard knight_attacks[64];
extern Bitboard king_attacks[64];
extern Bitboard pawn_attacks[2][64];

void init_attack_tables();

bool is_attacked(const Position& pos, Square square);
