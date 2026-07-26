add_executable(dahlia_microbench
	bench_placeholder.cpp
)
target_link_libraries(dahlia_microbench PRIVATE dahlia_core benchmark::benchmark)
