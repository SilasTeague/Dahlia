add_executable(dahlia_unit_tests
	test_placeholder.cpp
	test_position_fen.cpp
)
target_link_libraries(dahlia_unit_tests PRIVATE dahlia_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(dahlia_unit_tests)
