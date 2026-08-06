#include <catch2/catch_test_macros.hpp>

#include "core/move.h"
#include "search/tt.h"

using search::Bound;
using search::TranspositionTable;
using search::TTEntry;

TEST_CASE("tt: store then probe round-trips on the same key", "[tt]") {
	TranspositionTable tt(1);
	tt.store(0x1234, Move{a1, a8}, 42, 5, Bound::Exact);

	TTEntry out;
	REQUIRE(tt.probe(0x1234, out));
	CHECK(out.best_move.from == a1);
	CHECK(out.best_move.to == a8);
	CHECK(out.score == 42);
	CHECK(out.depth == 5);
	CHECK(out.bound == Bound::Exact);
}

TEST_CASE("tt: probe misses on an unwritten key", "[tt]") {
	TranspositionTable tt(1);
	TTEntry out;
	CHECK_FALSE(tt.probe(0xDEAD, out));
}

TEST_CASE("tt: depth-preferred replacement keeps the deeper entry", "[tt]") {
	TranspositionTable tt(1);
	tt.store(0x1, Move{a1, a2}, 10, 8, Bound::Exact);
	tt.store(0x1, Move{b1, b2}, 20, 3, Bound::Exact);  // shallower same-key store: ignored

	TTEntry out;
	REQUIRE(tt.probe(0x1, out));
	CHECK(out.depth == 8);
	CHECK(out.best_move.from == a1);
}

TEST_CASE("tt: new_search lets a shallower store replace a stale entry", "[tt]") {
	TranspositionTable tt(1);
	tt.store(0x1, Move{a1, a2}, 10, 8, Bound::Exact);
	tt.new_search();
	tt.store(0x1, Move{b1, b2}, 20, 1, Bound::Exact);

	TTEntry out;
	REQUIRE(tt.probe(0x1, out));
	CHECK(out.depth == 1);
	CHECK(out.best_move.from == b1);
}

TEST_CASE("tt: clear removes all entries", "[tt]") {
	TranspositionTable tt(1);
	tt.store(0x1, Move{a1, a2}, 10, 8, Bound::Exact);
	tt.clear();

	TTEntry out;
	CHECK_FALSE(tt.probe(0x1, out));
	CHECK(tt.hashfull_permille() == 0);
}
