# Architecture

How Dahlia is put together: the module graph, what each module owns, and the
structural properties that the rest of the code depends on.

For the full specification — conventions, subsystem designs, metrics catalog,
roadmap — see [REFERENCE.md](REFERENCE.md). For why a contested choice went the
way it did, see [`adr/`](adr).

---

## Module graph

`src/` is one directory per module with `.h`/`.cpp` colocated. Dependencies
point in exactly one direction, and this is a rule the code is held to rather
than a diagram of intent:

```
core  ←  movegen  ←  position  ←  search  ←  uci
                         ↑
                       eval
```

| Module | Owns | Deliberately *not* responsible for |
|---|---|---|
| `core/` | `Bitboard`, `Square`, `Piece`, `Color`, `CastlingRights`, `Move` | any chess *rules* logic |
| `movegen/` | leaper attack tables, magic-bitboard sliding attacks, pseudo-legal + legal generation | knowing about search or eval |
| `position/` | board state, FEN in/out, make/unmake, Zobrist hashing, repetition history | scoring or choosing moves |
| `eval/` | material + tapered piece-square tables | anything search-dependent |
| `search/` | negamax alpha-beta with PVS, iterative deepening, TT, move ordering, quiescence, null-move pruning, aspiration windows, LMR, time budget | I/O of any kind |
| `uci/` | protocol loop, search thread ownership, output serialization | being depended on by anything |

`util/` appears in REFERENCE.md's target layout but does not exist yet; nothing
has needed it.

## Data flow

```
stdin ──► uci::run_uci_loop ──► Engine
                                  │
                       position ──┤ parse_fen / make_move  ──► Position
                                  │                            PositionHistory
                            go ───┤
                                  ▼
                        [search thread] search::think
                                  │
                     iterative deepening (depth 1..N)
                                  │
                          search_root  (aspiration window)
                                  │
                             negamax  ──► TT probe/store
                                  │  ──► generate_legal_moves
                                  │  ──► score_moves / select_next_move
                                  ▼
                            quiescence  ──► eval::evaluate
                                  │
                        InfoCallback ──► Engine::write_line ──► stdout
```

Two threads exist: the reader loop and, for the duration of a `go`, one search
thread. Everything either thread writes to stdout passes through a single
mutex.

## Structural properties

**Sliding attacks are a table lookup, and the code they replaced is the test
oracle.** `rook_attacks`/`bishop_attacks` index a magic-bitboard table built at
startup by the ray walker that used to compute them live; `test_magics.cpp` then
re-derives all 107,648 entries through the lookup and requires them to match.
The walker is on no search path and is not dead code — it is what makes the
tables checkable ([ADR 0007](adr/0007-magic-bitboards.md),
[movegen.md](movegen.md)).

**Pseudo-legal generation plus a legality filter**, rather than fully-legal
generation up front. The hot generation loop stays simple and independently
benchmarkable; king safety is confirmed around `make_move`. Quiescence takes
this further and proves legality using the `make_move` it already performs for
the recursion ([ADR 0005](adr/0005-quiescence-pseudo-legal-movegen.md)).

**`Position` is a plain struct and the operations are free functions** —
`parse_fen`, `make_move(pos, m, undo)`, `unmake_move(pos, m, undo)` — matching
the style `core`/`movegen` already established rather than introducing a second
idiom mid-codebase. See [REFERENCE.md §3.4](REFERENCE.md#34-position--state-management-position).

**`make_move`/`unmake_move` restore state exactly**, including the
incrementally-maintained Zobrist key, backed by a round-trip property test
across five structurally different positions.

**Repetition history lives beside `Position`, not inside it.** A `Position`
carries no past, and most positions that matter for a threefold were reached
before the search root. Keeping the key list outside means `make_move` — which
also runs inside the legality filter and inside perft, neither of which can
repeat anything — pays no push/pop bookkeeping on the hottest path in the
engine.

**Search never writes to a stream.** `search::think` takes an `InfoCallback`;
the UCI layer supplies one that funnels every line through a single mutex. That
is what makes concurrent `info` output from the search thread and
`readyok`/`bestmove` from the reader thread safe to interleave without tearing a
line — a hard requirement, since a GUI that receives a torn line hangs.

**Every inexact technique has an off switch.** `search::SearchTuning` gates late
move reductions, null-move pruning, quiescence's delta pruning, aspiration
windows, and the transposition table's score cutoffs; `SearchTuning::exact()`
turns all of them off at once. Nothing in `src/` ever sets them and no UCI
command reaches them — they exist so that "does this technique change the
answer?" is a question the *test suite* can ask. See
[search.md](search.md#searchtuning-and-safe-mode).

**`go` runs on its own thread.** Before this, `stop` was a documented no-op and
`go infinite` was unusable, because the reader loop was blocked for the entire
duration of a search. The search thread observes an `std::atomic<bool>` checked
once per node; a second `go` arriving mid-search is rejected, not queued
([ADR 0003](adr/0003-async-search-stop.md)). This is also the engine's first real
concurrency, so a ThreadSanitizer CI leg landed in the same change.

## Repository layout

```
Dahlia/
├── CMakeLists.txt          # dahlia_core library + dahlia executable + test/bench targets
├── CMakePresets.json       # debug, debug-asan, debug-tsan, release, release-native
├── Makefile                # thin wrapper over the presets, no logic of its own
├── src/
│   ├── core/               # types.h, move.h
│   ├── movegen/            # leaper tables, magic-bitboard sliding attacks, generation
│   ├── position/           # Position, FEN, make/unmake, Zobrist, repetition history
│   ├── eval/               # material + tapered piece-square tables
│   ├── search/             # PVS negamax + iterative deepening, TT, ordering, pruning, clock
│   ├── uci/                # protocol loop, search thread, output serialization
│   └── main.cpp
├── tests/
│   ├── unit/               # Catch2: FEN, make/unmake, eval, search, node counts, TT, UCI
│   └── perft/              # perft + per-piece divide against CPW reference values
├── bench/
│   ├── microbench/         # Google Benchmark: attacks, move generation, evaluation
│   ├── search_bench/       # fixed-depth whole-engine search on a fixed position set
│   └── results/history/    # committed JSON, one file per benchmark run
├── tools/
│   └── magicgen/           # offline search for the checked-in magic multipliers
├── docs/                   # this directory — architecture, subsystems, results, ADRs
├── scripts/                # run_benchmarks.sh, compare_bench_results.py, build-release.sh
├── docker/                 # Dockerfile.release — static build + shipped-artifact smoke test
└── .github/workflows/      # ci.yml, release.yml
```

## Reading order

For a first pass through the codebase, the path with the most signal is:

1. This document, for the module graph.
2. [`adr/`](adr), for the decisions that were actually contested.
3. [search.md](search.md) then [`src/search/search.cpp`](../src/search/search.cpp),
   for where the interesting work happens.
