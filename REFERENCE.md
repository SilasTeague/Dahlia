# Dahlia Engineering Reference

**Status:** Living document. Consult before implementing any major subsystem; update when a decision changes.
**Scope:** This document is the single source of truth for architecture, conventions, testing, and benchmarking on Dahlia. Design discussions in chat/PRs should end with an update here, not live only in conversation history.

---

## 0. How to Use This Document

- Before starting a subsystem, read its section under [Part III — Subsystem Specifications](#part-iii--subsystem-specifications).
- Before opening a PR, check [Part I — Conventions](#part-i--project-conventions) for style, commit, and branch rules.
- Before claiming an optimization "worked," check [Part IV — Metrics Catalog](#part-iv--metrics-catalog--benchmarking) for how it must be measured.
- When a decision is made that isn't captured here, add it — either inline in the relevant section, or as a dated entry in [Appendix C — Architecture Decision Log](#appendix-c--architecture-decision-log).
- This document describes the **target** state. The repository may lag behind it during migration; the [Roadmap](#part-v--staged-roadmap) says what should be true at each milestone.

---

## 1. Project Vision

Dahlia is a UCI-compatible chess engine written in modern C++, built from scratch with two co-equal goals:

1. **Chess engine quality.** Correct, reasonably strong, and eventually competitive move generation, search, and evaluation.
2. **Portfolio quality.** A demonstration of professional systems-engineering practice: clean architecture, rigorous testing, real benchmarking, CI discipline, and documentation good enough that a stranger (recruiter, engineer, collaborator) can understand the design without asking questions.

**Neither goal is allowed to silently override the other.** When they conflict, the tie-break rule is:

> Prefer the option that best balances performance, maintainability, educational value, engineering quality, and portfolio impact — in that order only when a clear winner exists on the first four; otherwise favor the more instructive, better-documented option.

In practice this means: hand-rolled magic bitboards over a pulled-in library (educational value + control), a real benchmark harness before micro-optimizing (measurable engineering), and comments/docs that explain *why* a nonstandard chess-programming trick works, not just that it does.

This document exists so future sessions (with Claude or otherwise) do not need the rationale re-explained. **Do not write implementation code before checking whether this document already made the relevant decision.**

---

# Part I — Project Conventions

## 1.1 Directory Structure

Target layout (current repo is flat `src/`; migrating to this structure is Milestone 0 work — see [Roadmap](#milestone-0-project-scaffolding)):

```
Dahlia/
├── CMakeLists.txt
├── REFERENCE.md        # this document
├── README.md                  # project pitch, build instructions, quick demo
├── LICENSE                     # MIT
├── .clang-format
├── .clang-tidy
├── .github/
│   └── workflows/
│       ├── ci.yml              # build + test matrix on every push/PR
│       └── benchmark.yml       # scheduled/on-demand perf regression run
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   └── FetchDeps.cmake         # Catch2, benchmark, etc.
├── src/                         # .h/.cpp colocated per module
│   ├── core/                    (types, bitboard, square, move)
│   ├── movegen/                 (attacks, magics, generator)
│   ├── position/                (Position, Zobrist, make/unmake)
│   ├── search/                  (search, tt, ordering, timeman)
│   ├── eval/
│   ├── uci/
│   ├── util/                    (logging, cli, string utils)
│   └── main.cpp
├── tests/
│   ├── unit/                    # Catch2 unit tests, one file per module
│   ├── perft/                   # perft correctness suite + known positions
│   └── data/                    # EPD/FEN test suites, checked in
├── bench/
│   ├── microbench/               # Google Benchmark: movegen, magics, TT, eval
│   ├── search_bench/              # fixed-position search node-count/time bench
│   └── results/                  # historical benchmark JSON (tracked for regression)
├── tools/
│   ├── perftree/                  # perft divide / debugging helper script
│   ├── sprt/                      # SPRT match runner wrapper (cutechess-cli)
│   └── tuner/                     # (future) evaluation tuning harness
├── docs/
│   ├── architecture.md            # diagrams + prose overview, generated/maintained
│   ├── uci.md                     # UCI options and behavior specific to Dahlia
│   └── adr/                       # architecture decision records, one file each
└── scripts/
    ├── format.sh
    └── run_benchmarks.sh
```

**Module dependency rule:** dependencies point one direction only:

```
util  ←  core  ←  movegen  ←  position  ←  search  ←  uci
                                   ↑
                                 eval
```

- `util` depends on nothing else in-project.
- `core` (types, bitboards, squares) depends only on `util`.
- `movegen` depends on `core` only — no knowledge of `Position`, search, or eval.
- `position` depends on `core` and `movegen` (to apply moves) but not on `search`/`eval`.
- `eval` depends on `position`/`core` only, never on `search`.
- `search` depends on `position`, `movegen`, `eval`.
- `uci` depends on everything below it; nothing depends on `uci`.

A CI lint step (or `clang-tidy` include check / simple script) should enforce that headers never `#include` "upward" against this graph. This is worth having explicitly because chess engines are notorious for turning into one giant mutually-recursive blob — the whole point of the module split is to keep `movegen` and `eval` unit-testable in isolation.

## 1.2 Naming Conventions

| Element | Convention | Example |
|---|---|---|
| Types (classes, structs, enums) | `PascalCase` | `Position`, `TranspositionTable`, `Move` |
| Enum values | `PascalCase` (via `enum class`) | `Piece::Knight`, `Color::White` |
| Functions, methods | `snake_case` | `generate_legal_moves()`, `make_move()` |
| Local variables, parameters | `snake_case` | `from_square`, `attacker_mask` |
| Member variables | `snake_case` with trailing underscore | `side_to_move_`, `zobrist_key_` |
| Constants / constexpr | `kPascalCase` | `kMaxPly`, `kInfiniteScore` |
| Macros (avoid; if unavoidable) | `SCREAMING_SNAKE_CASE` | `DAHLIA_ASSERT` |
| Namespaces | lowercase, short | `dahlia::movegen` |
| Files | `snake_case`, mirror primary type | `transposition_table.h/.cpp` |
| Template parameters | `PascalCase` or single capital | `T`, `Color`, `MoveGenType` |

**Note on current code:** existing headers (`types.h`, `position.h`) use plain `enum` with explicit `u8` underlying type and non-prefixed member names (`side_to_move`, no trailing underscore). The convention above is the *target* for the rewrite. Rationale for changing:

- `enum class` prevents implicit int conversions that have caused real bugs in other engines (e.g., `Piece + 1` silently compiling into nonsense). Where the raw integer value is genuinely needed (array indexing), use `std::to_underlying` (C++23) or an explicit `static_cast`, wrapped in a small helper — this is intentionally a little more typing, as a guardrail.
- Trailing underscore on members is a widely recognized convention (Google C++ Style Guide et al.) that instantly disambiguates member access from locals/params in constructors and setters, which matters a lot in a codebase full of `make_move`/`unmake_move` pairs.

## 1.3 Modern C++ Guidelines

Target standard: **C++20** (already in use). Use C++20 features deliberately, not for novelty:

- `<bit>` (`std::popcount`, `std::countr_zero`) instead of compiler builtins directly — portable, and `constexpr`-friendly. Wrap in `core/bitboard.h` so call sites never touch builtins directly.
- `concepts` for template constraints where they improve error messages (e.g., constraining move-generation templates over `Color`).
- `constexpr`/`consteval` for compile-time table generation (e.g., precomputed knight/king attack tables) — a build with zero runtime initialization cost for constant tables is both faster and directly benchmarkable ("startup time").
- `std::span` for passing move buffers instead of raw pointer + length.
- Avoid exceptions in hot paths (search/movegen). Use exceptions only for truly exceptional, non-performance-critical paths (e.g., malformed UCI input, config file errors). Search and movegen signal errors via return codes/asserts, never throw.
- No raw `new`/`delete`. Use value types and `std::vector`/`std::array`/`std::unique_ptr` — engine state should be stack- or arena-allocated wherever possible for cache locality.
- No `using namespace std;` in headers. `using namespace` is acceptable inside a narrow `.cpp` function body only if it meaningfully reduces noise.
- Header hygiene: `#pragma once` (already the project's convention), forward-declare where possible to cut compile times.
- Prefer `enum class` everywhere (see 1.2) except for bit-flag types like `CastlingRights`, where a plain `enum` with explicit bitwise operators (or a small `Flags<T>` wrapper) is acceptable and arguably clearer.
- Format via `clang-format` (LLVM-derived style, tabs vs spaces to match current code's tab indentation — pin this in `.clang-format` so it's not a matter of taste per-PR).
- Static analysis via `clang-tidy` in CI (warnings-as-errors for a curated, deliberately-chosen subset of checks — not the full default set, which is noisy for chess-engine bit-twiddling code).

## 1.4 Documentation Standards

- **Every public header** gets a file-level comment: one paragraph on what the module is responsible for and, critically, what it is *not* responsible for (ties back to the dependency graph in 1.1).
- **Every public class/function** gets a Doxygen-style comment only when the *why* isn't obvious from the signature and name. Do not restate the signature in prose (`/// Returns the square. @return the square` is banned). Document:
  - Non-obvious invariants (e.g., "assumes `from` and `to` differ; UB otherwise").
  - Complexity/performance characteristics when they matter to the caller (e.g., "O(1) via magic bitboard lookup, ~a single cache miss").
  - Ownership/lifetime rules for anything non-value-type.
- **No comments restating code.** This applies project-wide, matching general engineering practice: comment the non-obvious constraint, workaround, or invariant — not the mechanics.
- `docs/architecture.md` holds a living high-level diagram (can be ASCII/mermaid) of the module graph and data flow (UCI loop → search → movegen/eval → position mutation). Regenerate/update it whenever the module graph changes — treat a stale architecture doc as a bug.
- **Architecture Decision Records** (`docs/adr/NNNN-title.md`) for any decision that was genuinely contested (e.g., "why 0x88 was rejected in favor of bitboards," "why Zobrist over incremental hash-by-piece-list"). Short format: Context / Decision / Consequences. This is one of the most recruiter-legible artifacts in the whole repo — it shows engineering judgment, not just output.
- `README.md` is the front door: what Dahlia is, current Elo/strength estimate if known, how to build, how to run a UCI session, a link into this reference doc and into `docs/architecture.md`.

## 1.5 Git Conventions

**Branching:**
- `master` is always buildable, always passes CI, and should always be able to play a legal game (even if weak). Never force-push to `master`.
- Feature branches: `feature/<short-topic>` (e.g., `feature/magic-bitboards`).
- Fix branches: `fix/<short-topic>`.
- Experimental/spike branches that may be thrown away: `spike/<short-topic>`.
- Rebase feature branches on `master` before merging; prefer a clean, readable history over preserving exact chronology, but never rewrite history already pushed to a shared branch other people (or other sessions) are using without saying so.

**Commits:**
- Conventional, present-tense, imperative subject line: `feat: add magic bitboard sliding attack generation`, `fix: correct en passant square reset on double pawn push`, `test: add perft suite for Kiwipete position`, `perf: replace vector with fixed array in move list`, `docs: add ADR for TT replacement scheme`.
- Prefixes: `feat`, `fix`, `perf`, `test`, `bench`, `docs`, `refactor`, `build`, `ci`, `chore`.
- Body explains *why*, not what (the diff already shows what). Reference the benchmark/test result that justifies a `perf:` commit when applicable (e.g., "perft(6) time: 4.2s → 2.9s, see bench/results/…").
- Squash-merge trivial/noisy branches; regular merge (or rebase) when individual commits in the branch are independently meaningful (e.g., a milestone's worth of distinguishable steps) — reviewer/recruiter value of readable history outweighs strict linearity here.

**Pull Requests — Decision (2026-07-25): every change, including solo-dev work, goes through a feature branch + PR, never a direct commit to `main`.** This is a deliberate choice over trunk-based direct-to-main commits: the PR history itself is a portfolio artifact — each PR's description carries the design rationale and before/after benchmark numbers that a recruiter or collaborator can read without digging through commit-by-commit diffs.
- PR description states: what changed, why, what was tested, what was benchmarked (with before/after numbers when performance-relevant).
- CI must pass (build matrix + tests + perft suite) before merge.
- Every subsystem PR should reference which section of this document it implements/updates, and update this document in the same PR if the design evolved during implementation.
- Merge every PR — no exceptions for "trivial" ones — since the PR is what carries the rationale/benchmarks going forward, not the raw commit.

## 1.6 Testing Philosophy

Chess engines fail in ways that are easy to miss (illegal move generated once in a billion nodes, hash collision losing a game) and easy to catch cheaply (perft is a near-perfect movegen oracle). Testing strategy:

- **Unit tests** (Catch2): pure functions and small components in isolation — bit manipulation helpers, magic index computation, Zobrist incremental update matches from-scratch computation, TT probe/store logic, move encoding/decoding round-trips.
- **Perft tests**: the backbone of movegen correctness. Test against well-known perft values (starting position, Kiwipete, "position 3", "position 4", "position 5", "position 6" from the chess programming wiki) up to a depth that completes in CI time budget (a few seconds), with a slower/deeper variant runnable manually or nightly.
- **Perft divide** tooling (`tools/perftree`) for debugging: when a perft count is wrong, bisect down to the exact move/subtree responsible rather than staring at movegen code.
- **Search regression tests**: fixed positions with known correct best move or mate-in-N, run at fixed depth/time, asserting the engine finds it. Guards against search bugs (e.g., a broken pruning condition that skips a forced mate).
- **UCI protocol tests**: feed scripted UCI command sequences, assert well-formed responses (`isready` → `readyok`, `go` produces a legal `bestmove`, etc.).
- **Property-style tests** where cheap: e.g., "make_move followed by unmake_move restores the exact prior position (including hash key)" — this single property test catches an enormous fraction of make/unmake bugs.
- Coverage is tracked (`gcov`/`llvm-cov` in CI) as a *signal*, not a target to game — 100% coverage of movegen with no perft test is worth less than 70% coverage with a full perft suite. Report coverage in CI but do not gate merges purely on a coverage percentage.
- Every bug fixed gets a regression test in the same PR, named after what it guards against, not the ticket number.

## 1.7 Benchmarking Philosophy

Benchmarking is architecture, not an afterthought bolted on when something feels slow. Concretely:

- **Google Benchmark** (`bench/microbench`) for microbenchmarks: magic lookup latency, move generation for a batch of representative positions, make/unmake cost, TT probe/store cost, evaluation cost.
- **Search benchmark suite** (`bench/search_bench`): a fixed set of representative positions (opening/middlegame/endgame mix) run to a fixed node budget or fixed time, recording nodes-per-second, depth reached, and (once comparable) node count to reach a given depth — the last one is what actually tells you a pruning/ordering change helped, independent of machine speed.
- **Every benchmark result that matters is committed** as JSON under `bench/results/`, one file per run, named with date + git short-hash, so history is diffable and plottable later (a simple script or notebook can turn this into a chart for the portfolio narrative — "nodes/sec over the project's lifetime" is a great README graphic). See [3.11](#311-benchmark--regression-framework-bench) for the full history/schema design.
- **Before/after discipline**: any PR claiming a performance improvement includes the benchmark numbers before and after in the PR description, on the same machine/build flags, ideally with the sanitizer/debug build excluded (benchmark only `Release`/`RelWithDebInfo`).
- **No micro-optimization without a benchmark proving the bottleneck.** Profile first (`perf`/Instruments/`valgrind --tool=callgrind`), then optimize the measured hot path, then re-benchmark to confirm the win and check for regressions elsewhere.
- CI runs functional tests on every push; a separate workflow (`benchmark.yml`) runs the benchmark suite and can fail/flag if nodes/sec regresses beyond a tolerance band versus the last committed baseline. See 3.11 for exactly when this workflow fires.

**Decision (2026-07-26): build in strict stages — Correctness → Measurement → Optimization, never the reverse.** A basic, unoptimized-but-correct implementation of a subsystem should exist and pass perft/unit tests *before* the benchmark harness for that subsystem is built out, and the benchmark harness must exist and have a recorded baseline *before* any optimization work on that subsystem begins. Optimizing first and measuring after produces "I heard X is faster" engineering; measuring first produces "commit `abc123` reduced move generation from 8.1ns to 2.9ns and increased NPS 31%" engineering — the latter is the entire point of [Part IV](#part-iv--metrics-catalog--benchmarking). Concretely this means: it is acceptable, even expected, for early milestones (see [Roadmap](#part-v--staged-roadmap)) to ship deliberately non-optimal implementations (e.g., simple mailbox-style sliding attacks before magic bitboards) as long as they are correct and perft-clean — the harness in 3.11 is what turns the later swap into a measured, citable win rather than a vibes-based one.

---

# Part II — Toolchain & Infrastructure

## 2.1 Build System

Migrate from the current Makefile to **CMake** (target: CMake ≥ 3.20), because:
- Enables clean separation of library target (`dahlia_core` — everything except `main.cpp`/UCI loop) vs. executable (`dahlia`) vs. test/bench targets, so tests link against the exact same code the engine ships.
- First-class support for sanitizers, `FetchContent` (Catch2, Google Benchmark), install targets, and cross-platform builds — all of which matter both for correctness (multiple compilers catch different bugs) and for the portfolio story (a project that "just builds" with `cmake --preset` reads as more professional than a bespoke Makefile).

Planned structure:
```cmake
# top-level CMakeLists.txt (sketch, not final)
project(Dahlia CXX)
set(CMAKE_CXX_STANDARD 20)
add_library(dahlia_core ...)      # src/core, movegen, position, search, eval, util
add_executable(dahlia src/main.cpp)
target_link_libraries(dahlia PRIVATE dahlia_core)

option(DAHLIA_BUILD_TESTS "" ON)
option(DAHLIA_BUILD_BENCHMARKS "" ON)
option(DAHLIA_ENABLE_SANITIZERS "" OFF)
```

Provide **CMake presets** (`CMakePresets.json`) for: `debug` (assertions on, sanitizers optional), `release` (`-O3`, LTO, `-march=native` behind an explicit opt-in flag since default release builds must stay portable for CI/other machines), `release-native` (tuned for the dev machine, used for local benchmarking only).

Keep the existing `Makefile` as a thin convenience wrapper (`make` → invokes CMake) during migration if desired, or replace it outright once CMake is proven — decide and record the decision as an ADR rather than keeping both indefinitely.

## 2.2 Compiler & Warnings

- Support and CI-test at least **two compilers** (e.g., GCC and Clang) — catching UB/warnings that only one compiler flags is a real, demonstrable correctness practice, and is cheap in GitHub Actions.
- Warning flags (from `cmake/CompilerWarnings.cmake`): `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion`. Treat warnings as errors (`-Werror`) in CI builds; keep it off by default for local iterative debug builds if it becomes friction, but never merge with warnings present.
- Sanitizers: ASan+UBSan build variant in CI running the unit + perft(reduced depth) suite; TSan is lower priority until/unless the engine gains real multithreading (Milestone: SMP search).

## 2.3 Continuous Integration (GitHub Actions)

`ci.yml` — on every push/PR:
1. Matrix: `{ubuntu-latest, macos-latest} × {gcc, clang} × {Debug, Release}`.
2. Build.
3. Run unit tests (Catch2, via `ctest`).
4. Run perft suite (bounded depth for CI time budget).
5. Run ASan/UBSan build + reduced test suite on at least one matrix leg.
6. Upload build artifacts / test logs.
7. (Optional, once stable) `clang-tidy` and `clang-format --check` as a lint job.

`benchmark.yml` — triggered by **changed paths**, not by a commit-message tag or convention the developer has to remember (see 3.11 for the rationale): any push/PR touching `src/movegen/`, `src/position/`, `src/search/`, `src/eval/`, or `bench/` itself runs the full suite; a push touching only `docs/`, `README.md`, or similar non-engine paths does not. Also runnable manually (`workflow_dispatch`) and on a weekly schedule as a noise/drift check independent of any single commit.
1. Build `Release` with fixed flags on a consistent runner.
2. Run `bench/microbench` and `bench/search_bench` (see 3.11 for micro vs. macro distinction), each multiple iterations, reporting median.
3. Emit a machine-readable JSON result (schema in 3.11) tagged with commit hash, compiler, and CPU; append it to `bench/results/history/`.
4. Compare against the last committed baseline; post a summary (job summary / PR comment) showing deltas, flagging only changes beyond a ±5% tolerance band as likely meaningful (raw CI-runner noise is real — see 3.11).
5. Commit new *baseline* only on explicit approval (not auto-committed from CI); the per-commit history file itself, by contrast, **is** auto-committed/appended by the workflow, since its entire value is being an unedited historical record.

## 2.4 Logging & Diagnostics

- Lightweight internal logging facility (`util/logging.h`) with levels (`Trace`, `Debug`, `Info`, `Warn`, `Error`), compiled out (or minimal-cost) in `Release` builds for the hot search path, fully available in `Debug`.
- **Critical constraint**: logging must never write to `stdout` during a UCI session except via well-formed UCI protocol lines — stray debug output on stdout breaks every GUI. Route diagnostic logs to `stderr` or a file, never stdout, by construction (make this hard to get wrong, e.g., the logger simply has no stdout sink).
- Search should support an optional structured trace (e.g., periodic `info` UCI lines: depth, seldepth, nodes, nps, hashfull, pv) — this doubles as both the UCI-required output and a debugging aid.
- Assertions (`DAHLIA_ASSERT`) compiled in for `Debug`/sanitizer builds, compiled out for `Release`, used liberally for invariants (e.g., "bitboard popcount ≤ 64", "TT entry key matches probe key").

## 2.5 Profiling Support

- Build with `-g` even in `Release` (`RelWithDebInfo`-style) for at least one CI/benchmark leg so `perf record`/Instruments/`samply` produce meaningful symbols.
- Document (in `docs/architecture.md` or a `docs/profiling.md`) the exact local recipe used (e.g., `perf stat`/`perf record -g` on Linux, Instruments Time Profiler on macOS) so profiling is reproducible session-to-session rather than re-derived each time.

---

# Part III — Subsystem Specifications

Each subsystem below follows the same template: Purpose, Responsibilities, Public Interface (sketch), Internal Data Structures, Future Extensions, Common Pitfalls, Implementation Order. These are design sketches, not final APIs — the actual interface is decided at implementation time but should be reconciled with this section afterward.

## 3.1 Core Types (`core/`)

**Purpose:** Fundamental vocabulary types used everywhere: `Square`, `Piece`, `Color`, `Bitboard`, `CastlingRights`.

**Responsibilities:** Define types + small constexpr utilities (square arithmetic, file/rank extraction, bitboard set operations). No chess *rules* logic here — that belongs to `movegen`/`position`.

**Public interface (sketch):**
```cpp
enum class Color : uint8_t { White, Black };
enum class PieceType : uint8_t { Pawn, Knight, Bishop, Rook, Queen, King };
enum class Square : uint8_t { A1, B1, ..., H8, None = 64 };

namespace bb {
  constexpr Bitboard square_bb(Square s);
  constexpr Square lsb(Bitboard b);       // std::countr_zero wrapper
  constexpr int popcount(Bitboard b);      // std::popcount wrapper
  constexpr Bitboard shift(Bitboard b, Direction d);
}
```

**Internal data structures:** none beyond the enums/typedefs and small constexpr lookup tables (e.g., file/rank masks, precomputed at compile time via `consteval`).

**Future extensions:** none expected — this layer should be nearly frozen after Milestone 0.

**Common pitfalls:** letting "just one" piece of chess-rules logic leak in here (e.g., "is this square attacked" does *not* belong in `core`, it belongs in `movegen`). Keep this layer boring on purpose.

**Implementation order:** first thing built after project scaffolding; nothing else compiles without it.

## 3.2 Move Representation (`core/move.h`)

**Purpose:** Compact, cheap-to-copy representation of a chess move.

**Responsibilities:** Encode from/to/promotion/move-type (capture, castle, en passant, double pawn push) with accessor methods.

**Decision (2026-07-25):** keep the current plain-struct representation (`Square from; Square to; Promotion promotion;`) rather than packing into a 16-/32-bit integer. Packing is deferred, tracked as a GitHub issue rather than committed here — revisit once benchmarking (§1.7) shows move-list/TT cache pressure actually matters, rather than packing preemptively.

**Public interface (sketch, struct-based):**
```cpp
struct Move {
  Square from;
  Square to;
  Promotion promotion = NO_PROMOTION;
  // move-type (capture, castle, en passant, double pawn push) to be added
  // as movegen requires distinguishing them beyond from/to/promotion.
};
```

**Internal data structures:** plain field-by-field struct (no bit-packing). If the deferred packing issue is ever picked up, revisit this section and record the bit layout explicitly in the header comment — the encoding would not be obvious from the type otherwise.

**Future extensions:** adding a "scored move" wrapper (`Move` + `int16_t score`) for move ordering, kept as a separate type so `Move` itself stays minimal and TT-friendly.

**Common pitfalls:** conflating "no move"/null-move sentinel with a legitimate move's field values (e.g., `from == to` used as a null sentinel might collide with a real edge case — define an explicit sentinel); forgetting promotion needs to be captured for both quiet promotions and capture-promotions; once a move-type/flag field is added, equality must account for it, not just `from`/`to`/`promotion`.

**Implementation order:** immediately after `core` types, before movegen (movegen produces `Move`s).

## 3.3 Move Generation (`movegen/`)

**Purpose:** Given a `Position`, produce the set of legal (or pseudo-legal, see below) moves, as fast as possible, with perft-verified correctness.

**Responsibilities:**
- Precomputed attack tables for non-sliding pieces (knight, king, pawn attacks) via `consteval`/compile-time generation.
- **Magic bitboards** for sliding pieces (bishop/rook, queen = union).
- **Decision (2026-07-25):** pseudo-legal generation + legality filter during `make_move` (king-safety check), not fully legal generation up front. Generate pseudo-legal moves fast, then confirm legality via a fast "does this leave my king in check" test integrated into `make_move`/a dedicated `is_legal(Move)` check — the standard modern approach (see Stockfish), keeping the hot movegen loop simpler and more benchmarkable in isolation, at the cost of slightly more complex `make/unmake`. Write this up as `docs/adr/0001-pseudo-legal-movegen.md` at implementation time.
- Provide both "generate all pseudo-legal" and "generate captures/checks only" (for quiescence search) entry points.

**Public interface (sketch):**
```cpp
namespace movegen {
  int generate_pseudo_legal(const Position&, std::span<Move> out);
  int generate_captures(const Position&, std::span<Move> out); // for quiescence
  bool is_square_attacked(const Position&, Square, Color by);
}
```

**Internal data structures:**
- Precomputed knight/king/pawn-attack tables (`std::array<Bitboard, 64>`).
- Magic tables: per-square magic number, mask, shift, and pointer/offset into a shared attack table (classic "fancy magic bitboards" layout — single contiguous array indexed via precomputed offsets minimizes memory and improves cache behavior vs. one array per square).
- Magic numbers: generate via a one-time offline search (documented, reproducible tool under `tools/`), checked into source as constants — do not regenerate at runtime (startup-time metric matters).

**Future extensions:** PEXT-based attack lookup (`__builtin_ia32_pext` / `_pext_u64`, BMI2) as a faster alternative to magic multiplication on supported hardware, selected via a compile-time or runtime CPU-feature switch — a great "measurable optimization" case study (magic vs PEXT lookup latency, see [Metrics Catalog](#part-iv--metrics-catalog--benchmarking)).

**Common pitfalls:** off-by-one in magic shift/index math; forgetting edge-of-board wraparound when generating knight/king attacks (classic bug: knight on `a-file` "attacking" `g/h`-file squares); pawn double-push and en passant interacting incorrectly with the legality filter; generating castling moves without checking intervening-square-attacked conditions.

**Implementation order:** right after `Move`, and *before* `Position`/make-unmake, since perft testing of movegen against known positions is most valuable when it can be done against a minimal, correct `Position` shell before search exists at all.

## 3.4 Position & State Management (`position/`)

**Purpose:** Represent a chess position and its mutation (`make_move`/`unmake_move`), including all state needed to reverse a move exactly (castling rights, en passant square, halfmove clock, captured piece, Zobrist key).

**Responsibilities:**
- Bitboard-based board representation (`Bitboard pieces[Color][PieceType]`, `Bitboard occupied[Color]`, `Bitboard occupied_all`), plus a mailbox array (`Piece board[64]`) for O(1) "what's on this square" — a well-known worthwhile redundancy in bitboard engines; document the sync invariant clearly.
- **Zobrist hashing**: incremental key update on `make_move`/`unmake_move`, not recomputed from scratch — with a unit test asserting the incremental key always matches a from-scratch computation (see 1.6).
- `make_move`/`unmake_move` pair that mutates in place and can perfectly restore prior state (this requires a small "undo stack" of the irreversible state: captured piece, prior castling rights, prior en passant square, prior halfmove clock, prior Zobrist key).
- FEN parsing/serialization (`Position::from_fen`, `Position::to_fen`) — needed for UCI `position fen ...` and for test suites.

**Public interface (sketch):**
```cpp
class Position {
 public:
  static Position from_fen(std::string_view fen);
  std::string to_fen() const;
  void make_move(Move m, StateInfo& undo);
  void unmake_move(Move m, const StateInfo& undo);
  uint64_t zobrist_key() const;
  bool is_in_check(Color c) const;
  ...
};
```

**Internal data structures:** the `StateInfo`/undo-stack entry described above; Zobrist random-number tables (`std::array` of precomputed constants for piece-square, castling rights, en passant file, side-to-move — generated once via a fixed-seed PRNG at build time or `consteval`, checked in as constants for reproducibility).

**Future extensions:** supporting `null_move`/`unmake_null_move` for null-move pruning (Zobrist key still needs a well-defined update rule for "no move played, side switched").

**Common pitfalls:** the classic "sync bug" between bitboards and mailbox representation after a promotion or capture; forgetting to include the *previous* en passant square in the Zobrist key XOR (must XOR out old, XOR in new, not just XOR in new); halfmove clock reset rules (reset on pawn move or capture, not otherwise); castling rights update when a rook is captured on its home square (not just when it moves).

**Implementation order:** after movegen exists (so make/unmake can be tested via perft immediately), before search.

## 3.5 Perft & Correctness Harness (`tests/perft/`)

**Purpose:** The primary correctness oracle for movegen + make/unmake, before any search or evaluation exists.

**Responsibilities:** implement `perft(depth)` (raw node count) and `perft_divide(depth)` (per-move subtree counts, for bisecting failures). Maintain a table of known-correct perft results for standard test positions (start position, Kiwipete, positions 3–6 from the CPW perft page) up to a depth practical for CI.

**Public interface:** a small standalone binary/test target, not part of the engine library, since perft has no business being in the shipped UCI binary's hot path (though a `go perft N` UCI extension for manual debugging is a reasonable, common addition).

**Common pitfalls:** using perft results from an untrusted source rather than the standard, widely-cross-checked CPW values; not testing en passant/castling/promotion-heavy positions (the standard "Kiwipete" position exists specifically because it's rich in these edge cases — always include it).

**Implementation order:** built alongside movegen/position, used continuously from that point forward as a regression gate.

## 3.6 Evaluation (`eval/`)

**Purpose:** Assign a numeric score to a position from the side-to-move's perspective.

**Responsibilities (staged):**
- v0: material count only (piece values) — enough to make search meaningful and testable.
- v1: piece-square tables (PSTs), tapered for game phase (midgame/endgame interpolation).
- v2 (future, optional): mobility, pawn structure, king safety terms.
- v3 (future, optional/stretch): NNUE-style learned evaluation — explicitly a stretch goal, not required for the portfolio story to succeed; classical eval that's well-documented and well-tested is itself a strong showcase, and NNUE brings a large scope/complexity jump (training pipeline, data generation) that should be its own clearly-scoped milestone if pursued.

**Public interface (sketch):**
```cpp
namespace eval {
  int16_t evaluate(const Position&); // centipawns, side-to-move relative
}
```

**Internal data structures:** PST tables (`std::array<std::array<int16_t,64>, 6>` per phase), game-phase counter (incremental, updated in make/unmake alongside material).

**Future extensions:** incremental evaluation updates (update score delta in make/unmake rather than recomputing fully each node) once profiling shows eval cost matters relative to search overhead.

**Common pitfalls:** sign errors relative to side-to-move (a shockingly common and shockingly hard-to-notice bug class — guard with tests asserting `evaluate(pos) == -evaluate(pos.mirrored())` for symmetric positions); PST tables not flipped correctly for Black.

**Implementation order:** material-only version can exist as soon as `Position` exists; needed before search produces meaningful moves.

## 3.7 Transposition Table (`search/tt.h`)

**Purpose:** Cache previously-searched position results to avoid re-searching transposed move orders; central to search efficiency and a rich source of measurable metrics (hit rate, hashfull, collision rate).

**Responsibilities:** fixed-size hash table (power-of-two sized, indexed via Zobrist key), entries storing: key (or a verification tag), best move, score, depth, node type (exact/lower-bound/upper-bound), age/generation for replacement.

**Public interface (sketch):**
```cpp
class TranspositionTable {
 public:
  void resize(size_t megabytes);
  void store(uint64_t key, Move best, int16_t score, int8_t depth, Bound bound, uint8_t generation);
  std::optional<TTEntry> probe(uint64_t key) const;
  void new_search(); // bump generation
  int hashfull_permille() const; // UCI "hashfull"
};
```

**Internal data structures:** entries packed to a cache-line-friendly size (classic target: 16 bytes/entry, several entries per cache-line "bucket" — e.g., 4-way bucketed table, replacement scheme preferring lower depth/older generation within a bucket). Lockless-safe layout is a future concern (SMP), not required initially.

**Future extensions:** SMP-safe TT (XOR-checksum trick for lockless concurrent access), TT-based move ordering hints, persistent TT across `ucinewgame` vs. reused.

**Common pitfalls:** Zobrist key collisions silently returning wrong scores (mitigate with a verification tag stored alongside, e.g., top bits of the key not used for indexing); replacement scheme that thrashes useful entries; forgetting to bump generation on `ucinewgame`/new search, causing stale-but-valid-looking entries from a previous unrelated position.

**Implementation order:** after basic alpha-beta search works without it; add TT as a measured optimization (before/after node-count benchmark required, per 1.7).

## 3.8 Search (`search/`)

**Purpose:** Find the best move within a time/node budget. This is the subsystem with the most staged complexity — build it up incrementally, each stage independently benchmarkable.

**Responsibilities, staged (see [Roadmap](#part-v--staged-roadmap) for milestone mapping):**
1. Plain negamax alpha-beta, fixed depth.
2. Iterative deepening (depth 1..N, using each depth's result to inform the next — also enables anytime behavior for time control).
3. Quiescence search at leaf nodes (resolve captures to avoid horizon effect) — a documented, common source of subtle bugs, isolate and unit-test its termination condition explicitly (stand-pat, delta pruning).
4. Move ordering: TT move first, then captures (MVV-LVA), then killer moves, then history heuristic, then remaining quiets.
5. Principal Variation Search (PVS): full window on the first move at each node, null-window (scout) search on the rest, re-search on fail-high.
6. Killer move heuristic (2 killers per ply, indexed by ply not by node, cleared appropriately).
7. History heuristic (indexed `[side][from][to]` or `[side][piece][to]`, aged/decayed to avoid stale saturation).
8. Null-move pruning (with zugzwang guard — disable in low-material/pawn-only endgames where null move is unsound).
9. Aspiration windows around the previous iteration's score.
10. Late move reductions (reduce search depth for late, quiet, non-critical moves in ordered move list; re-search at full depth on fail-high).
11. (Future) SMP search (lazy SMP is the most common modern approach: multiple threads searching the same tree sharing one TT, minimal synchronization) — explicitly scoped as a later milestone given the complexity jump (thread-safety of TT, node counting, `stop` handling across threads).

**Public interface (sketch):**
```cpp
class Search {
 public:
  SearchResult think(const Position&, const SearchLimits& limits); // time/depth/nodes
  void stop();  // for UCI "stop"
 private:
  int16_t negamax(Position&, int depth, int16_t alpha, int16_t beta, int ply);
  int16_t quiescence(Position&, int16_t alpha, int16_t beta, int ply);
};
```

**Internal data structures:** per-ply data (killers, static eval cache), history table (persists across a `go`, decayed not cleared between iterative-deepening iterations, cleared on `ucinewgame`), a `SearchLimits`/time manager (see 3.9), a `SearchStack`/PV-tracking structure to reconstruct the principal variation for UCI `info pv` output.

**Future extensions:** listed inline above (LMR, aspiration, SMP); further out — singular extensions, multi-cut, razoring, futility pruning, staged move generation (generate captures first, only generate quiets if needed) once profiling shows movegen cost dominates in quiet-heavy nodes.

**Common pitfalls:** mate-score handling across ply boundaries (must adjust mate distance when propagating up the tree — a very common off-by-one/sign bug); fail-soft vs fail-hard alpha-beta consistency (pick one, document it, be consistent — mixing them breaks PVS re-search logic); null-move pruning in zugzwang positions (K+P endgames) giving wrong results without a guard; killer/history tables not reset between `ucinewgame` causing move-ordering bias from an unrelated prior game; not respecting `stop`/time checks frequently enough (checking every node is too slow, checking too rarely blows time controls — check via a node-count modulus, e.g., every 2048 nodes).

**Implementation order:** plain alpha-beta first (Milestone 2), each subsequent technique added as its own milestone/PR with a dedicated before/after node-count benchmark proving it actually reduces the tree (not just "feels like it should").

## 3.9 Time Management (`search/timeman.h`)

**Purpose:** Convert UCI `go` time-control parameters (`wtime`/`btime`/`winc`/`binc`/`movestogo`/`movetime`/`depth`/`nodes`/`infinite`) into a per-move time (or node) budget, and enforce it during iterative deepening.

**Responsibilities:** soft limit (stop starting a new iterative-deepening iteration if unlikely to finish/unlikely to be useful) vs. hard limit (must return a move by this time, checked frequently inside search). Simple formula-based allocation first (e.g., remaining time / expected remaining moves + increment, with a cap), refined later (e.g., extending time when the best move is unstable across iterations).

**Common pitfalls:** losing on time due to hard-limit checks being too infrequent; not accounting for `movestogo` in classical time controls; not handling `infinite`/`ponder` (even if `ponder` support itself is deferred, `go infinite` must be handled correctly since GUIs use it for analysis mode).

**Implementation order:** needed as soon as iterative deepening exists and the engine is driven via real UCI time controls rather than fixed depth in test harnesses.

## 3.10 UCI Protocol (`uci/`)

**Purpose:** Implement the Universal Chess Interface so Dahlia works with standard GUIs (Arena, Cute Chess, ChessBase, Banksia, etc.) and match-running tools (`cutechess-cli`) used for SPRT testing.

**Responsibilities:** parse/respond to `uci`, `isready`, `ucinewgame`, `position [fen|startpos] moves ...`, `go [...]`, `stop`, `quit`, `setoption name ... value ...`; declare supported options (`Hash`, `Threads` once SMP exists, `Move Overhead`, maybe `Ponder`); emit correctly-formatted `info` lines during search (depth, seldepth, score cp/mate, nodes, nps, hashfull, pv) and a final `bestmove`.

**Public interface:** a single blocking read-loop over stdin, dispatching to a small command table — deliberately simple/boring, since UCI is a solved, well-specified protocol and cleverness here just introduces GUI-compatibility bugs.

**Common pitfalls:** buffering/flushing issues (must flush stdout after every line, GUIs can hang otherwise); any stray non-UCI output on stdout (see 2.4 — logging discipline exists specifically to prevent this); off-by-one in `mate` score reporting (moves-to-mate vs. plies-to-mate — UCI wants moves); not handling `position startpos moves ...` with a long move list efficiently (replaying from scratch each time is fine and simplest; do not over-engineer this).

**Implementation order:** minimal UCI (enough to respond to `uci`/`isready`/`position`/`go`/`bestmove` with a legal random or fixed-depth move) should exist very early (Milestone 1/2) so the engine is GUI-testable from early on, even before search is any good — "it plays a legal game in a real GUI" is a strong, demoable milestone.

## 3.11 Benchmark & Regression Framework (`bench/`)

**Purpose:** Make every performance claim about Dahlia falsifiable and reproducible, and — just as important — catch *accidental* performance regressions that correctness tests can't see. Deliberately not called a "performance monitoring suite": monitoring implies watching one number go up; this framework's job is equally to notice when a change that passes perft and unit tests quietly makes the engine 18% slower. This is as much a portfolio artifact (a plottable, citable history of the engine's evolution) as it is an engineering tool.

**Responsibilities:** see 1.7 for the correctness-then-measurement-then-optimization philosophy. Four categories of thing this framework tracks, only the first two of which run automatically per-commit:

1. **Correctness** (always run, every push — this is `ci.yml`, not `benchmark.yml`): perft at CI-budget depth, known tactical positions, FEN round-trip, move legality, hash consistency, repetition detection. Cheap enough to never gate behind path-filtering.
2. **Performance** (run on `benchmark.yml`, gated by changed paths — see below): nodes/sec, move generation time, search speed, evaluation speed, TT probe/store latency and hit rate, memory usage, startup latency.
3. **Engine strength** (SPRT via `cutechess-cli`/`tools/sprt`, run manually/on-demand, not per-commit): too expensive for every push. A change can raise NPS and lower strength (e.g., an unsound pruning heuristic) or vice versa — these are genuinely different questions, both tracked, never conflated.
4. **Profiling-derived signals** (periodic/manual, not gated): move-ordering cutoff %, effective branching factor, null-move cutoff rate, LMR re-search rate, TT cutoff %, quiescence node share, eval-cache hit rate. Not CI-gated pass/fail metrics — these are the "fascinating graphs" category, consumed via `docs/profiling.md` sessions rather than automated thresholds.

**Two kinds of performance benchmark, not one** (both matter — they catch different classes of regression):
- **Microbenchmarks** (Google Benchmark, `bench/microbench`): isolated hot functions — magic attack lookup, `make_move`/`unmake_move`, `evaluate()`, TT probe/store, move list generation for a battery of representative positions (opening/tactical/endgame). Millisecond-scale, isolate a change to one subsystem.
- **Macrobenchmarks** (`bench/search_bench`): the whole engine — fixed position set (opening/middlegame/endgame/tactical mix), fixed node or time budget, tracking nodes/sec, depth reached, and (the most meaningful long-run metric) nodes-to-reach-depth-N, which isolates search-quality improvements from raw hardware/implementation speed. Necessary because a microbenchmark can get faster in isolation while the whole engine gets slower due to cache pressure or overhead introduced elsewhere — a macrobenchmark is the only thing that would catch that.

**Decision (2026-07-26): trigger `benchmark.yml` by changed source paths, not by a commit-message/PR tag.** The original idea considered a `(perft)`-style commit tag to mark performance-relevant commits, so the (expensive) benchmark suite only runs when a human flags it. Rejected in favor of path-based triggering (any push touching `src/movegen/`, `src/position/`, `src/search/`, `src/eval/`, or `bench/`) because it doesn't rely on a developer remembering to tag correctly — a forgotten tag on a real performance-relevant commit is a silent gap in the regression history, whereas path filtering fires deterministically off what actually changed. A manual `workflow_dispatch` trigger remains available for the rare case of wanting to force a run on a docs-only or non-matched commit (e.g., a runner/toolchain change).

**Per-commit history, not just a rolling baseline:** every `benchmark.yml` run appends one machine-readable result to `bench/results/history/`, regardless of whether it becomes the new approved baseline. This is what makes "nodes/sec over the project's lifetime" a real, plottable dataset rather than a single current number — see 1.7 and the Milestone 7 "historical benchmark chart" deliverable.

Filename convention: `bench/results/history/<date>-<short-hash>.json`. Schema (illustrative, not final — extend as new metrics are added, but keep it flat/machine-parseable):

```json
{
  "commit": "8d12a91",
  "timestamp": "2026-07-26T14:03:00Z",
  "compiler": "clang-18",
  "build": "Release",
  "cpu": "Apple M-series / x86_64 model string",
  "benchmarks": {
    "move_generation": { "rook_ns": 2.3, "bishop_ns": 2.1, "queen_ns": 2.4 },
    "search": { "nodes_per_second": 134122991, "depth": 9 },
    "perft": { "depth5_ms": 192, "depth6_ms": 1202 }
  }
}
```

**Noise discipline** (performance measurements are notoriously noisy — a naive per-commit comparison will cry wolf constantly and train everyone to ignore it):
- Run each benchmark multiple iterations per commit; report median, not a single sample.
- Benchmark only `Release`/`RelWithDebInfo` builds with fixed, recorded compiler flags — never Debug/sanitizer builds.
- Run on a consistent runner/machine where possible; record compiler version and CPU model in every result file regardless, so cross-machine noise is at least explainable after the fact.
- Only flag a delta as a likely-meaningful regression/improvement above a tolerance band (±5% as a starting point, tune once real history exists); smaller deltas are logged but not flagged.

**Implementation order:** microbenchmark scaffolding as early as Milestone 0/1 (even trivial ones), search/macro benchmarks once negamax exists, the path-triggered `benchmark.yml` + history-file workflow as soon as there are at least two commits worth comparing (roughly Milestone 1–2), SPRT tooling once there's a second version worth comparing strength against (roughly Milestone 3+).

---

# Part IV — Metrics Catalog & Benchmarking

For every metric below: what it measures, how to measure it, what tooling produces it, and what regression test/guard exists.

| Metric | What it tells you | How measured | Tooling | Regression guard |
|---|---|---|---|---|
| **Perft node count** | Movegen/make-unmake correctness | Compare `perft(N)` to known-correct reference values | `tests/perft` | CI test, fails build on mismatch |
| **Perft runtime** | Raw movegen + make/unmake throughput | Wall-clock time for `perft(6)` from start position, fixed hardware/flags | `tests/perft` timed mode | Tracked in `bench/results/`, flagged on regression |
| **Nodes per second (NPS)** | Search throughput | `nodes / elapsed_time` during a fixed-time or fixed-depth search on benchmark positions | `bench/search_bench` | Tracked per commit in `benchmark.yml` |
| **Magic lookup latency** | Sliding-attack lookup cost | Google Benchmark micro-bench isolating `attacks(square, occupancy)` call | `bench/microbench` | Regression band on `benchmark.yml` |
| **Move generation speed** | Cost of producing a full pseudo-legal move list | Google Benchmark over representative positions (batch, avg moves/position) | `bench/microbench` | Regression band |
| **TT hit rate** | Search efficiency / cache effectiveness | `hits / probes` counted during a fixed search | Instrumented counters, reported via debug UCI `info string` or bench harness | Tracked, expect increase after ordering/TT improvements |
| **Hashfull (‰)** | TT memory pressure at a given hash size | UCI standard `info hashfull` | Built into TT | N/A (informational, UCI-required) |
| **Search depth reached** | Search efficiency within time budget | Max depth completed in fixed time on benchmark positions | `bench/search_bench` | Tracked; should trend up as pruning improves |
| **Effective branching factor (EBF)** | Search efficiency independent of raw speed | `nodes(depth) / nodes(depth-1)` averaged, or perft-style ratio | Computed from search node counts at successive depths | Tracked; should trend down as ordering/pruning improves |
| **Nodes to reach depth N** | Isolates algorithmic search quality from hardware speed | Fixed position, fixed depth, count nodes (not time) | `bench/search_bench` | The primary metric for judging "did this pruning/ordering change actually help" |
| **Memory usage (RSS)** | Efficiency / footprint, matters for TT sizing | `/usr/bin/time -v` (Linux) or Instruments (macOS) during a fixed search | Manual/CI script | Track for major structural changes (e.g., TT entry size change) |
| **Startup time** | UX + "no wasted runtime init" engineering discipline | Wall-clock from process start to `uciok`/`readyok` | Simple timed script | Should stay near-zero given constexpr table generation; flag if it grows |
| **Binary size** | Build hygiene, LTO/inlining sanity check | `size`/`ls -la` on the release binary | CI artifact metadata | Informational; flag large unexplained jumps |
| **Cache friendliness** | Data layout quality | `perf stat -e cache-misses,cache-references` (Linux) or Instruments on hot functions (movegen, TT probe) | Manual profiling session, documented in `docs/profiling.md` | Not CI-gated; used to justify layout decisions (e.g., TT entry packing, fancy-magic single-array layout) |
| **Test coverage** | Signal for undertested code paths | `llvm-cov`/`gcov` report | CI job (informational) | Reported, not gated (see 1.6 rationale) |
| **Elo gain / strength** | Actual chess-playing improvement | SPRT match vs. previous version or reference engine | `tools/sprt` + `cutechess-cli` | Required before claiming a search/eval change is a "strength improvement," as opposed to merely a speed improvement |

**Rule of thumb:** a PR that touches search or movegen and claims a performance or strength win must cite at least one row from this table with before/after numbers. "Feels faster" is not a metric.

---

# Part V — Staged Roadmap

Every milestone must leave `master` in a working, UCI-playable state (even Milestone 0/1, trivially). Each milestone lists goals, deliverables, benchmarks, tests, and measurable success criteria.

### Milestone 0: Project Scaffolding
- **Goals:** establish the engineering foundation before chess logic accumulates further in the flat, ad hoc layout.
- **Deliverables:** CMake build (2.1), directory restructure (1.1), `.clang-format`/`.clang-tidy`, GitHub Actions `ci.yml` skeleton (build-only initially), Catch2 wired in with a trivial passing test, this reference document committed.
- **Benchmarks:** none functional yet; confirm `bench/` scaffolding builds an empty/trivial Google Benchmark target.
- **Tests:** one placeholder unit test per planned module, to lock in the directory/target structure.
- **Success criteria:** `cmake --preset debug && cmake --build build && ctest` succeeds on a clean checkout on at least two machines/compilers (or CI matrix legs).

### Milestone 1: Core Types, Move Representation, Move Generation
- **Goals:** correct, fast, perft-verified pseudo-legal move generation with magic bitboards.
- **Deliverables:** `core/`, packed `Move`, precomputed attack tables, magic bitboard sliding attacks (with the offline magic-number generator tool checked in), `movegen::generate_pseudo_legal`.
- **Benchmarks:** magic lookup latency, move generation speed microbenchmarks committed as the first real `bench/results/` baseline.
- **Tests:** perft against start position + Kiwipete + positions 3–6, to at least depth 5–6 in CI (deeper locally/manually).
- **Success criteria:** perft matches reference values exactly at all tested depths; magic lookup benchmark recorded as baseline.

### Milestone 2: Position, Make/Unmake, Zobrist, Minimal UCI
- **Goals:** a `Position` that can legally play a full game via make/unmake, with a minimal UCI loop making it GUI-testable.
- **Deliverables:** `Position` (FEN in/out, make/unmake, legality filter per 3.3's ADR), Zobrist hashing with incremental-vs-scratch test, minimal `uci/` responding to `uci`/`isready`/`ucinewgame`/`position`/`go`/`stop`/`quit` with (initially) random or one-ply-material-greedy move selection.
- **Benchmarks:** make/unmake cost microbenchmark.
- **Tests:** make/unmake round-trip property test (position + Zobrist key restored exactly); perft re-run at deeper depths now that a real `Position` exists end-to-end; scripted UCI protocol tests.
- **Success criteria:** engine loads in a real GUI (e.g., Arena/Cute Chess) and plays a full legal game against itself or a human, however weak.

### Milestone 3: Material Eval + Plain Alpha-Beta Search
- **Goals:** the engine makes non-random, non-trivial decisions.
- **Deliverables:** material-only `eval::evaluate`, negamax alpha-beta at fixed depth, iterative deepening, basic time management (3.9).
- **Benchmarks:** first `bench/search_bench` baseline: nodes/sec, depth reached in fixed time, on the standard position set.
- **Tests:** search regression tests on a handful of tactical positions solvable at shallow depth (e.g., simple mate-in-2/free-piece-capture puzzles).
- **Success criteria:** engine reliably avoids hanging material and finds simple tactics; first SPRT-comparable version tag exists for future strength tracking.

### Milestone 4: Move Ordering, Quiescence, TT
- **Goals:** search efficiency — same or better tactical strength at dramatically lower node counts.
- **Deliverables:** MVV-LVA capture ordering, killer heuristic, history heuristic, quiescence search, transposition table.
- **Benchmarks:** nodes-to-reach-depth-N before/after each of these, individually, per PR (1.7's discipline); TT hit rate tracked.
- **Tests:** quiescence termination/stand-pat unit tests; TT correctness tests (verification tag prevents wrong-position hits); expanded tactical suite (e.g., a checked-in subset of a standard EPD tactics suite).
- **Success criteria:** measurable EBF reduction vs. Milestone 3 baseline at the same benchmark positions; first SPRT match run and recorded against Milestone 3's tag, expecting a clear Elo gain.

### Milestone 5: PVS, PSTs/Tapered Eval, Null-Move Pruning
- **Goals:** stronger search algorithm and richer evaluation.
- **Deliverables:** Principal Variation Search, piece-square tables with tapered midgame/endgame eval, null-move pruning with zugzwang guard.
- **Benchmarks:** nodes-to-depth-N again; SPRT vs. Milestone 4 tag.
- **Tests:** eval symmetry test (3.6); null-move zugzwang regression test (a known K+P endgame position where naive null-move fails, confirming the guard works).
- **Success criteria:** SPRT-confirmed Elo gain over Milestone 4.

### Milestone 6: Aspiration Windows, Late Move Reductions
- **Goals:** deeper effective search within the same time budget.
- **Deliverables:** aspiration windows around prior iteration score with re-search on fail-high/low; LMR with re-search on fail-high.
- **Benchmarks:** depth-reached-in-fixed-time before/after; nodes-to-depth-N (LMR trades node-count-per-depth for raw depth, so track both, not just one).
- **Tests:** re-search correctness tests (aspiration/LMR fail-high must produce the same result as a full-window/full-depth search would have — a property test comparing against a "safe mode" search on a small position set is valuable here).
- **Success criteria:** SPRT-confirmed Elo gain over Milestone 5; document the LMR/aspiration tuning constants and rationale in an ADR.

### Milestone 7: Polish, Options, Portfolio Packaging
- **Goals:** the "resume-ready" milestone — everything above this line is chess engineering; this milestone is presentation and completeness.
- **Deliverables:** full `docs/architecture.md` with diagrams, `README.md` with strength estimate/build instructions/demo GIF, complete UCI option set (`Hash`, `Move Overhead`, etc.), historical benchmark chart (nodes/sec and Elo over the project's milestones, generated from `bench/results/` history), ADR log reviewed for completeness.
- **Benchmarks:** consolidated report across all prior milestones (the "engine's performance history" chart).
- **Tests:** full CI matrix green (multi-compiler, sanitizers, perft, unit, UCI protocol).
- **Success criteria:** a stranger can clone the repo, read the README, build it, load it into a GUI, and understand — from `docs/` and ADRs alone — every major design decision and why it was made.

### Beyond Milestone 7 (explicitly future/stretch)

**Decision (2026-07-25):** the following are committed stretch goals — planned, but explicitly scoped for *after* Milestone 7 and a working, portfolio-ready engine exist. They are deliberately not pulled into the milestone list above so that the core roadmap (0–7) stays the thing that must ship.

- **Lazy-SMP search** — multithreaded search with multiple threads sharing one TT and minimal synchronization. Real complexity in thread-safety (TT access, node counting, `stop` propagation across threads); gets its own milestone (tentatively "Milestone 8") with its own benchmarks (NPS scaling vs. thread count, not just single-thread NPS) and its own TSan CI leg once implemented.
- **Opening book support + evaluation tuning harness** (`tools/tuner`, e.g., Texel tuning against a game database) — moderate scope, mostly integration and data-pipeline work rather than new algorithmic complexity. Also a later milestone, after Milestone 7.

**Explicitly out of scope for now** (not planned, revisit only if priorities change):
- PEXT-based attack lookup as a BMI2 alternative to magics — cheap enough to reconsider later, but not committed.
- NNUE-style learned evaluation + training pipeline — large scope jump (data generation, training infra); a classical, well-documented, well-tested eval is itself sufficient for the portfolio goal, so this is not required.
- Endgame tablebase probing (Syzygy) — not committed; revisit if the engine reaches a strength level where it matters.

---

# Appendix A — Glossary

- **Perft**: "performance test" — exhaustive move-count enumeration to a fixed depth, used to verify move generator correctness against known reference values, not for performance despite the name's origin.
- **Zobrist hashing**: a technique for incrementally maintaining a hash key representing board state via XOR of precomputed random numbers per piece/square/state feature.
- **Magic bitboards**: a technique using a precomputed "magic" multiplier per square to perfectly hash a masked occupancy bitboard into an index into a precomputed attack table, giving O(1) sliding-piece attack lookup.
- **PVS (Principal Variation Search)**: an alpha-beta refinement that searches the first move at full window and subsequent moves with a cheaper null window, re-searching only if a move unexpectedly beats the window.
- **LMR (Late Move Reductions)**: reduce search depth for moves ordered late (assumed less likely to be best), re-searching at full depth if they turn out to beat alpha.
- **Null-move pruning**: search assuming the side to move "passes," to cheaply prove a position is so good a real move must also be good — unsound in zugzwang positions, hence the guard.
- **EBF (Effective Branching Factor)**: ratio of nodes searched between consecutive depths; a proxy for search/pruning efficiency independent of raw hardware speed.
- **SPRT (Sequential Probability Ratio Test)**: a statistical test used (originally by the Stockfish project) to determine, with as few games as possible, whether an engine change is a real strength improvement versus noise.

---

# Appendix B — External References Worth Consulting

(Not fetched/embedded here — consult directly when implementing the relevant subsystem.)

- Chess Programming Wiki — perft reference values, magic bitboards, standard search technique writeups.
- Stockfish source — as a reference for idiomatic modern engine architecture (not for copying code, for architectural inspiration and as a benchmark opponent once Dahlia is playable).
- `cutechess-cli` — for running UCI-vs-UCI matches (SPRT tooling).

---

# Appendix C — Architecture Decision Log

Record contested decisions here as they're made, newest first. Full-form ADRs for major decisions live in `docs/adr/`; this table is a quick index.

| Date | Decision | Summary | ADR |
|---|---|---|---|
| 2026-07-26 | Stage subsystem work as Correctness → Measurement → Optimization; benchmark suite triggers on changed source paths, not a commit tag | See 1.7, 3.11 | — (settled here, no separate ADR needed) |
| 2026-07-25 | Colocated `.h`/`.cpp` per module in `src/`, no separate `include/` tree | See 1.1 | — (settled here, no separate ADR needed) |
| 2026-07-25 | Feature branch + PR for every change, including solo-dev work | See 1.5 | — (settled here, no separate ADR needed) |
| 2026-07-25 | Lazy-SMP and opening-book/tuner are committed post-M7 stretch goals; NNUE and Syzygy are explicitly out of scope for now | See [Beyond Milestone 7](#beyond-milestone-7-explicitly-futurestretch) | — |
| 2026-07-25 | Pseudo-legal generation + legality filter in make_move, vs. fully legal movegen | See 3.3 | `docs/adr/0001-*.md` (to be written at implementation time) |
| _(pending)_ | CMake vs. retaining Makefile | See 2.1 | `docs/adr/0002-*.md` |

## Appendix C.1 — Resolved (2026-07-25)

| Topic | Decision |
|---|---|
| License | **MIT** |
| Test framework | **Catch2** |
| Benchmark framework | **Google Benchmark** |
| CI compiler/OS matrix | **GCC + Clang on Linux + macOS** (no Windows/MSVC leg) |
| `Move` representation | **Keep the current plain struct** (§3.2); packing to 16-bit tracked as a separate GitHub issue, not committed here |
| Merge policy | **Judgment call, as originally written** — squash trivial branches, regular-merge meaningful ones |
| Coverage gating | **Reported in CI, not gated on a percentage**, as originally written |
| Benchmark trigger mechanism | **Path-based** (changed files under `src/movegen/`, `src/position/`, `src/search/`, `src/eval/`, `bench/`), not a commit-message/PR tag — see 3.11 |
| Benchmark result history | **Every `benchmark.yml` run auto-commits a JSON file** to `bench/results/history/`; approving a new *baseline* remains a manual step — see 3.11 |

No open items remain in this appendix. Future undecided questions should be added here as they arise.

---

*End of reference document. Update this file whenever a design decision changes — it is the specification, not a snapshot.*
