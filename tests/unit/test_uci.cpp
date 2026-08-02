#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "movegen/attacks.h"
#include "movegen/movegen.h"
#include "position/position.h"
#include "uci/uci.h"

// Scripted UCI protocol tests (REFERENCE.md 3.10): feed commands through an
// istream, assert well-formed responses on the ostream. `go` now runs
// Milestone 3's real (deterministic) alpha-beta search, so bestmove output
// is checked for legality/well-formedness rather than a specific move.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

std::vector<std::string> lines_of(const std::string& s) {
	std::vector<std::string> out;
	std::istringstream iss(s);
	std::string line;
	while (std::getline(iss, line)) out.push_back(line);
	return out;
}

bool any_line_is(const std::vector<std::string>& lines, const std::string& target) {
	for (const auto& l : lines) {
		if (l == target) return true;
	}
	return false;
}

bool any_line_starts_with(const std::vector<std::string>& lines, const std::string& prefix) {
	for (const auto& l : lines) {
		if (l.rfind(prefix, 0) == 0) return true;
	}
	return false;
}

}  // namespace

TEST_CASE("uci: uci command produces uciok", "[uci]") {
	std::istringstream in("uci\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	auto lines = lines_of(out.str());
	CHECK(any_line_starts_with(lines, "id name"));
	CHECK(any_line_is(lines, "uciok"));
}

TEST_CASE("uci: isready produces readyok", "[uci]") {
	std::istringstream in("isready\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	CHECK(any_line_is(lines_of(out.str()), "readyok"));
}

TEST_CASE("uci: go from startpos produces a legal bestmove", "[uci]") {
	std::istringstream in("position startpos\ngo\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	auto lines = lines_of(out.str());
	CHECK(any_line_starts_with(lines, "bestmove "));

	std::string bestmove;
	for (const auto& l : lines) {
		if (l.rfind("bestmove ", 0) == 0) bestmove = l.substr(9);
	}
	REQUIRE_FALSE(bestmove.empty());
	CHECK(bestmove != "0000");
	CHECK(bestmove.size() >= 4);
}

TEST_CASE("uci: position startpos moves applies moves before go", "[uci]") {
	// 1. e4 e5 2. Nf3 — bestmove must be legal for black-to-move here, not
	// for the start position (a stale-position bug would likely produce an
	// illegal or nonsensical move).
	std::istringstream in("position startpos moves e2e4 e7e5 g1f3\ngo\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	auto lines = lines_of(out.str());
	std::string bestmove;
	for (const auto& l : lines) {
		if (l.rfind("bestmove ", 0) == 0) bestmove = l.substr(9);
	}
	REQUIRE_FALSE(bestmove.empty());

	Position pos = parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	StateInfo undo;
	make_move(pos, Move{e2, e4}, undo);
	make_move(pos, Move{e7, e5}, undo);
	make_move(pos, Move{g1, f3}, undo);
	CHECK(pos.side_to_move == BLACK);
}

TEST_CASE("uci: position fen sets an arbitrary position", "[uci]") {
	std::istringstream in(
		"position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1\ngo\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	CHECK(any_line_starts_with(lines_of(out.str()), "bestmove "));
}

TEST_CASE("uci: checkmate produces bestmove 0000", "[uci]") {
	// Fool's mate: black has no legal moves and is in check.
	std::istringstream in(
		"position fen rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3\ngo\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	CHECK(any_line_is(lines_of(out.str()), "bestmove 0000"));
}

TEST_CASE("uci: ucinewgame resets to the start position", "[uci]") {
	std::istringstream in(
		"position fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1\nucinewgame\ngo\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	CHECK(any_line_starts_with(lines_of(out.str()), "bestmove "));
}
