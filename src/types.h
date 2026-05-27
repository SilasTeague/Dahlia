#pragma once

#include <cstdint>

enum Piece : uint8_t {
	EMPTY = 0,
	WHITE_KING,
	WHITE_QUEEN,
	WHITE_ROOK,
	WHITE_BISHOP,
	WHITE_KNIGHT,
	WHITE_PAWN,
	BLACK_KING,
	BLACK_QUEEN,
	BLACK_ROOK,
	BLACK_BISHOP,
	BLACK_KNIGHT,
	BLACK_PAWN,

};

// We will start with a1 = 0, b1 = 1... such that
// each square also represents the shift value
enum Square : uint8_t {
	a1 = 0, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8,
};


enum Color : uint8_t {
	WHITE = 0, BLACK
};

enum CastlingRights : uint8_t {
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
