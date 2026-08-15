# Dahlia

[![CI](https://github.com/SilasTeague/Dahlia/actions/workflows/ci.yml/badge.svg)](https://github.com/SilasTeague/Dahlia/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](#license)

A UCI-compatible chess engine written from scratch in modern C++ — bitboard move generation,
Zobrist-hashed make/unmake, an iterative-deepening principal-variation search with a transposition
table, tapered evaluation, null-move pruning, aspiration windows and late move reductions, and an
asynchronous UCI loop that stays responsive mid-search.

**Rated 1928 blitz on Lichess** as of 2026-08-15, over 64 games (35 W / 6 D / 23 L). That is a
rating still in motion over a small sample rather than a converged one, and it is quoted with its
date and game count for exactly that reason — see
[current limitations](#current-limitations).

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
- Move ordering was added one heuristic at a time — MVV-LVA, then killers, then history — each
  measured on its own rather than landed as a single "move ordering" commit whose parts can't be
  told apart. ([See the numbers.](#move-ordering-and-quiescence-milestone-4))

Every performance claim in this README is reproducible: the depth-5 numbers come from committed
JSON under [`bench/results/history/`](bench/results/history), and the milestone comparisons at
depth 7 or a fixed five seconds are fixed-depth and fixed-time searches of the same four pinned
positions, run against the named commits. Nothing here is quoted from a position or a build the
repository doesn't contain — a rule this README broke once and now
[documents](#move-ordering-and-quiescence-milestone-4).

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
| `eval/` | material + tapered piece-square tables (v1) | anything search-dependent |
| `search/` | negamax alpha-beta with PVS, iterative deepening, transposition table, move ordering, quiescence, null-move pruning, aspiration windows, late move reductions, time budget | I/O of any kind |
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

**Every inexact technique has an off switch.** `search::SearchTuning` gates late move reductions,
null-move pruning, quiescence's delta pruning, aspiration windows, and the transposition table's
score cutoffs, and `SearchTuning::exact()` turns all of them off at once. Nothing in `src/` ever
sets them and no UCI command reaches them — they exist so that "does this technique change the
answer?" is a question the *test suite* can ask, rather than one answered by rebuilding the engine
twice by hand and comparing, which is how Milestone 5 had to do it
([ADR 0006](docs/adr/0006-aspiration-lmr-constants.md)).

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
| `release` | `RelWithDebInfo` | portable release; what CI benchmarks and what ships |
| `release-native` | `RelWithDebInfo -march=native` | local benchmarking only, never shipped |

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
visible: with the material-only evaluation of the day and no quiescence search, an unresolved capture
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
| `setoption name Move Overhead value <ms>` | ✅ — 0–1000 ms, default 10 |
| `info` lines | partial — `depth`, `score cp`/`score mate`, `nodes`, `nps`, `time`; no `seldepth`, `pv`, or `hashfull` yet |
| `go nodes`, `searchmoves`, `ponder`, `Threads` | not implemented (unrecognized tokens are ignored, not errors) |

Time management converts the `go` clock parameters into a single-move budget
(`time_left / 20 + increment / 2`, capped at half the remaining clock). `movestogo`, when the GUI
supplies it, only tightens the divisor — below 20 moves to the next time control the budget becomes
`time_left / movestogo + increment / 2` so the clock isn't left unspent at the control. A bare `go`
with no time control at all gets a 200 ms anytime budget rather than searching forever.

Every GUI-imposed budget — `movetime` included — is then reduced by `Move Overhead` (default 10 ms)
so the reply lands *before* the deadline rather than exactly on it. Everything after the search
returns happens on the GUI's clock, and a GUI configured with a zero time margin (cutechess-cli's
default `timemargin`) scores an on-the-deadline reply as a forfeit. Raise the option for high-latency
transports; the reserve never shrinks a budget below 1 ms.

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

90 Catch2 test cases across 15 files:

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
- **Re-search correctness** ([`test_research.cpp`](tests/unit/test_research.cpp)) — the
  Milestone 6 techniques against a "safe mode" search in the same binary
  (`SearchTuning::exact()`, every inexact technique switched off). Two claims, deliberately
  different in strength: aspiration windows must return the *identical* score a full-window
  search would, while LMR must merely never promote a move on reduced-depth evidence — a mate,
  and a free piece, must both survive the reductions. Writing the second as an equality test
  would have been asserting something LMR doesn't promise.
- **Node counts and conclusions** ([`test_search_nodes.cpp`](tests/unit/test_search_nodes.cpp)) —
  a golden-file table of nodes, score, and best move at a fixed depth, split into two test cases
  so that a search which got *cheaper* and a search which changed its *mind* fail separately.
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

Every run is recorded as a schema'd JSON file under
[`bench/results/history/`](bench/results/history), named `<date>-<short-hash>.json`, and
committed alongside the change it measures. The point is an unedited, plottable record of the
engine's whole lifetime, not a single current number.

```bash
scripts/run_benchmarks.sh      # build, run both suites, record a history file, print the deltas
```

One command does the whole loop, so the workflow is: make a change, run the script, read the
numbers, commit the result file with the change. This is its real output for the commit that
introduced the transposition table:

```
### 2026-07-31 `acf4aac`  →  2026-08-03 `5dab191`

| Benchmark              | Nodes   | Δ      | Time      | Δ      |      |
|------------------------|---------|--------|-----------|--------|------|
| `BM_Search_Opening`    |  34,195 | -11.2% | 2.631 ms  |  +0.5% | ✅   |
| `BM_Search_Middlegame` | 145,195 | -71.3% | 15.26 ms  | -74.8% | ✅   |
| `BM_Search_Endgame`    |     820 | -16.7% | 0.3449 ms | +167.9%| ✅ ⚠️ |
```

The endgame row is why both columns are shown: fewer nodes, more wall time. To see the whole arc
rather than the last step:

```bash
scripts/compare_bench_results.py --history
scripts/compare_bench_results.py --history --benchmark Middlegame
```

**Timing deliberately isn't in CI.** Wall-clock numbers are only comparable within one machine,
and a GitHub runner label pins an image, not a CPU — so automating them would buy an unreliable
signal that trains you to ignore it. Instead the suite runs on one machine, records the CPU,
compiler, flags, and repetition count in every file, and warns if any of those move between
runs. Each benchmark runs multiple repetitions and the **median** is what gets recorded.

What *is* automated is the half that can be: see
[node counts as a test](#node-counts-are-a-test-not-a-benchmark) below.

**Metric of record: nodes to reach depth N**, not wall time. Node count is what isolates a
search-quality improvement from the speed of whatever machine happened to run it.

### Node counts are a test, not a benchmark

A fixed-depth search with no time limit visits an *identical* number of nodes on any machine,
compiler, or optimization level — verified by reproducing 34,195 / 145,195 / 820 across Debug
and release builds and separate runs days apart. That makes node count an assertion rather than
a measurement, so it lives in the test suite
([`test_search_nodes.cpp`](tests/unit/test_search_nodes.cpp)) with the expected values in a
checked-in table, running on every push with no tolerance band and no statistics.

An intentional search improvement *fails* that test, and the fix is to update the table in the
same PR — so the diff on those values becomes the regression report, showing exactly what the
change did to the tree, reviewable in place. It's the same pattern as the perft reference
values, for the same reason ([ADR 0004](docs/adr/0004-node-counts-in-ci-timing-local.md)).

The table pins the score and best move alongside the node count, in a *separate* test case, so
that a search which got cheaper and a search which changed its mind fail apart from each other.
Since Milestone 6 the engine contains one technique that is allowed to do the second — late move
reductions — so that half of the table is no longer expected to hold across every search change.
It still makes a changed conclusion impossible to land by accident, which is what it was for.

Those counts are specified at a fixed 16 MB hash: node count legitimately falls as the
transposition table grows (164,722 nodes at 1 MB down to 161,277 at 16 MB, flat from there to
256 MB as replacement stops thrashing), so the hash size is part of the specification. What the
test asserts across hash sizes instead is the **score and best move** — which is also the first
thing that would break if the TT ever returned an entry belonging to a different position.

### The transposition table (Milestone 4)

Fixed depth 5, same machine (Apple Silicon), same build (`RelWithDebInfo`, `-O2 -g`), before
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

The endgame position got *slower* in wall time while searching fewer nodes. That was never a
regression in search — it was the benchmark measuring the wrong thing: an 820-node search was
dominated by allocating and zeroing the 16 MB table, which the harness then did once per
iteration inside the timed region. That's since been
[fixed](#a-note-on-the-numbers-before-2026-08-14), which drops the endgame's measured time from
0.345 ms to 0.103 ms. The anomaly is left in the table rather than quietly dropped, because a
benchmark suite you only cite when it agrees with you isn't a benchmark suite.

### Move ordering and quiescence (Milestone 4)

Alpha-beta's node count is decided almost entirely by how early the best move gets searched, so
the four Milestone 4 changes were built and measured **one at a time**. Same machine, same
build, **nodes to reach depth 7** — cumulative across the iterative-deepening iterations, which
is the search's real cost:

| After adding | Opening | Middlegame (Kiwipete) | Endgame (K+P) |
|---|---:|---:|---:|
| *(Milestone 3 baseline)* | 828,549 | 4,866,267 | 3,739 |
| MVV-LVA capture ordering | 502,967 | 1,394,445 | 3,754 |
| \+ killer moves | 371,750 | 1,392,651 | 4,028 |
| \+ history heuristic | 345,941 | 1,384,999 | 4,046 |
| \+ quiescence search | 370,254 | 2,431,525 | 6,349 |
| **Net change** | **−55.3%** | **−50.0%** | **+69.8%** |

Reading this honestly matters more than the headline number:

- **MVV-LVA did most of the work.** Ordering captures by what they win, before what they risk,
  cut Kiwipete by 71% on its own. Nothing else in the milestone comes close.
- **Killers and history helped where MVV-LVA couldn't.** Both only rank *quiet* moves, so their
  gains land in the opening position (down another 31%), which has few captures to order, and
  are nearly invisible on Kiwipete, where MVV-LVA had already found the cutoffs.
- **The K+P endgame got worse at every step, and that's expected.** It has no captures to order
  and almost no quiets worth remembering, so ordering adds bookkeeping and returns nothing.
  A position with nothing to order is where ordering costs you.
- **Quiescence *raises* node counts and is still the most valuable change here.** It adds a
  capture-resolving search at every leaf, so "nodes to depth 7" now buys strictly more than it
  used to. Judging it by node count alone would be measuring the wrong thing — see below.

The metric that shows what quiescence bought is **depth reached in a fixed 5 seconds**:

| Position | Milestone 3 | Milestone 4 | Nodes at M3's depth |
|---|---:|---:|---|
| Opening | 9 | **10** | 15,347,198 → 5,957,927 |
| Middlegame (Kiwipete) | 7 | **8** | 4,866,267 → 2,431,525 |
| Endgame (K+P) | 25 | **27** | — |
| Tactical (WAC.019) | 7 | **8** | 34,941,856 → 3,523,284 |

**A correction to the tactical numbers, made at Milestone 5.** The two tables above originally
carried a fourth position labelled "Tactical" whose FEN was never committed — not to
`bench_search.cpp`, not to the tests, not anywhere in the history. That made it the one
performance claim in this README that couldn't be reproduced, which is exactly the failure mode
the benchmark history exists to prevent. The fix was to pin a tactical position for good
([`BM_Search_Tactical`](bench/search_bench/bench_search.cpp) — WAC.019, the same position the
tactics suite already uses, filling the tactical slot
[REFERENCE.md §3.11](REFERENCE.md) asks the position set to cover) and re-measure it against the
two commits that bracket Milestone 4: [`098d366`](https://github.com/SilasTeague/Dahlia/commit/098d366)
and [`030d518`](https://github.com/SilasTeague/Dahlia/commit/030d518). Those are the numbers in
the table above, and in this one:

| Tactical (WAC.019) | Milestone 3 `098d366` | Milestone 4 `030d518` | Δ |
|---|---:|---:|---:|
| Nodes to depth 7 | 34,941,856 | 3,523,284 | **−89.9%** |
| Nodes to depth 5 | 458,327 | 151,544 | **−66.9%** |
| Score at depth 7 | +370 | +270 | — |

The per-step breakdown can't be recovered for this position: the four ordering changes share a
single commit, so the intermediate states were measured but never committed, and only the
endpoints can be re-run. The score column is worth its own line, though — the drop from +370 to
+270 isn't a regression, it's quiescence declining a capture sequence the pre-quiescence search
scored as winning material at the horizon.

And the behaviour it fixes is directly observable. On `4k3/8/2p1p3/3p4/8/8/8/3QK3 w - -`, where
the d5 pawn is defended twice, a depth-1 search before Milestone 4 played **Qxd5 and scored it
+700** — it watched the pawn come off and stopped looking one ply before `cxd5`. The same search
today declines the capture and scores the position +600, its true material value. That position
is checked in as a regression test
([`test_quiescence.cpp`](tests/unit/test_quiescence.cpp)).

On the [Win At Chess](tests/unit/test_tactics.cpp) subset checked into the test suite, the
engine went from **13/18 to 14/18** solved at depth 7. The modest jump is the honest result:
the four it still misses are not search failures. They need an evaluation that understands pawn
structure and king safety, and they fail identically at one second per move as at depth 7 —
which makes them Milestone 5's problem, not Milestone 4's. They're checked in as a documented
known-failure list rather than omitted.

One implementation note worth recording, because it cost a 2× slowdown before it was caught:
quiescence generates **pseudo-legal** moves and proves legality using the `make_move` it already
performs for the recursion. Calling `generate_legal_moves` instead — which make/unmakes every
move to filter it — meant paying full legality checks on ~35 quiet moves in order to search 4
captures, and it dropped middlegame throughput from 5.4M to 2.5M nodes/sec
([ADR 0005](docs/adr/0005-quiescence-pseudo-legal-movegen.md)).

### Piece-square tables and tapered eval (Milestone 5)

Until this change the engine counted material and nothing else, and the tactics suite had
recorded four positions it couldn't solve for exactly that reason. The evaluation now scores
*where* each piece stands as well as what it is worth, twice — once for a full board and once for
an endgame — and blends the two by how much material is left
([`evaluate.cpp`](src/eval/evaluate.cpp)).

The headline result is the one the known-failure list was checked in to make falsifiable:

| | Milestone 4 | Milestone 5 |
|---|---:|---:|
| [Win At Chess](tests/unit/test_tactics.cpp) subset solved at depth 7 | 14/18 | **15/18** |

WAC.022 moved from the known-failure list to the solved list. It's a pawn endgame where
`...Nxg4+` wins a pawn and centralizes the king — a material-only evaluation scored the result
level, because everything the move actually gains was invisible to it. The other three still
fail, and the comment above the list now says why: they turn on king safety and the bishop pair,
which piece-square tables don't reach. A table scores where a piece stands, not what it's doing.

**It cost speed, and the numbers are worth stating plainly rather than burying:**

| | Opening | Middlegame | Endgame | Tactical |
|---|---:|---:|---:|---:|
| Nodes to depth 5 | +17.1% | +19.8% | +1.1% | +5.4% |
| Nodes/sec | −32.5% | −25.6% | −11.1% | — |
| Depth reached in 5 s | 10 → **9** | 8 → 8 | 27 → **25** | 8 → 8 |

Two separate effects, and it's worth not conflating them:

- **Node counts went up** because a positional evaluation returns far fewer *equal* scores than a
  material-only one did. Ties are cheap for alpha-beta — sibling moves that score identically cut
  off immediately — and a table that distinguishes a knight on d4 from one on a1 stops handing
  them out.
- **Nodes/sec went down** because `evaluate()` stopped being ten popcounts and became a walk over
  every piece on the board with two table lookups each — **34.2 ns** on a full board against
  **9.1 ns** on a five-piece endgame, now tracked as
  [`BM_Evaluate_*`](bench/microbench/bench_eval.cpp). At roughly 110 ns per node, that's about a
  third of the engine's time spent in evaluation, and the endgame's smaller loss lines up exactly
  with its smaller board.

Together those cost a ply of depth in the opening and two in the K+P endgame. That is a real
regression in search depth, accepted deliberately and not permanently: the fix is incremental
evaluation — updating a running score inside `make_move`/`unmake_move` instead of recomputing
from scratch at every node — which [REFERENCE.md §3.6](REFERENCE.md) has listed as a future
extension all along, gated on "once profiling shows eval cost matters". It now does, and the
microbenchmark that will justify it exists as of this change. The remaining Milestone 5 work
(PVS, null-move pruning) attacks the same deficit from the other end by shrinking the tree.

### Principal Variation Search (Milestone 5)

PVS is a bet on move ordering: search the first move at each node with a full window, and every
move after it with a null window one point wide, which asks only "is this better than what we
already have?" — a question that fails high or low almost immediately. When the first move really
is best, the rest of the node is answered cheaply. When it isn't, the scout has to be repeated
with the real window, and the bet loses.

So the bet was checked before it was placed. Instrumenting the search to count *where* beta
cutoffs happen:

| Position | Cutoffs on the first move tried |
|---|---:|
| Opening | 84.3% |
| Middlegame (Kiwipete) | 95.5% |
| Endgame (K+P) | 92.7% |
| Tactical (WAC.019) | 92.7% |

That's a search whose first guess is right five times in six at worst — Milestone 4's ordering
work is what paid for it. The result, nodes to reach a fixed depth:

| Position | Depth 5 | Δ | Depth 7 | Δ |
|---|---:|---:|---:|---:|
| Opening | 26,959 → 26,183 | −2.9% | 533,650 → 488,704 | −8.4% |
| Middlegame (Kiwipete) | 193,194 → 152,499 | **−21.1%** | 2,764,317 → 2,393,395 | −13.4% |
| Endgame (K+P) | 1,427 → 1,330 | −6.8% | 7,043 → 6,437 | −8.6% |
| Tactical (WAC.019) | 159,796 → 156,240 | −2.2% | 4,097,425 → 3,821,576 | −6.7% |

**Every score and best move is unchanged**, which is the entire claim PVS makes — a smaller tree,
the same answer — and it's now pinned in
[`test_search_nodes.cpp`](tests/unit/test_search_nodes.cpp) as a table of scores and moves
alongside the node counts, so a future change that quietly starts searching *differently* rather
than *less* fails a test instead of passing unnoticed.

Verifying that took an experiment, because scores in the K+P endgame did shift at deep fixed
depths (+268 vs +980 at depth 18). Rebuilding both versions with the transposition table's score
cutoffs disabled — leaving it as a move-ordering hint only — made the scores identical at every
depth and position tested. So PVS is exact, and what moves the numbers is the table: a
null-window search stores a *bound* where a full-window search stored an exact score, and a later
probe that gets a bound where it used to get an exact value has to search rather than return.

**Which is also why the K+P endgame got worse, and it's the one position that did:**

| Endgame (K+P), nodes to depth | 15 | 18 | 20 | 22 |
|---|---:|---:|---:|---:|
| Before PVS | 219,339 | 620,528 | 1,178,279 | 2,531,519 |
| After PVS | 278,001 | 655,497 | 1,214,204 | 5,166,104 |

Depth reached in a fixed 5 seconds fell from 25 to 23 there (reproducibly — three runs, identical
node counts). It is not re-search overhead: the instrumented build puts the re-search rate at
**0.01–0.46%** of scouts across every position and depth measured. It's the TT effect above,
landing hardest on the position that leans on the table most — five pieces, and nearly every line
transposing into every other.

Two fixes were tried and both rejected on the numbers:

- **Skipping the scout near the leaves** (full window at depth ≤ 1, 2, or 3). Changed the three
  normal positions by under 0.5%, and in the endgame swung *both* ways with the threshold
  (depth ≤ 2 halved nodes at depth 22 but cost 7% at depth 20; depth ≤ 3 was worse at both). That
  is one position's noise, not a signal, and tuning a constant on it would be overfitting.
- **Preferring exact entries over bounds when replacing a TT slot at equal depth.** Zero change,
  to the node, on all six measurements — the case is rarer than the theory suggests.

So textbook PVS ships, with the regression documented rather than tuned away. The honest summary
is that PVS trades a large middlegame gain for a small endgame loss, which is the same shape as
Milestone 4's ordering result and the same reason: a position with few moves and no captures is
where every ordering-dependent optimization has the least to work with.

### Null-move pruning (Milestone 5)

The idea is almost impudent: hand the opponent a free move, search the result two plies shallower
than normal, and if the position is *still* winning, don't bother generating the real move list at
all. Nearly every position obliges, because passing is worse than any legal move — nearly.

| Position | Nodes to depth 5 | Δ | Nodes to depth 7 | Δ | Depth in 5 s |
|---|---:|---:|---:|---:|---:|
| Opening | 26,183 → 21,337 | −18.5% | 488,704 → 287,123 | −41.2% | 9 → **11** |
| Middlegame (Kiwipete) | 152,499 → 136,030 | −10.8% | 2,393,395 → 1,129,624 | −52.8% | 8 → **10** |
| Endgame (K+P) | 1,330 → 1,330 | **0.0%** | 6,437 → 6,437 | **0.0%** | 23 → **24** |
| Tactical (WAC.019) | 156,240 → 25,809 | **−83.5%** | 3,821,576 → 300,109 | **−92.1%** | 8 → **11** |

Scores and best moves are unchanged on all four. Two of these rows are worth reading closely:

**The tactical position lost 92% of its tree** because it is winning by roughly three pawns, and
that is exactly the shape null-move pruning is built for: in a position this far above beta, most
subtrees can be dismissed without being searched. It is the largest single-change node reduction
in the project's history.

**The endgame moved by exactly zero nodes, and that is the feature working**, not a bug. It is a
king and a pawn per side, so the zugzwang guard switches the heuristic off completely. Which is
the whole point:

> Null-move pruning assumes passing is worse than any real move. In a pawn endgame that
> assumption is backwards — pawns can't move backwards, and positions where every legal move
> loses ground are the entire subject of endgame theory.

Rebuilding the engine with the guard removed and nothing else changed, that same K+P position at
depth 17 scores **+219 instead of +962** and never finds the promotion. That's a 700-centipawn
misjudgement of a won game, and it's now pinned as a regression test
([`test_nullmove.cpp`](tests/unit/test_nullmove.cpp)) with the failing number written into the
comment, so the test says what it's protecting against rather than just asserting a magic
threshold.

The endgame also gained a ply in fixed time (23 → 24) despite the guard, which looks contradictory
until you notice where: once a pawn promotes, the side to move *has* a queen, the guard stops
applying, and the pruning switches itself back on for the rest of the line.

Null moves are never searched two in a row, never while in check (passing there would leave a
capturable king), and never when beta is already a mate score — near a forced mate, "I'm winning
even if I do nothing" is a statement about beta, not about the position. One case is knowingly
unhandled: the pruning runs before the move list is generated, so a *stalemate* is treated as a
position where the side to move could happily pass. The guard covers the common case, and
detecting the rest would mean generating the move list, which is the cost the whole heuristic
exists to avoid.

### Milestone 5 end to end

Three changes — [tapered PSTs](#piece-square-tables-and-tapered-eval-milestone-5),
[PVS](#principal-variation-search-milestone-5), and null-move pruning — measured against the
Milestone 4 tag:

| | Opening | Middlegame | Endgame (K+P) | Tactical |
|---|---:|---:|---:|---:|
| Nodes to depth 7, M4 | 370,254 | 2,431,525 | 6,349 | 3,523,284 |
| Nodes to depth 7, M5 | 287,123 | 1,129,624 | 6,437 | 300,109 |
| Δ | −22.5% | **−53.5%** | +1.4% | **−91.5%** |
| Depth in 5 s, M4 → M5 | 10 → **11** | 8 → **10** | 27 → **24** | 8 → **11** |

Plus one more Win At Chess position solved (14/18 → 15/18). The endgame column is the honest cost:
it lost three plies of depth in fixed time across the milestone — two to the evaluation getting
more expensive, one to PVS — and null-move pruning could only give one back, because the guard
correctly refuses to help there. Every other position gained one to three plies.

### Aspiration windows (Milestone 6)

Iterative deepening produces a free estimate of where the next iteration's score will land: one
extra ply rarely overturns a position's assessment. So instead of searching depth *d* with the
maximal window and paying for its width, search it with a narrow window centred on depth *d−1*'s
score — 25 centipawns either side — and pay a re-search only when the guess was wrong.

The window is **exact**. A score that lands inside it is the same score a full-window search would
have returned; a score that reaches a bound proves only "at least this" or "at most this", which
is not an answer, so the window widens and the depth is searched again. A failed guess costs
nodes, never accuracy.

Measured alone, at depth 10 and before LMR landed, it was **a wash** — ±5% on every position,
sometimes negative. That is not a surprise in hindsight: PVS already searches every root move but
the first with a null window, so the only subtree an aspiration window narrows is the first
move's, and the re-searches ate the difference. It only starts paying once the engine is searching
deep enough for the saving to compound, which is exactly what the other half of this milestone
provided. Re-measured at depth 14 with LMR on:

| δ (half-width) | Opening | Middlegame | Endgame | Tactical |
|---|---:|---:|---:|---:|
| 15 | +32.5% | +4.6% | −3.8% | −21.7% |
| **25** (chosen) | **−10.2%** | **+1.0%** | **+1.3%** | **−9.4%** |
| 50 | −4.7% | +1.5% | +5.9% | +42.2% |
| 100 | ~0% | ~0% | 0% | +0.9% |

Too narrow and every iteration fails and re-searches; too wide and the window stops narrowing
anything, converging on the full-window baseline as it must.

**The interesting failure.** The K+P endgame's zugzwang regression test broke when this landed,
and the obvious suspect was LMR — the inexact technique, in the position the project already knows
is fragile. It was not LMR. The aspiration window moved that win from depth 17 to depth 18 *without
being inexact*: rebuilt with the transposition table's score cutoffs disabled, an aspirating build
and a full-window build return the identical score (+262) at depth 17. What a narrow window changes
is what lands in the *table* — bounds where a wide window stored exact scores — and a pawn endgame
is dense with transpositions, so the engine had been reading a real extra ply of effective depth
back out of those exact entries.

That is the same effect [PVS](#principal-variation-search-milestone-5) hit in this same position
one milestone ago, which makes it a general property of every narrowing technique in this engine
rather than a quirk of one. The ply comes back on the clock, which is the only place it matters:
the win is now found in **72 ms at depth 18**, where it used to take **79 ms at depth 17**.

### Late move reductions (Milestone 6)

Every other technique in this engine is exact. LMR is the first one allowed to be wrong, and the
trade is why it's worth it: search the moves the ordering already ranked as unpromising to a
*shallower* depth, and spend what that saves on depth for everything else.

The bet is on the move ordering, not on the moves. By the time the fourth quiet move at a node is
reached, the TT move, every capture, both killers and the highest-history quiets have been searched
and none beat alpha. Such a move is very unlikely to be best — and if the ordering is right, all
the search needs from it is confirmation that it isn't, which a shallow search provides cheaply.
Reduction is `0.75 + ln(depth) · ln(move_index) / 2.25`: deep searches can afford to give up more
plies, late moves deserve to lose more, and a move that is both gets reduced hardest.

When the shallow search is wrong — the move beats alpha — its answer is thrown away and the move
is re-searched at full depth. That keeps the failure mode one-sided: **LMR can miss a good move,
but it can never promote a bad one on shallow evidence.** That asymmetry, not score equality, is
what [`test_research.cpp`](tests/unit/test_research.cpp) pins.

It is the largest single-change node reduction in the project:

| Nodes to depth 7 | Opening | Middlegame | Endgame (K+P) | Tactical |
|---|---:|---:|---:|---:|
| Before | 287,123 | 1,129,624 | 6,437 | 300,109 |
| After | 25,424 | 110,879 | 3,090 | 27,338 |
| Δ | **−91.1%** | **−90.2%** | −52.0% | **−90.9%** |

**Nodes/sec fell 20–51%, and that is fine.** The
[recorded run](bench/results/history/2026-08-15-6cee021.json) shows wall time down 30–65% at
depth 5 alongside a large drop in nodes/sec, which looks alarming until you ask what the remaining
nodes *are*. It is not per-node overhead: rebuilding this same code with both new techniques
switched off reproduces Milestone 5's tree exactly, and the Kiwipete benchmark then runs in the
identical time to the node — so the `in_check` query hoisted to the top of every node, the obvious
suspect, costs nothing measurable. What changed is the node *mix*. LMR deletes whole subtrees,
and subtrees bottom out in quiescence leaves, which are the cheapest nodes in the engine — one
evaluation and a capture scan. Removing them leaves a tree proportionally richer in interior
nodes, which pay for move generation, move scoring, and a TT probe each. The average node costs
more because the cheap ones are gone. Nodes/sec is a per-node cost metric, and this is a change
that deliberately stops visiting nodes, which is why
[REFERENCE.md's metrics catalog](REFERENCE.md) makes nodes-to-depth-N the primary search metric
and treats nodes/sec as a supporting one.

**What it costs, stated plainly.** At a fixed depth 7 the Win At Chess suite fell to 13/18, from
15/18. That is a measurement artifact, not a strength regression — under a *time* limit, which is
how the engine is actually used, the same suite scores 15/18 at any movetime from 200 ms up,
unchanged from Milestone 5. A nominal ply simply means less tree than it used to, so the suite's
depth was raised to 10 (where it again solves everything it did before, in under a second) and its
solve count is no longer compared across milestones. Two of the four pinned depth-5 conclusions
also moved: the endgame's score by two centipawns, and the opening's best move between two moves
it scores identically.

**Four conventional extra guards were implemented, measured, and rejected** — a zugzwang guard
mirroring null-move's, exempting killer moves, reducing less at PV nodes, and searching four moves
at full depth instead of three. Only the last was rejected as harmful (2.5× the nodes on the
opening). The other three were rejected as *unresolvable*: exempting killers, for instance, is
−29% nodes on the opening and +14% on Kiwipete with no change to the tactics suite, and node
counts cannot rank an inexact change that helps one position and hurts another. Settling them
needs games, which — as of this milestone — the project finally has.
([ADR 0006](docs/adr/0006-aspiration-lmr-constants.md) has the full sweeps.)

### Milestone 6 end to end

Both changes, measured against the Milestone 5 tag:

| | Opening | Middlegame | Endgame (K+P) | Tactical |
|---|---:|---:|---:|---:|
| Nodes to depth 7, M5 | 287,123 | 1,129,624 | 6,437 | 300,109 |
| Nodes to depth 7, M6 | 25,424 | 110,879 | 3,090 | 27,338 |
| Δ | **−91.1%** | **−90.2%** | **−52.0%** | **−90.9%** |
| Depth in 5 s, M5 → M6 | 11 → **14** | 10 → **14** | 24 → **27** | 11 → **16** |

Three to five plies on every position — the largest single-milestone movement in the project, and
the first one where the endgame column is not the apology. Both metrics are quoted because LMR is
specifically the kind of change that can trade one for the other; here it won on both.

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

### A note on the numbers before 2026-08-14

The search benchmark had two measurement bugs, fixed on 2026-08-14. They affected the reported
figures, never the engine:

- **`nodes_per_second` was wrong by the iteration count.** The Google Benchmark rate counter was
  fed a single search's node count but divides by the *whole* benchmark's elapsed time, so it
  reported "one search's nodes ÷ ~0.7 s". Because the iteration count varies with how slow the
  position is (269 for the opening, 2,080 for the endgame), three runs of one engine appeared
  200× apart. Fixed by switching to `kIsIterationInvariantRate`, which multiplies by the
  iteration count before dividing by time.
- **Timing included setup.** FEN parsing and allocating + zeroing a 16 MB transposition table sat
  inside the timed loop. For an 820-node endgame search that setup dominated the measurement.
  The table is now allocated once outside the loop and cleared under `PauseTiming`.

Node counts were never affected, and `scripts/compare_bench_results.py` derives nps from nodes
and time rather than reading the recorded field — so the whole history reads correctly without
any file being edited. The pre-fix records are kept as-is; a benchmark history you retouch when
it embarrasses you is not a benchmark history.

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
| 4 | **Move ordering, quiescence, TT** | ✅ Complete — TT, MVV-LVA, killers, history, quiescence ([numbers](#move-ordering-and-quiescence-milestone-4)) |
| 5 | **PVS, piece-square tables / tapered eval, null-move pruning** | ✅ Complete — [tapered PSTs](#piece-square-tables-and-tapered-eval-milestone-5), [PVS](#principal-variation-search-milestone-5), [null-move pruning](#null-move-pruning-milestone-5) ([summary](#milestone-5-end-to-end)) |
| 6 | **Aspiration windows, late move reductions** | ✅ Complete — [aspiration windows](#aspiration-windows-milestone-6), [LMR](#late-move-reductions-milestone-6) ([summary](#milestone-6-end-to-end)) |
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
| [Node counts in CI, timing local and manual](docs/adr/0004-node-counts-in-ci-timing-local.md) | Deterministic and noisy metrics have opposite needs; one mechanism for both forced timing's weaknesses onto node counts, which need no tolerance band at all |
| [Quiescence generates pseudo-legal moves](docs/adr/0005-quiescence-pseudo-legal-movegen.md) | Filtering a *legal* move list pays a make/unmake for ~35 quiet moves to search ~4 captures; halved engine throughput with every test still green |
| [`Move` stays a plain struct](REFERENCE.md) | Bit-packing is deferred until a benchmark shows move-list/TT cache pressure actually matters |
| [Feature branch + PR for every change](REFERENCE.md) | The PR description is where design rationale and before/after numbers live |
| [Aspiration/LMR constants, and four LMR guards rejected](docs/adr/0006-aspiration-lmr-constants.md) | The first constants in the project that nothing derives — three of the four rejected guards were dropped as *unresolvable by node counts*, not as harmful, because an inexact heuristic that helps one position and hurts another needs games to rank |
| [Every inexact technique gets a runtime switch](docs/adr/0006-aspiration-lmr-constants.md) | "Is this technique exact?" can only be asked with the *other* inexact ones held still; `SearchTuning::exact()` turns a hand-rebuilt experiment into a CI test |

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

`release.yml` is the only tag-triggered workflow; performance is measured locally before a tag
rather than on a CI runner ([see benchmarks](#benchmarks)).

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

- **Evaluation is material and piece-square tables, nothing else.** No mobility, no pawn
  structure, no king safety beyond "a castled king scores better than a central one". The three
  Win At Chess positions still recorded as failures all need one of those terms, and searching
  them deeper doesn't help — evaluation, not search, remains the binding constraint on strength.
- **Evaluation is recomputed from scratch at every node.** `evaluate()` walks every piece on the
  board rather than maintaining a running score through `make_move`/`unmake_move`, which costs
  about a third of the engine's time (34.2 ns per call on a full board, against roughly 110 ns
  per node). The incremental version is a known, measurable win with the benchmark for it already
  in place ([`BM_Evaluate_*`](bench/microbench/bench_eval.cpp)) — it hasn't been done yet.
- **The piece-square values are borrowed, not tuned here.** They're PeSTO's published tables. A
  tuner of Dahlia's own is a post-Milestone-7 stretch goal; until it exists, using a tuned set is
  the honest option and inventing numbers would be the dishonest one.
- **Null-move pruning mishandles stalemate.** It runs before the move list is generated, so a
  stalemated side is pruned as though it could pass — which is exactly what it would want to do.
  The zugzwang guard covers the common case (a stalemated side is usually down to a bare king),
  and catching the rest would mean generating the move list, which is the cost the heuristic
  exists to avoid.
- **Quiescence doesn't handle checks.** A node that is *in check* still stands pat as though the
  side to move could decline, and a mate appearing exactly at a quiescence leaf is scored as
  material. Every mate at depth 1 or deeper is still found by the main search; only the
  horizon-leaf case is missed.
- **Move ordering has no SEE.** MVV-LVA ranks captures by what they take, with no notion of
  whether the victim is defended, so QxP-into-a-recapture is searched at the same priority as a
  genuinely winning QxP. That costs nodes, never correctness — quiescence still scores the
  exchange correctly once it searches it.
- **The rating is a first measurement, not a converged one.** 1928 blitz is 64 games. The
  confidence interval on a sample that size is wide, Lichess is still moving the number quickly,
  and it was recorded mid-milestone — the Milestone 6 work is part of what will move it next.
  Quote it with its date and game count or not at all.
- **No SPRT match has been run.** The absolute rating above says what Dahlia is worth; it does
  *not* say whether any individual change helped, because every other variable moves with it. That
  is what SPRT is for, it remains un-run, and two constants in
  [ADR 0006](docs/adr/0006-aspiration-lmr-constants.md) are explicitly parked until it happens. The
  match tooling lives outside this repository.
- **LMR can lose a move the engine would otherwise have found.** It is the one technique here
  allowed to be wrong: a reduced move that fails low is believed without verification. The
  re-search guarantees it cannot *promote* a bad move, and the tactics suite at a fixed time shows
  no measured cost, but "no measured cost on 18 positions" is a weaker claim than soundness and is
  not dressed up as one.
- **Timing regressions depend on remembering to run the script.** Node-count regressions are
  caught automatically on every push, but wall-clock ones surface only when
  `scripts/run_benchmarks.sh` is run. Accepted deliberately: an unreliable automated timing
  signal is worse than a reliable manual one
  ([ADR 0004](docs/adr/0004-node-counts-in-ci-timing-local.md)).
- **Nothing is actually built at `-O3`, including the shipped binary.** The `release` preset and
  `docker/Dockerfile.release` both pass `-O3` via `CMAKE_CXX_FLAGS`, which CMake places *before*
  `CMAKE_CXX_FLAGS_RELWITHDEBINFO` (`-O2 -g -DNDEBUG`) on the command line — and the last `-O`
  flag wins, so every build is effectively `-O2`. The intent is unrealized project-wide rather
  than inconsistent between builds; benchmarks and the release binary do at least agree with
  each other. Fixing it will shift every recorded time at once, so it's a deliberate change to
  make at a tag boundary, not a silent correction.
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
│   ├── eval/               # material + tapered piece-square tables
│   ├── search/             # PVS negamax + iterative deepening, TT, pruning, time budget
│   ├── uci/                # protocol loop, search thread, output serialization
│   └── main.cpp
├── tests/
│   ├── unit/               # Catch2: FEN, make/unmake, eval, search, node counts, TT, UCI
│   └── perft/              # perft + per-piece divide against CPW reference values
├── bench/
│   ├── microbench/         # Google Benchmark: attacks, move generation
│   ├── search_bench/       # fixed-depth whole-engine search on a fixed position set
│   └── results/history/    # committed JSON, one file per benchmark run
├── docs/adr/               # architecture decision records
├── scripts/                # run_benchmarks.sh, compare_bench_results.py, build-release.sh
├── docker/                 # Dockerfile.release — static build + shipped-artifact smoke test
└── .github/workflows/      # ci.yml, release.yml
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
