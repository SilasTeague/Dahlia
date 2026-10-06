# Testing

Chess engines fail in ways that are easy to miss and cheap to catch, so the test
strategy leans hard on oracles rather than on assertions written by the same
person who wrote the bug.

94 Catch2 test cases across 16 unit files, plus a perft suite. Philosophy and
requirements: [REFERENCE.md §1.6](REFERENCE.md#16-testing-philosophy).

```bash
ctest --preset debug --output-on-failure
```

---

## Perft — the movegen correctness oracle

Node counts are checked against the standard
[Chess Programming Wiki reference values](https://www.chessprogramming.org/Perft_Results).
Every depth is *also* checked as a breakdown by which piece type made the root
move, which catches a class of bug the total alone would hide — for example pawn
and knight moves miscounted in offsetting directions.

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

All values match exactly. The deep runs are tagged Catch2-hidden (`[.]`) to stay
inside a CI time budget; run them explicitly with:

```bash
./build/release/tests/perft/dahlia_perft_tests "[perft]"
```

Perft uses the real `make_move`/`unmake_move` rather than a copy-per-node
shortcut, so it exercises the undo path the search depends on.

It is also the backstop for magic bitboards. Sliding attacks feed every legal
move, so one wrong entry among 107,648 moves a perft count — the deep suite
(317 million nodes) matching exactly is a stronger statement about the tables
than any assertion written by hand.

## Unit and regression tests

| File | What it pins |
|---|---|
| `test_position_fen.cpp` | FEN parse, serialize and round-trip, including partial castling rights and en passant |
| `test_magics.cpp` | every one of the 107,648 magic table entries against the ray walker it replaced — see below |
| `test_make_unmake.cpp` | a round-trip property test asserting the position *and* Zobrist key are restored bit-for-bit, over five structurally different positions |
| `test_eval.cpp` | evaluation symmetry under a colour-swapped mirror — the sign-error bug class that is notoriously hard to notice by playing |
| `test_search.cpp` | finds a back-rank mate in one, wins a free rook, declines a poisoned pawn a depth-1 search would grab |
| `test_quiescence.cpp` | stand-pat, delta pruning, termination, and the +700 → +600 horizon-effect regression |
| `test_tt.cpp` | probe/store round-trip, verified-key misses, depth-preferred replacement, generation aging |
| `test_ordering.cpp` | band disjointness, MVV-LVA ranking, killer demotion, history saturation halving |
| `test_nullmove.cpp` | the zugzwang guard, with the guard-less misjudgement written into the assertion |
| `test_repetition.cpp` | the two repetition rules, and the pre-root history the search cannot see for itself |
| `test_research.cpp` | aspiration and LMR against "safe mode" — see below |
| `test_search_nodes.cpp` | the golden node/score/move table — see below |
| `test_clock.cpp` | time budget allocation, `movestogo` tightening, the `Move Overhead` reserve |
| `test_tactics.cpp` | a Win At Chess subset with a documented known-failure list |
| `test_uci.cpp` | scripted protocol sequences over an injected `istream`/`ostream`, including the concurrency cases |

Three of these are worth more detail.

### Magic tables against the code they replaced (`test_magics.cpp`)

Magic bitboards swap a computation for a table, so what has to be proved is that
the table says what the computation said. The ray walker is still in the binary
for exactly this reason, and the test is exhaustive rather than sampled: all
102,400 rook and 5,248 bishop mask subsets — **every occupancy either lookup can
ever be handed** — must produce the ray walker's answer. The count is asserted
too, so a mask that quietly lost a bit fails as the wrong number of checks
rather than as 102,400 easier ones.

Three narrower cases sit beside it, because the exhaustive test only ever passes
subsets of the mask and real callers pass whole boards: a dense board with bits
set on the edges the mask deliberately drops, the empty and full boards, and the
mask/shift invariants themselves. Full reasoning in
[movegen.md](movegen.md#how-it-is-verified).

### Re-search correctness (`test_research.cpp`)

The Milestone 6 techniques are compared against a "safe mode" search **in the
same binary** — `SearchTuning::exact()`, every inexact technique switched off.
That comparison is only meaningful because the safe search is reachable from the
same build as the real one; Milestone 5 had to perform the equivalent check by
rebuilding the engine twice by hand.

Two claims, deliberately different in strength:

- **Aspiration windows must return the *identical* score** a full-window search
  would.
- **LMR must merely never promote a move on reduced-depth evidence** — a mate,
  and a free piece, must both survive the reductions.

Writing the second as an equality test would have been asserting something LMR
does not promise. See [search.md](search.md#late-move-reductions).

### Node counts and conclusions (`test_search_nodes.cpp`)

A golden-file table of nodes, score, and best move at a fixed depth and a fixed
16 MB hash, split into **two test cases** so that a search which got *cheaper*
and a search which changed its *mind* fail separately. Full rationale in
[benchmarking.md](benchmarking.md#node-counts-are-a-test-not-a-benchmark).

### The tactics suite

`test_tactics.cpp` embeds a Win At Chess subset directly rather than reading
`tests/data/*.epd`. A file-backed suite makes the test depend on the working
directory, so it behaves differently under `ctest`, an IDE runner, and a bare
`./dahlia_unit_tests`. Each record keeps its WAC id and its `bm` field
translated to UCI, so every position is still traceable to the published suite.

Three positions are a checked-in known-failure list rather than being omitted.
They need king safety or the bishop pair, which piece-square tables structurally
cannot express, and they fail identically at one second per move as at depth 10
— so they are evaluation failures, not search failures. Since Milestone 6 the
suite runs at depth 10 and its solve *count* is no longer compared across
milestones (LMR makes a nominal ply mean less tree); what it pins is *which*
positions fail.

## CI

Every push and PR runs [`ci.yml`](../.github/workflows/ci.yml):

- **Build + test matrix** — `{ubuntu, macos} × {gcc, clang} × {Debug, Release}`,
  minus the macOS/GCC leg (Apple Clang already provides a second distinct
  compiler there).
- **ASan + UBSan** leg, running the full suite.
- **TSan** leg, running the UCI protocol tests specifically, because they
  exercise the search-thread / reader-thread boundary.

Warnings are errors in CI. The full warning set is in
[`cmake/CompilerWarnings.cmake`](../cmake/CompilerWarnings.cmake).

Wall-clock benchmarking is deliberately absent — see
[benchmarking.md](benchmarking.md#timing-deliberately-isnt-in-ci) and
[ADR 0004](adr/0004-node-counts-in-ci-timing-local.md).
