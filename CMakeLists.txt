cmake_minimum_required(VERSION 3.20)
project(Dahlia CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DAHLIA_BUILD_TESTS "Build the Catch2 unit test suite" ON)
option(DAHLIA_BUILD_BENCHMARKS "Build the Google Benchmark microbenchmarks" ON)
option(DAHLIA_ENABLE_SANITIZERS "Build with ASan+UBSan" OFF)
option(DAHLIA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

add_library(dahlia_core
	src/movegen/attacks.cpp
	src/movegen/movegen.cpp
	src/position/position.cpp
)
target_include_directories(dahlia_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
dahlia_set_warnings(dahlia_core)
dahlia_enable_sanitizers(dahlia_core)

add_executable(dahlia src/main.cpp)
target_link_libraries(dahlia PRIVATE dahlia_core)
dahlia_set_warnings(dahlia)
dahlia_enable_sanitizers(dahlia)

if (DAHLIA_BUILD_TESTS OR DAHLIA_BUILD_BENCHMARKS)
	include(cmake/FetchDeps.cmake)
endif()

if (DAHLIA_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests/unit)
endif()

if (DAHLIA_BUILD_BENCHMARKS)
	add_subdirectory(bench/microbench)
endif()
