# 0002 — CMake as the build system of record; Makefile kept as a thin wrapper

## Context

Dahlia started with a flat `src/*.cpp` Makefile: no test target, no benchmark
target, no sanitizer/warning presets, and no way to build a library target
shared between the engine binary, unit tests, and benchmarks without
duplicating flags. REFERENCE.md 2.1 always intended CMake as the target build
system, once tests/benchmarks/multi-compiler CI existed to justify it — that
point is now (Milestone 0).

## Decision

CMake (>= 3.20) is the build system of record: `dahlia_core` library target
+ `dahlia` executable + `tests/unit` + `bench/microbench`, driven via
`CMakePresets.json` (`debug`, `debug-asan`, `release`, `release-native`).

The Makefile is kept, but reduced to a thin wrapper delegating to
`cmake --preset`/`ctest --preset` — useful purely as muscle-memory (`make`,
`make test`, `make run`) for local iteration. It carries no build logic of
its own and should never drift from what the CMake presets do.

## Consequences

- Adding a new source file, warning, or dependency means editing
  `CMakeLists.txt`, not the Makefile.
- CI (`ci.yml`) invokes `cmake`/`ctest` directly, not `make`, so the Makefile
  wrapper being thin/stale cannot break CI.
- If the Makefile wrapper ever becomes a source of confusion rather than
  convenience, it should be deleted outright rather than patched further —
  see REFERENCE.md 2.1's original framing of this as an explicit either/or.
