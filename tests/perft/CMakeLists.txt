add_executable(dahlia_perft_tests
	perft.cpp
	perft_test.cpp
)
target_link_libraries(dahlia_perft_tests PRIVATE dahlia_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(dahlia_perft_tests)
