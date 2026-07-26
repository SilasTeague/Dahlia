add_executable(dahlia_microbench
	bench_placeholder.cpp
	bench_movegen.cpp
)
target_link_libraries(dahlia_microbench PRIVATE dahlia_core benchmark::benchmark)
