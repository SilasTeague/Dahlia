#include <catch2/catch_test_macros.hpp>

#include "position/position.h"

TEST_CASE("parse_fen: start position", "[position][fen]") {
	Position pos = parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	CHECK(pos.side_to_move == WHITE);
	CHECK(pos.castling_rights == ANY_CASTLING);
	CHECK(pos.en_passant_square == NULL_SQUARE);
	CHECK(pos.halfmove_clock == 0);
	CHECK(pos.fullmove_count == 1);

	CHECK(pos.board[a1] == ROOK);
	CHECK(pos.board[e1] == KING);
	CHECK(pos.board[e8] == KING);
	CHECK(pos.board[a2] == PAWN);
	CHECK(pos.board[e4] == NULL_PIECE);

	CHECK(pos.pieces[WHITE][KING] == (1ULL << e1));
	CHECK(pos.pieces[BLACK][KING] == (1ULL << e8));
	CHECK(pos.aggregates[ALL] == (pos.aggregates[WHITE] | pos.aggregates[BLACK]));
	CHECK(pos.aggregates[WHITE] == 0x000000000000FFFFULL);
	CHECK(pos.aggregates[BLACK] == 0xFFFF000000000000ULL);
}

TEST_CASE("parse_fen: Kiwipete (castling, en passant, missing pieces)", "[position][fen]") {
	Position pos = parse_fen(
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

	CHECK(pos.side_to_move == WHITE);
	CHECK(pos.castling_rights == ANY_CASTLING);
	CHECK(pos.board[a8] == ROOK);
	CHECK(pos.board[e8] == KING);
	CHECK(pos.board[b8] == NULL_PIECE);
	CHECK(pos.board[d5] == PAWN);
}

TEST_CASE("parse_fen: en passant square and partial castling rights", "[position][fen]") {
	Position pos = parse_fen("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w Kq e6 0 2");

	CHECK(pos.en_passant_square == e6);
	CHECK(pos.castling_rights == (WHITE_00 | BLACK_000));
	CHECK((pos.castling_rights & WHITE_000) == 0);
	CHECK((pos.castling_rights & BLACK_00) == 0);
}

TEST_CASE("to_fen round-trips through parse_fen", "[position][fen]") {
	const char* fens[] = {
		"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
		"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w Kq e6 0 2",
		"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 b - - 3 40",
	};

	for (const char* fen : fens) {
		Position pos = parse_fen(fen);
		CHECK(to_fen(pos) == fen);
	}
}
