add_executable(dahlia_unit_tests
	test_placeholder.cpp
	test_position_fen.cpp
	test_make_unmake.cpp
	test_uci.cpp
	test_eval.cpp
	test_search.cpp
	test_search_nodes.cpp
	test_ordering.cpp
	test_quiescence.cpp
	test_nullmove.cpp
	test_tactics.cpp
	test_repetition.cpp
	test_tt.cpp
	test_clock.cpp
)
target_link_libraries(dahlia_unit_tests PRIVATE dahlia_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(dahlia_unit_tests)
