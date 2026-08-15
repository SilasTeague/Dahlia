add_executable(dahlia_microbench
	bench_placeholder.cpp
	bench_movegen.cpp
	bench_eval.cpp
)
target_link_libraries(dahlia_microbench PRIVATE dahlia_core benchmark::benchmark)
