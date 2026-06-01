#pragma once

#include <cstdint>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;


// The chess board is 64 squares, so it can be represented by a 64 bit unisgned int
// We will use LSB = a1 and MSB = h8
using Bitboard = uint64_t;

enum Piece : u8 {
	PAWN = 0, 
	KNIGHT, 
	BISHOP, 
	ROOK, 
	QUEEN, 
	KING,
	NULL_PIECE = 255
};


enum Promotion : u8 {
	NO_PROMOTION = 0, 
	PROMOTION_KNIGHT, 
	PROMOTION_BISHOP, 
	PROMOTION_QUEEN
};

// We will start with a1 = 0, b1 = 1... such that
// each square also represents the shift value
enum Square : u8 {
	a1 = 0, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8,
	NULL_SQUARE = 255
};


enum Color : u8 {
	WHITE = 0, BLACK
};

enum CastlingRights : u8 {
	NO_CASTLING = 0,
	WHITE_00,
	WHITE_000 = WHITE_00 << 1,
	BLACK_00 = WHITE_00 << 2,
	BLACK_000 = WHITE_00 << 3,

	KING_SIDE = WHITE_00 | BLACK_00,
	QUEEN_SIDE = WHITE_000 | BLACK_000,
	WHITE_CASTLING = WHITE_00 | WHITE_000,
	BLACK_CASTLING = BLACK_00 | BLACK_000,
	ANY_CASTLING = WHITE_CASTLING | BLACK_CASTLING
};
