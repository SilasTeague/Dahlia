#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "eval/evaluate.h"
#include "position/position.h"

// Material + tapered piece-square tables (REFERENCE.md 3.6, v1; Milestone 5).
//
// The tests that matter here are the invariants, not the magic numbers. An
// evaluation is a pile of hand-tuned constants: asserting that a particular
// position scores exactly +47 pins the constants rather than the code, and
// breaks on every future tuning pass for no benefit. What must hold no matter
// what the constants are is that the function is antisymmetric (3.6's "sign
// errors relative to side-to-move... a shockingly common and shockingly
// hard-to-notice bug class"), that Black's tables are the mirror of White's,
// and that the phase blend moves in the right direction.

namespace {

// Mirrors a FEN across the middle of the board and swaps the colours: rank 8
// becomes rank 1, White's pieces become Black's, and castling rights, the side
// to move, and the en passant square all follow.
//
// This is the transformation the symmetry test needs and the one a PST bug
// survives: an engine that flips White's table but not Black's still scores
// the start position at zero, still flips sign with the side to move, and
// still passes every material test -- because those all compare a position to
// itself. Only comparing a position to its colour-swapped mirror separates
// "the evaluation is symmetric" from "the evaluation is symmetric about the
// one position I tested".
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

// A spread of phases and structures: opening, dense middlegame, a position
// with the queens off, a pawn endgame, and one with a lone advanced pawn.
const char* kPositions[] = {
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
	"r1bqrbn1/pp3ppp/2np4/2p5/2B1P3/2N2N2/PPP2PPP/R1BQR1K1 w - - 0 1",
	"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
	"8/8/4k3/8/8/4K3/4P3/8 w - - 0 1",
	"4k3/8/8/8/8/8/6P1/4K3 w - - 0 1",
};

}  // namespace

// Equal, not negated. REFERENCE.md 3.6 states this invariant as
// `evaluate(pos) == -evaluate(pos.mirrored())`, which is the right form for a
// White-relative evaluation; this one is side-to-move-relative, and mirroring
// the board *and* swapping the colours also swaps who is to move. The result
// is the same position seen from the other chair, so the same side-relative
// score comes back with the same sign. The negation form of the invariant is
// the test below this one, where the side to move changes and the board does
// not.
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
	// A queen is worth more than every piece-square bonus on the board put
	// together, and no arrangement of the tables should be able to say
	// otherwise. This is the guard against a table typo three digits wide.
	Position queen_up = parse_fen("4k3/8/8/8/8/8/8/4K2Q w - - 0 1");
	Position queen_and_bishop_down = parse_fen("4kb2/8/8/8/8/8/8/4K2Q w - - 0 1");

	CHECK(eval::evaluate(queen_up) > 800);
	CHECK(eval::evaluate(queen_up) - eval::evaluate(queen_and_bishop_down) > 250);
}

TEST_CASE("evaluate: piece-square tables reward the better square", "[eval]") {
	// Identical material, one knight placed centrally and one in the corner.
	// The tables are what separate them; a material-only evaluation scores
	// these the same.
	Position centre = parse_fen("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
	Position corner = parse_fen("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");

	CHECK(eval::evaluate(centre) > eval::evaluate(corner));
}

TEST_CASE("evaluate: the king's best square depends on the phase", "[eval]") {
	// The point of tapering, isolated. In each pair the only difference is
	// where White's king stands: tucked in the corner or standing in the
	// centre.
	//
	// With queens and rooks on, the corner is safety and the centre is a
	// target. With nothing left but pawns, the centre is where the king is
	// needed and the corner is a wasted piece. A single untapered table has to
	// pick one of those and be wrong about the other; two tables blended by
	// phase are right about both, and that difference is this whole feature.
	Position midgame_corner = parse_fen("r2q1rk1/pppppppp/8/8/8/8/PPPPPPPP/R2Q1RK1 w - - 0 1");
	Position midgame_centre = parse_fen("r2q1rk1/pppppppp/8/8/3K4/8/PPPPPPPP/R2Q1R2 w - - 0 1");
	CHECK(eval::evaluate(midgame_corner) > eval::evaluate(midgame_centre));

	Position endgame_corner = parse_fen("6k1/pp4pp/8/8/8/8/PP4PP/6K1 w - - 0 1");
	Position endgame_centre = parse_fen("6k1/pp4pp/8/8/3K4/8/PP4PP/8 w - - 0 1");
	CHECK(eval::evaluate(endgame_centre) > eval::evaluate(endgame_corner));
}

TEST_CASE("evaluate: an advanced pawn is worth more in the endgame", "[eval]") {
	// The same pawn on the same square, once with a full board and once with
	// only kings left, measured against the same pawn on its starting square.
	// The endgame table pays far more for the advance, which is how a search
	// with no passed-pawn term still pushes one.
	auto advance_gain = [](const char* back, const char* advanced) {
		return eval::evaluate(parse_fen(advanced)) - eval::evaluate(parse_fen(back));
	};

	// a5 → a6, one square from the rank where the endgame table's numbers run
	// away from the midgame table's (+94 against -6).
	int midgame_gain = advance_gain(
		"rnbqkbnr/pppppppp/8/P7/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1",
		"rnbqkbnr/pppppppp/P7/8/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
	int endgame_gain = advance_gain(
		"4k3/8/8/P7/8/8/8/4K3 w - - 0 1",
		"4k3/8/P7/8/8/8/8/4K3 w - - 0 1");

	CHECK(endgame_gain > midgame_gain);
}
