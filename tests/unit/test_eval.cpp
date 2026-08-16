#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "eval/evaluate.h"
#include "position/position.h"

// Invariants, not magic numbers: asserting a position scores exactly +47 would
// pin the constants rather than the code and break on every tuning pass. What
// must hold regardless is antisymmetry, that Black's tables mirror White's, and
// that the phase blend moves in the right direction.

namespace {

// Mirrors a FEN across the middle of the board and swaps the colours. This is
// the one transformation a PST bug does not survive: every test that compares a
// position to itself passes with White's table flipped and Black's not.
std::string mirror_fen(const std::string& fen) {
	std::istringstream in(fen);
	std::string placement, side, castling, en_passant, halfmove, fullmove;
	in >> placement >> side >> castling >> en_passant >> halfmove >> fullmove;

	std::vector<std::string> ranks;
	for (size_t start = 0; start <= placement.size();) {
		size_t slash = placement.find('/', start);
		if (slash == std::string::npos) slash = placement.size();
		ranks.push_back(placement.substr(start, slash - start));
		start = slash + 1;
	}
	std::reverse(ranks.begin(), ranks.end());

	std::string mirrored;
	for (size_t i = 0; i < ranks.size(); i++) {
		if (i) mirrored += '/';
		for (char c : ranks[i]) {
			mirrored += static_cast<char>(std::islower(static_cast<unsigned char>(c))
				? std::toupper(static_cast<unsigned char>(c))
				: std::tolower(static_cast<unsigned char>(c)));
		}
	}

	std::string swapped_castling;
	for (char c : castling) {
		swapped_castling += static_cast<char>(std::islower(static_cast<unsigned char>(c))
			? std::toupper(static_cast<unsigned char>(c))
			: std::tolower(static_cast<unsigned char>(c)));
	}
	if (swapped_castling.empty()) swapped_castling = "-";

	std::string swapped_en_passant = en_passant;
	if (en_passant != "-" && en_passant.size() == 2) {
		swapped_en_passant[1] = static_cast<char>('1' + ('8' - en_passant[1]));
	}

	return mirrored + " " + (side == "w" ? "b" : "w") + " " + swapped_castling + " " +
	        swapped_en_passant + " " + (halfmove.empty() ? "0" : halfmove) + " " +
	        (fullmove.empty() ? "1" : fullmove);
}

// A spread of phases: opening, middlegame, queens off, pawn endgame, lone pawn.
const char* kPositions[] = {
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
	"r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1",
	"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
	"8/8/4k3/8/8/4K3/4P3/8 w - - 0 1",
	"4k3/8/8/8/8/8/6P1/4K3 w - - 0 1",
};

}  // namespace

// Equal, not negated: this evaluation is side-to-move-relative, so mirroring and
// swapping colours also swaps who is to move and the two sign flips cancel. The
// negation form is the test below, where the side moves and the board does not.
TEST_CASE("evaluate: a position and its colour-swapped mirror score the same", "[eval]") {
	for (const char* fen : kPositions) {
		Position pos = parse_fen(fen);
		Position mirrored = parse_fen(mirror_fen(fen));
		INFO("fen: " << fen << "\nmirrored: " << mirror_fen(fen));
		CHECK(eval::evaluate(pos) == eval::evaluate(mirrored));
	}
}

TEST_CASE("evaluate: a symmetric position is equal for both sides", "[eval]") {
	// Both start-position tests in one: the score is zero, and it is zero from
	// either side of the board.
	Position white_to_move = parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	Position black_to_move = parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");

	CHECK(eval::evaluate(white_to_move) == 0);
	CHECK(eval::evaluate(black_to_move) == 0);
}

TEST_CASE("evaluate: side to move perspective flips with side to move", "[eval]") {
	// Same material imbalance (White is up a queen), viewed from each side.
	Position white_to_move = parse_fen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
	Position black_to_move = parse_fen("4k3/8/8/8/8/8/8/4K2Q b - - 0 1");

	CHECK(eval::evaluate(white_to_move) > 0);
	CHECK(eval::evaluate(black_to_move) == -eval::evaluate(white_to_move));
}

TEST_CASE("evaluate: material dominates positional terms", "[eval]") {
	// A queen outweighs every piece-square bonus on the board put together, which
	// is the guard against a table typo three digits wide.
	Position queen_up = parse_fen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
	Position queen_and_bishop_down = parse_fen("4kb2/8/8/8/8/8/8/4K2Q w - - 0 1");

	CHECK(eval::evaluate(queen_up) > 800);
	CHECK(eval::evaluate(queen_up) - eval::evaluate(queen_and_bishop_down) > 250);
}

TEST_CASE("evaluate: piece-square tables reward the better square", "[eval]") {
	// Identical material, one knight central and one in the corner, so only the
	// tables can separate them.
	Position centre = parse_fen("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
	Position corner = parse_fen("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");

	CHECK(eval::evaluate(centre) > eval::evaluate(corner));
}

TEST_CASE("evaluate: the king's best square depends on the phase", "[eval]") {
	// The point of tapering, isolated: a single untapered table has to pick
	// between corner-is-safety and centre-is-activity and be wrong about one.
	Position midgame_corner = parse_fen("r2q1rk1/pppppppp/8/8/8/8/PPPPPPPP/R2Q1RK1 w - - 0 1");
	Position midgame_centre = parse_fen("r2q1rk1/pppppppp/8/8/3K4/8/PPPPPPPP/R2Q1R2 w - - 0 1");
	CHECK(eval::evaluate(midgame_corner) > eval::evaluate(midgame_centre));

	Position endgame_corner = parse_fen("6k1/pp4pp/8/8/8/8/PP4PP/6K1 w - - 0 1");
	Position endgame_centre = parse_fen("6k1/pp4pp/8/8/3K4/8/PP4PP/8 w - - 0 1");
	CHECK(eval::evaluate(endgame_centre) > eval::evaluate(endgame_corner));
}

TEST_CASE("evaluate: an advanced pawn is worth more in the endgame", "[eval]") {
	// The endgame table pays far more for the advance, which is how a search with
	// no passed-pawn term still pushes one.
	auto advance_gain = [](const char* back, const char* advanced) {
		return eval::evaluate(parse_fen(advanced)) - eval::evaluate(parse_fen(back));
	};

	// a5 -> a6, where the two tables' numbers diverge sharply (+94 against -6).
	int midgame_gain = advance_gain(
		"rnbqkbnr/pppppppp/8/P7/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1",
		"rnbqkbnr/pppppppp/P7/8/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
	int endgame_gain = advance_gain(
		"4k3/8/8/P7/8/8/8/4K3 w - - 0 1",
		"4k3/8/P7/8/8/8/8/4K3 w - - 0 1");

	CHECK(endgame_gain > midgame_gain);
}
