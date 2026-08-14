cmake_minimum_required(VERSION 3.20)
project(Dahlia CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DAHLIA_BUILD_TESTS "Build the Catch2 unit test suite" ON)
option(DAHLIA_BUILD_BENCHMARKS "Build the Google Benchmark microbenchmarks and search_bench" ON)
option(DAHLIA_ENABLE_SANITIZERS "Build with ASan+UBSan" OFF)
option(DAHLIA_ENABLE_TSAN "Build with ThreadSanitizer" OFF)
option(DAHLIA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if (DAHLIA_ENABLE_SANITIZERS AND DAHLIA_ENABLE_TSAN)
	message(FATAL_ERROR "DAHLIA_ENABLE_SANITIZERS (ASan+UBSan) and DAHLIA_ENABLE_TSAN are mutually exclusive")
endif()

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(dahlia_core
	src/eval/evaluate.cpp
	src/movegen/attacks.cpp
	src/movegen/movegen.cpp
	src/position/history.cpp
	src/position/position.cpp
	src/position/zobrist.cpp
	src/search/clock.cpp
	src/search/ordering.cpp
	src/search/search.cpp
	src/search/tt.cpp
	src/uci/uci.cpp
)
target_include_directories(dahlia_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(dahlia_core PUBLIC Threads::Threads)
dahlia_set_warnings(dahlia_core)
dahlia_enable_sanitizers(dahlia_core)
dahlia_enable_tsan(dahlia_core)

add_executable(dahlia src/main.cpp)
target_link_libraries(dahlia PRIVATE dahlia_core)
dahlia_set_warnings(dahlia)
dahlia_enable_sanitizers(dahlia)
dahlia_enable_tsan(dahlia)

if (DAHLIA_BUILD_TESTS OR DAHLIA_BUILD_BENCHMARKS)
	include(cmake/FetchDeps.cmake)
endif()

if (DAHLIA_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests/unit)
	add_subdirectory(tests/perft)
endif()

if (DAHLIA_BUILD_BENCHMARKS)
	add_subdirectory(bench/microbench)
	add_subdirectory(bench/search_bench)
endif()
