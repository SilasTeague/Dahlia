#pragma once

#include "core/types.h"
#include "position/position.h"

extern Bitboard knight_attacks[64];
extern Bitboard king_attacks[64];
extern Bitboard pawn_attacks[2][64];

void init_attack_tables();

Bitboard bishop_attacks(Square square, Bitboard occupied);

Bitboard rook_attacks(Square square, Bitboard occupied);

Bitboard queen_attacks(Square square, Bitboard occupied);

bool is_attacked(const Position& pos, Square square, Color by);
