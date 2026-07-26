#include <catch2/catch_test_macros.hpp>

#include "core/types.h"

// Milestone 0 placeholder: locks in the CMake/CTest/Catch2 wiring before
// real per-module tests exist. Replace with core/movegen/position tests
// as those subsystems land.
TEST_CASE("square enum values are stable", "[core]") {
	REQUIRE(a1 == 0);
	REQUIRE(h8 == 63);
}
