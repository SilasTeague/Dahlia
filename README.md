# Dahlia

[![CI](https://github.com/SilasTeague/Dahlia/actions/workflows/ci.yml/badge.svg)](https://github.com/SilasTeague/Dahlia/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](#license)

A UCI-compatible chess engine written from scratch in modern C++ — bitboard move generation,
Zobrist-hashed make/unmake, iterative-deepening alpha-beta search with a transposition table,
and an asynchronous UCI loop that stays responsive mid-search.

### ▶ [Play it at silasteague.com/chess](https://silasteague.com/chess)

The live site runs the same statically-linked Linux binary this repository publishes on every
`v*` tag. Pick a side (White / Black / Random) and a time control (5 min, 10 min, or infinite),
and you're playing the engine in this repo.

---

## Contents

- [What Dahlia is](#what-dahlia-is)
- [Architecture](#architecture)
- [Build & run](#build--run)
- [UCI support](#uci-support)
- [Testing](#testing)
- [Benchmarks](#benchmarks)
- [Milestones](#milestones)
- [Design decisions](#design-decisions)
- [Release & deployment](#release--deployment)
- [Current limitations](#current-limitations)
- [Repository layout](#repository-layout)
- [License](#license)

---

## What Dahlia is

Dahlia is built with two co-equal goals, stated up front in
[`REFERENCE.md`](REFERENCE.md) and enforced by every decision since:

1. **Chess engine quality** — correct, and progressively stronger, move generation, search, and
   evaluation.
2. **Engineering quality** — a demonstration of professional systems practice: a one-direction
   module graph, perft-verified correctness, a real benchmark harness with committed history,
   a multi-compiler + sanitizer CI matrix, and architecture decision records explaining *why*
   each contested choice went the way it did.

The operating discipline is **Correctness → Measurement → Optimization, never the reverse**
([REFERENCE.md §1.7](REFERENCE.md)). Concretely, that means Dahlia deliberately ships the *slow,
obviously-correct* implementation first and only replaces it once a benchmark baseline exists to
measure the replacement against:

- Sliding-piece attacks are a **loop/bit-shift ray walk** over live occupancy, not magic
  bitboards. Magics are planned, not forgotten — the microbenchmark that will justify them
  (`BM_RookAttacksLookup` and friends) has been recording a baseline since Milestone 1.
- The transposition table was added *after* plain alpha-beta worked, so its effect could be
  quoted as a real before/after node-count delta rather than an assumption.
  ([See the numbers.](#the-transposition-table-milestone-4))

Every performance claim in this README is traceable to a committed JSON file under
[`bench/results/history/`](bench/results/history).

---

## Architecture

`src/` is organized as one directory per module with `.h`/`.cpp` colocated. Dependencies point in
exactly one direction, and this is a rule the code is held to, not a diagram of intent:

```
util  ←  core  ←  movegen  ←  position  ←  search  ←  uci
                                  ↑
                                eval
```

| Module | Responsibility | Notably *not* responsible for |
|---|---|---|
| `core/` | `Bitboard`, `Square`, `Piece`, `Color`, `CastlingRights`, `Move` | any chess *rules* logic |
| `movegen/` | attack tables, ray-walk sliding attacks, pseudo-legal + legal generation | knowing about search or eval |
| `position/` | board state, FEN in/out, make/unmake, Zobrist hashing | scoring or choosing moves |
| `eval/` | material-only static evaluation (v0) | anything search-dependent |
| `search/` | negamax alpha-beta, iterative deepening, transposition table, time budget | I/O of any kind |
| `uci/` | protocol loop, search thread ownership, output serialization | being depended on by anything |

A few properties worth calling out:

**Pseudo-legal generation + a legality filter**, rather than fully-legal generation up front.
The hot generation loop stays simple and independently benchmarkable; king-safety is confirmed
around `make_move`. This is the standard modern approach and is a deliberate, recorded decision
([REFERENCE.md §3.3](REFERENCE.md)).

**`Position` is a plain struct and the operations are free functions** —
`parse_fen(...)`, `make_move(pos, m, undo)`, `unmake_move(pos, m, undo)` — matching the style
`core`/`movegen` already established rather than introducing a second idiom mid-codebase.

**`make_move`/`unmake_move` restore state exactly**, including the incrementally-maintained
Zobrist key, backed by a round-trip property test across five structurally different positions.

**Search never writes to a stream.** `search::think` takes an `InfoCallback`; the UCI layer
supplies one that funnels every line through a single mutex. This is what makes concurrent
`info` output from the search thread and `readyok`/`bestmove` from the reader thread safe to
interleave without tearing a line — a hard requirement, since a GUI that receives a torn line
hangs.

**`go` runs on its own thread.** Before this, `stop` was a documented no-op and `go infinite`
was unusable, because the reader loop was blocked for the entire duration of a search. The
search thread observes an `std::atomic<bool>` checked once per node; a second `go` arriving
mid-search is *rejected, not queued*, and the reasoning for that choice is written down in
[ADR 0003](docs/adr/0003-async-search-stop.md). This is also the engine's first real
concurrency, so a ThreadSanitizer CI leg landed in the same change rather than later.

---

## Build & run

Requires CMake ≥ 3.20 and a C++20 compiler. Catch2 and Google Benchmark are fetched
automatically via `FetchContent`; there is nothing to install first.

```bash
git clone https://github.com/SilasTeague/Dahlia.git
cd Dahlia

cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

./build/debug/dahlia
```

### Presets

| Preset | Build type | Purpose |
|---|---|---|
| `debug` | `Debug` | day-to-day iteration; runs the full test suite |
| `debug-asan` | `Debug` + ASan/UBSan | memory and UB checking |
| `debug-tsan` | `Debug` + TSan | data races across the search/reader thread boundary |
| `release` | `RelWithDebInfo -O3` | portable release; what CI benchmarks and what ships |
| `release-native` | `RelWithDebInfo -O3 -march=native` | local benchmarking only, never shipped |

`release` stays deliberately portable — `-march=native` would make benchmark history
incomparable across machines and would produce a binary that crashes on the deployment host.

### Makefile wrapper

The `Makefile` carries no build logic; it delegates to the presets purely as muscle memory
([ADR 0002](docs/adr/0002-cmake-migration.md)):

```bash
make              # configure + build (PRESET=debug)
make test         # ctest
make run          # launch the engine
make PRESET=release build
```

### Playing a game

Dahlia speaks UCI, so it loads into Arena, Cute Chess, BanksiaGUI, or `cutechess-cli`. To drive
it by hand:

```
$ ./build/release/dahlia
uci
id name Dahlia
id author Silas Teague
option name Hash type spin default 16 min 1 max 1024
uciok
position startpos moves e2e4 e7e5
go movetime 1000
info depth 1 score cp 0 nodes 30 nps 30000 time 0
info depth 2 score cp 0 nodes 145 nps 145000 time 0
info depth 3 score cp 100 nodes 1963 nps 1963000 time 1
info depth 4 score cp -100 nodes 37931 nps 3160916 time 12
info depth 5 score cp 100 nodes 87385 nps 4161190 time 21
info depth 6 score cp -100 nodes 545166 nps 5925717 time 92
info depth 7 score cp 100 nodes 2542256 nps 10637054 time 239
bestmove g1f3
```

The score alternating between `+100` and `-100` across depths is the horizon effect made
visible: with a material-only evaluation and no quiescence search, an unresolved capture
sequence scores as a won or lost pawn depending purely on which side happens to move last at
the leaf. Quiescence search is what fixes this
([see limitations](#current-limitations)).

---

## UCI support

| Command | Status |
|---|---|
| `uci`, `isready`, `ucinewgame`, `quit` | ✅ — `isready` answers immediately even mid-search |
| `position startpos \| fen <fen> [moves ...]` | ✅ |
| `go depth \| movetime \| wtime \| btime \| winc \| binc \| movestogo \| infinite` | ✅ |
| `stop` | ✅ — returns the best move from the last completed depth |
| `setoption name Hash value <MB>` | ✅ — 1–1024 MB, default 16 |
| `info` lines | partial — `depth`, `score cp`/`score mate`, `nodes`, `nps`, `time`; no `seldepth`, `pv`, or `hashfull` yet |
| `go nodes`, `searchmoves`, `ponder`, `Threads` | not implemented (unrecognized tokens are ignored, not errors) |

Time management converts the `go` clock parameters into a single-move budget
(`time_left / movestogo + increment`, capped at half the remaining clock), with `movestogo`
defaulting to 30 when the GUI doesn't supply it. A bare `go` with no time control at all gets a
200 ms anytime budget rather than searching forever.

---

## Testing

Chess engines fail in ways that are easy to miss and cheap to catch, so the test strategy leans
hard on oracles rather than on assertions written by the same person who wrote the bug.

### Perft — the movegen correctness oracle

Node counts are checked against the standard
[Chess Programming Wiki reference values](https://www.chessprogramming.org/Perft_Results).
Every depth is *also* checked as a breakdown by which piece type made the root move, which
catches a class of bug the total alone would hide (e.g. pawn and knight moves miscounted in
offsetting directions).

| Position | Depth | Nodes | Runs in CI |
|---|---:|---:|:--:|
| Start position | 1 | 20 | ✅ |
| Start position | 2 | 400 | ✅ |
| Start position | 3 | 8,902 | ✅ |
| Start position | 4 | 197,281 | ✅ |
| Start position | 5 | 4,865,609 | manual |
| Start position | 6 | 119,060,324 | manual |
| Kiwipete | 1 | 48 | ✅ |
| Kiwipete | 2 | 2,039 | ✅ |
| Kiwipete | 3 | 97,862 | ✅ |
| Kiwipete | 4 | 4,085,603 | ✅ |
| Kiwipete | 5 | 193,690,690 | manual |

All values match exactly. The deep runs are tagged Catch2-hidden (`[.]`) to stay inside a CI
time budget; run them explicitly with:

```bash
./build/release/tests/perft/dahlia_perft_tests "[perft]"
```

### Unit and regression tests

34 Catch2 test cases across seven files:

- **FEN** — parse, serialize, and round-trip, including partial castling rights and en passant.
- **make/unmake** — a round-trip property test asserting the position *and* Zobrist key are
  restored bit-for-bit, run over the start position, Kiwipete, an endgame, a
  castling/promotion-rich position, and an en-passant-available position.
- **Evaluation** — symmetry (`evaluate(pos) == -evaluate(pos_with_side_flipped)`), guarding the
  sign-error bug class that is notoriously hard to notice by playing.
- **Search** — finds a back-rank mate in one, wins a free rook, and declines a poisoned pawn
  that a depth-1 search would grab.
- **Transposition table** — probe/store round-trip, verified-key misses, depth-preferred
  replacement, and generation-based aging.
- **UCI protocol** — scripted command sequences over an injected `istream`/`ostream`, including
  the concurrency cases: `isready` answered while a search is running, `go infinite` terminated
  by `stop` with exactly one `bestmove`, `quit` mid-search leaking no thread, and a second `go`
  being rejected rather than queued.

### CI

Every push and PR runs:

- **Build + test matrix** — `{ubuntu, macos} × {gcc, clang} × {Debug, Release}`, minus the
  macOS/GCC leg (Apple Clang already provides a second distinct compiler there).
- **ASan + UBSan** leg, running the full suite.
- **TSan** leg, running the UCI protocol tests specifically because they exercise the
  search-thread / reader-thread boundary.

---

## Benchmarks

Benchmarking is treated as architecture, not as something bolted on when the engine feels slow.
Two independent layers exist, because they catch different failures:

- **Microbenchmarks** (`bench/microbench`, Google Benchmark) isolate one hot function.
- **Macrobenchmarks** (`bench/search_bench`) run the whole engine to a fixed depth on a fixed
  position set — the only thing that catches a function getting faster in isolation while the
  engine gets slower from cache pressure introduced elsewhere.

Every run is committed as a schema'd JSON file under
[`bench/results/history/`](bench/results/history), named `<date>-<short-hash>.json`, whether or
not it becomes an approved baseline. The point is an unedited, plottable record of the engine's
whole lifetime, not a single current number.

```bash
scripts/run_benchmarks.sh      # builds Release, runs both suites, writes a history file
```

**Metric of record: nodes to reach depth N**, not wall time. Node count is what isolates a
search-quality improvement from the speed of whatever machine happened to run it.

### The transposition table (Milestone 4)

Fixed depth 5, same machine (Apple Silicon), same build (`RelWithDebInfo -O3`), before
([`acf4aac`](bench/results/history/2026-07-31-acf4aac.json)) and after
([`5dab191`](bench/results/history/2026-08-03-5dab191.json)):

| Position | Nodes before | Nodes after | Δ nodes | Time before | Time after |
|---|---:|---:|---:|---:|---:|
| Opening (start position) | 38,489 | 34,195 | **−11.2%** | 2.62 ms | 2.63 ms |
| Middlegame (Kiwipete) | 505,339 | 145,195 | **−71.3%** | 60.54 ms | 15.26 ms |
| Endgame (K+P) | 984 | 820 | **−16.7%** | 0.13 ms | 0.34 ms |

The middlegame result is the one that matters: Kiwipete is dense with transpositions, and
cutting the tree by 71% cut wall time by 75%. The opening position transposes far less at depth
5, so it gains much less — exactly the shape you'd expect, which is itself a sanity check on the
result.

The endgame position got *slower* in wall time while searching fewer nodes. That is not a
regression in search: an 820-node search is dominated by allocating and zeroing the 16 MB table,
which the benchmark does once per iteration. It's recorded here rather than quietly dropped,
because a benchmark suite you only cite when it agrees with you isn't a benchmark suite.

### Movegen baseline

The loop/bit-shift ray walker, unchanged since Milestone 1 — this is the number a future
magic-bitboard implementation gets measured against:

| Benchmark | 2026-07-26 | 2026-08-03 |
|---|---:|---:|
| `BM_RookAttacksLookup` | 8.67 ns | 8.30 ns |
| `BM_BishopAttacksLookup` | 5.42 ns | 4.93 ns |
| `BM_QueenAttacksLookup` | 14.52 ns | 13.57 ns |
| `BM_GeneratePseudoLegalMoves_StartPosition` | 81.20 ns | 76.28 ns |

No movegen optimization has been attempted yet, so these deltas are run-to-run drift on
identical code — mostly inside the ±5% band the framework treats as noise. They are listed to
establish the baseline, not to claim a win.

> **Note on the `nodes_per_second` field** in the history JSON: it comes from a Google Benchmark
> rate counter and is not directly comparable to a UCI `nps` figure. Node counts and wall times
> are the trustworthy fields, and node count is the one decisions are made on.

---

## Milestones

The roadmap is defined in full in [REFERENCE.md Part V](REFERENCE.md). Every milestone must
leave `master` in a buildable, UCI-playable state.

| # | Milestone | Status |
|---|---|---|
| 0 | **Project scaffolding** — CMake + presets, CI skeleton, Catch2, reference doc | ✅ Complete |
| 1 | **Core types, `Move`, move generation** — attack tables, ray-walk sliding attacks, perft-verified | ✅ Complete |
| 2 | **`Position`, make/unmake, Zobrist, minimal UCI** — legal game playable in a real GUI | ✅ Complete |
| 3 | **Material eval + alpha-beta** — negamax, iterative deepening, time management | ✅ Complete |
| 4 | **Move ordering, quiescence, TT** | 🚧 In progress — TT and TT-move ordering landed; MVV-LVA, killers, history, and quiescence remain |
| 5 | **PVS, piece-square tables / tapered eval, null-move pruning** | ⬜ Not started |
| 6 | **Aspiration windows, late move reductions** | ⬜ Not started |
| 7 | **Polish & portfolio packaging** — architecture diagrams, full option set, strength estimate | 🚧 Partial — this README, the live deployment, and the release pipeline are done |
| 8+ | **Lazy-SMP search**, opening book, evaluation tuning harness | ⬜ Committed stretch goals |

Explicitly out of scope for now: NNUE evaluation, Syzygy tablebases, and PEXT-based attack
lookup — each is a large scope jump that a well-documented classical engine doesn't need.

Two things shipped that the original roadmap didn't anticipate, both driven by real need rather
than by the plan:

- **Asynchronous search and `stop`** ([ADR 0003](docs/adr/0003-async-search-stop.md)), which
  pulled the ThreadSanitizer CI leg forward from the SMP milestone to now.
- **Static Linux release binaries and live deployment**, which is what makes
  [silasteague.com/chess](https://silasteague.com/chess) possible.

---

## Design decisions

Contested decisions are recorded rather than re-litigated. Full ADRs live in
[`docs/adr/`](docs/adr); the complete index is in
[REFERENCE.md Appendix C](REFERENCE.md).

| Decision | Rationale |
|---|---|
| [Pseudo-legal movegen + legality filter](REFERENCE.md) | Keeps the hot generation loop simple and benchmarkable in isolation; cost is slightly more complex make/unmake |
| [Magic bitboards deferred past Milestone 1](REFERENCE.md) | Correctness → Measurement → Optimization: the loop-based baseline is what makes the later swap a citable win instead of a vibes-based one |
| [CMake as the build system of record](docs/adr/0002-cmake-migration.md) | Library/executable/test/bench target separation, sanitizers, `FetchContent`; the Makefile survives only as a thin wrapper with no logic of its own |
| [Async search; concurrent `go` rejected, not queued](docs/adr/0003-async-search-stop.md) | Queueing means answering "what does a *third* `go` do", for a case no compliant GUI produces |
| [Benchmarks triggered by changed paths, not a commit tag](REFERENCE.md) | A forgotten tag is a silent hole in the regression history; path filtering fires deterministically |
| [`Move` stays a plain struct](REFERENCE.md) | Bit-packing is deferred until a benchmark shows move-list/TT cache pressure actually matters |
| [Feature branch + PR for every change](REFERENCE.md) | The PR description is where design rationale and before/after numbers live |

---

## Release & deployment

Pushing a `v*` tag runs [`release.yml`](.github/workflows/release.yml), which:

1. Gates on the unit + perft suite.
2. Builds **statically linked** binaries for `linux-x64` and `linux-arm64` — natively on each
   architecture, no emulation — using
   [the same script a laptop build uses](scripts/build-release.sh), so CI cannot drift from
   local output.
3. Smoke-tests the exact artifact that ships inside the build container: a full UCI handshake
   plus a real search from a real position, asserting a well-formed `bestmove` rather than
   `bestmove 0000`. A binary that starts and then finds no move would sail past a bare `uciok`
   check.
4. Publishes the binaries with a `SHA256SUMS` manifest.

Static linking is load-bearing, not incidental: deployment hosts span glibc 2.31 through 2.36,
and a dynamically linked build simply refuses to start on anything older than the build image.

The website's instance polls the checksum manifest and pulls new binaries itself. Nothing in
this repository holds credentials for, or pushes to, a server.

**Current release: [`v2.0`](https://github.com/SilasTeague/Dahlia/releases/tag/v2.0)** — static
Linux release binaries.

---

## Current limitations

Stated plainly, because a portfolio README that only lists strengths isn't an engineering
document:

- **Evaluation is material-only.** No piece-square tables, no mobility, no king safety. The
  engine counts wood and nothing else, so its positional play is weak by construction.
- **No quiescence search**, so the engine is subject to the horizon effect: a capture sequence
  that resolves one ply past the search depth is scored as though it never happened.
- **Move ordering is TT-move-only.** No MVV-LVA, killers, or history heuristic yet, which is the
  single largest available reduction in tree size.
- **No strength estimate.** No SPRT match has been run, so Dahlia has no Elo figure and this
  README deliberately doesn't invent one. SPRT tooling lands with Milestone 4's completion.
- **`benchmark.yml` is an unimplemented placeholder.** Benchmarks are currently run locally via
  `scripts/run_benchmarks.sh` and committed by hand; the path-triggered CI workflow is designed
  ([REFERENCE.md §3.11](REFERENCE.md)) but not yet written.
- **`docs/architecture.md` doesn't exist yet** — it's a Milestone 7 deliverable. Until then,
  [`REFERENCE.md`](REFERENCE.md) is the architecture document.
- **`info` lines omit `seldepth`, `pv`, and `hashfull`**, all of which a GUI will display if
  given and silently skip if not.

---

## Repository layout

```
Dahlia/
├── REFERENCE.md            # the specification: architecture, conventions, roadmap, metrics
├── CMakeLists.txt          # dahlia_core library + dahlia executable + test/bench targets
├── CMakePresets.json       # debug, debug-asan, debug-tsan, release, release-native
├── Makefile                # thin wrapper over the presets, no logic of its own
├── src/
│   ├── core/               # types.h, move.h
│   ├── movegen/            # attacks, ray-walk sliding attacks, generation
│   ├── position/           # Position, FEN, make/unmake, Zobrist
│   ├── eval/               # material-only evaluation
│   ├── search/             # negamax + iterative deepening, TT, time budget
│   ├── uci/                # protocol loop, search thread, output serialization
│   └── main.cpp
├── tests/
│   ├── unit/               # Catch2: FEN, make/unmake, eval, search, TT, UCI
│   └── perft/              # perft + per-piece divide against CPW reference values
├── bench/
│   ├── microbench/         # Google Benchmark: attacks, move generation
│   ├── search_bench/       # fixed-depth whole-engine search on a fixed position set
│   └── results/history/    # committed JSON, one file per benchmark run
├── docs/adr/               # architecture decision records
├── scripts/                # run_benchmarks.sh, build-release.sh, format_bench_result.py
├── docker/                 # Dockerfile.release — static build + shipped-artifact smoke test
└── .github/workflows/      # ci.yml, release.yml, benchmark.yml
```

If you're reading the codebase for the first time, the path with the most signal is:
[`REFERENCE.md §1.1`](REFERENCE.md) for the module graph → [`docs/adr/`](docs/adr) for the
decisions that were actually contested → [`src/search/search.cpp`](src/search/search.cpp) for
where the interesting work happens.

---

## License

MIT. See [REFERENCE.md Appendix C.1](REFERENCE.md).

---

<p align="center">
  <a href="https://silasteague.com/chess"><b>Play Dahlia →</b></a>
</p>
