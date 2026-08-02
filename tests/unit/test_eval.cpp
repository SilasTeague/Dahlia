#include <catch2/catch_test_macros.hpp>

#include "eval/evaluate.h"
#include "position/position.h"

// Material-only eval (REFERENCE.md 3.6, v0).

TEST_CASE("evaluate: start position is materially equal", "[eval]") {
	Position pos = parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	CHECK(eval::evaluate(pos) == 0);
}

TEST_CASE("evaluate: side to move perspective flips with side to move", "[eval]") {
	// Same material imbalance (white is up a queen), viewed from each side.
	Position white_to_move = parse_fen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
	Position black_to_move = parse_fen("4k3/8/8/8/8/8/8/4K2Q b - - 0 1");

	CHECK(eval::evaluate(white_to_move) > 0);
	CHECK(eval::evaluate(black_to_move) == -eval::evaluate(white_to_move));
}

TEST_CASE("evaluate: material counts sum piece values across multiple imbalances", "[eval]") {
	// White has a lone extra queen (900cp) vs. a bare king.
	Position queen_up = parse_fen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
	CHECK(eval::evaluate(queen_up) == 900);

	// Same extra queen, but Black also has a bishop (330cp): net +570cp.
	Position queen_up_minus_bishop = parse_fen("4kb2/8/8/8/8/8/8/4K2Q w - - 0 1");
	CHECK(eval::evaluate(queen_up_minus_bishop) == 900 - 330);
}
