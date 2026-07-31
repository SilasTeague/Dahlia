#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "movegen/attacks.h"
#include "movegen/movegen.h"
#include "position/position.h"
#include "position/zobrist.h"

// Property test per REFERENCE.md 1.6/3.4: make_move followed by unmake_move
// must restore the exact prior position, including the incrementally
// maintained Zobrist key. Also checks make_move's incremental key update
// always agrees with a from-scratch recomputation (the correctness oracle
// for the incremental update, per 3.4's "Zobrist incremental key update...
// with a unit test asserting the incremental key always matches a
// from-scratch computation").

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

bool positions_equal(const Position& a, const Position& b) {
	if (std::memcmp(a.pieces, b.pieces, sizeof(a.pieces)) != 0) return false;
	if (std::memcmp(a.aggregates, b.aggregates, sizeof(a.aggregates)) != 0) return false;
	if (std::memcmp(a.board, b.board, sizeof(a.board)) != 0) return false;
	return a.side_to_move == b.side_to_move &&
		a.castling_rights == b.castling_rights &&
		a.en_passant_square == b.en_passant_square &&
		a.halfmove_clock == b.halfmove_clock &&
		a.fullmove_count == b.fullmove_count &&
		a.zobrist_key == b.zobrist_key;
}

// Exercises every pseudo-legal move from `fen`: make_move, check the
// incremental key against a from-scratch computation, unmake_move, and check
// the position is restored exactly.
void check_make_unmake_round_trip(const char* fen) {
	Position pos = parse_fen(fen);
	Position original = pos;

	MoveList moves;
	generate_pseudo_legal_moves(moves, pos);
	REQUIRE(moves.count > 0);

	for (int i = 0; i < moves.count; i++) {
		StateInfo undo;
		make_move(pos, moves.moves[i], undo);

		CHECK(pos.zobrist_key == compute_zobrist_key(pos));

		unmake_move(pos, moves.moves[i], undo);
		CHECK(positions_equal(pos, original));
	}
}

}  // namespace

TEST_CASE("make/unmake round trip: start position", "[position][make_unmake]") {
	check_make_unmake_round_trip("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

TEST_CASE("make/unmake round trip: Kiwipete (castling, captures)", "[position][make_unmake]") {
	check_make_unmake_round_trip("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
}

TEST_CASE("make/unmake round trip: position 3 (endgame)", "[position][make_unmake]") {
	check_make_unmake_round_trip("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
}

TEST_CASE("make/unmake round trip: position 4 (castling, promotion)", "[position][make_unmake]") {
	check_make_unmake_round_trip("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
}

TEST_CASE("make/unmake round trip: en passant capture available", "[position][make_unmake]") {
	check_make_unmake_round_trip("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w Kq e6 0 2");
}

TEST_CASE("is_in_check reflects the position", "[position][check]") {
	// White king on e1 in check from the black rook on e8, open e-file.
	Position pos = parse_fen("4r3/8/8/8/8/8/8/4K3 w - - 0 1");
	CHECK(is_in_check(pos, WHITE));
	CHECK_FALSE(is_in_check(pos, BLACK));
}
