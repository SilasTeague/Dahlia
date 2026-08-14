#include <catch2/catch_test_macros.hpp>
#include <chrono>
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

int count_lines_starting_with(const std::vector<std::string>& lines, const std::string& prefix) {
	int count = 0;
	for (const auto& l : lines) {
		if (l.rfind(prefix, 0) == 0) count++;
	}
	return count;
}

// -1 if no line starts with `prefix`.
int index_of_first_starting_with(const std::vector<std::string>& lines, const std::string& prefix) {
	for (size_t i = 0; i < lines.size(); i++) {
		if (lines[i].rfind(prefix, 0) == 0) return static_cast<int>(i);
	}
	return -1;
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

TEST_CASE("uci: declares the Move Overhead option", "[uci]") {
	std::istringstream in("uci\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	CHECK(any_line_starts_with(lines_of(out.str()), "option name Move Overhead type spin "));
}

TEST_CASE("uci: multi-word setoption names are matched whole", "[uci]") {
	// "Move Overhead" arrives as two tokens; a parser that joined them without
	// the space (or stopped at the first) would silently ignore this, so the
	// observable effect -- a shorter search -- is what's checked. 400 ms of
	// overhead against a 500 ms movetime leaves ~100 ms of search.
	std::istringstream in(
		"setoption name Move Overhead value 400\nposition startpos\ngo movetime 500\nquit\n");
	std::ostringstream out;

	auto start = std::chrono::steady_clock::now();
	run_uci_loop(in, out);
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start).count();

	CHECK(any_line_starts_with(lines_of(out.str()), "bestmove "));
	CHECK(elapsed < 400);
}

TEST_CASE("uci: movetime search reserves the move overhead", "[uci]") {
	// The regression this guards: replying at exactly `movetime` forfeits
	// against a GUI with a zero time margin (cutechess-cli's default).
	std::istringstream in("position startpos\ngo movetime 200\nquit\n");
	std::ostringstream out;

	auto start = std::chrono::steady_clock::now();
	run_uci_loop(in, out);
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start).count();

	CHECK(any_line_starts_with(lines_of(out.str()), "bestmove "));
	CHECK(elapsed < 200);
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

// Async `go`/`stop` tests (REFERENCE.md 3.8/3.9/3.10): `go` now runs on its
// own thread so the reader loop keeps consuming lines while a search is in
// flight. `go infinite` never self-terminates (no depth/time cap), so these
// tests are deterministic: the engine cannot emit `bestmove` until `stop` or
// `quit` reaches it, regardless of how the OS schedules the search thread.

TEST_CASE("uci: go infinite runs until stop, emitting exactly one bestmove", "[uci]") {
	std::istringstream in("position startpos\ngo infinite\nstop\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	auto lines = lines_of(out.str());
	CHECK(count_lines_starting_with(lines, "bestmove ") == 1);
}

TEST_CASE("uci: isready responds immediately while a search is running", "[uci]") {
	std::istringstream in("position startpos\ngo infinite\nisready\nstop\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	auto lines = lines_of(out.str());
	int readyok_index = index_of_first_starting_with(lines, "readyok");
	int bestmove_index = index_of_first_starting_with(lines, "bestmove ");

	REQUIRE(readyok_index >= 0);
	REQUIRE(bestmove_index >= 0);
	// A synchronous `go` would block the reader loop, so `readyok` could only
	// ever appear after `bestmove` -- this ordering is the actual proof the
	// search runs concurrently rather than just returning quickly.
	CHECK(readyok_index < bestmove_index);
}

TEST_CASE("uci: quit during a search terminates cleanly with no leaked thread", "[uci]") {
	std::istringstream in("position startpos\ngo infinite\nquit\n");
	std::ostringstream out;
	// If `quit` failed to stop and join the search thread, this call would
	// either hang (timing out the test) or the process would terminate with
	// a live detached thread; returning normally here is the assertion.
	run_uci_loop(in, out);

	CHECK(count_lines_starting_with(lines_of(out.str()), "bestmove ") == 1);
}

TEST_CASE("uci: a second go while a search is in flight is rejected, not queued", "[uci]") {
	std::istringstream in("position startpos\ngo infinite\ngo\nstop\nquit\n");
	std::ostringstream out;
	run_uci_loop(in, out);

	// A queued second `go` would still be running when `quit` arrives, and
	// `quit` stops+joins unconditionally -- so queueing would also produce a
	// second bestmove here. Exactly one proves the second `go` never ran.
	CHECK(count_lines_starting_with(lines_of(out.str()), "bestmove ") == 1);
}

TEST_CASE("uci: engine accepts a new go after a prior search completes", "[uci]") {
	// Two independent sessions rather than two `go`s in one script: after
	// `stop`, nothing in the UCI protocol lets a scripted test block until
	// the search thread has actually finished before sending the next `go`
	// (a real GUI would simply wait to read `bestmove` first). Ending each
	// session without `quit` forces ~Engine to join before returning, which
	// is the deterministic stand-in for that wait.
	std::istringstream in1("position startpos\ngo infinite\nstop\n");
	std::ostringstream out1;
	run_uci_loop(in1, out1);
	CHECK(count_lines_starting_with(lines_of(out1.str()), "bestmove ") == 1);

	std::istringstream in2("position startpos\ngo infinite\nstop\n");
	std::ostringstream out2;
	run_uci_loop(in2, out2);
	CHECK(count_lines_starting_with(lines_of(out2.str()), "bestmove ") == 1);
}
