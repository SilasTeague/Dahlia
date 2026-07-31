add_executable(dahlia_search_bench
	bench_search.cpp
)
target_link_libraries(dahlia_search_bench PRIVATE dahlia_core benchmark::benchmark)
