# Benchmarking

How Dahlia measures itself, and why timing deliberately isn't in CI.

The numbers themselves live in [results.md](results.md). The full framework
specification is
[REFERENCE.md §3.11](REFERENCE.md#311-benchmark--regression-framework-bench) and
the metrics catalog is
[REFERENCE.md Part IV](REFERENCE.md#part-iv--metrics-catalog--benchmarking).

---

## Two layers

Benchmarking is treated as architecture, not as something bolted on when the
engine feels slow. Two independent layers exist, because they catch different
failures:

- **Microbenchmarks** (`bench/microbench`, Google Benchmark) isolate one hot
  function — sliding-attack lookup, move generation, `evaluate()`.
- **Macrobenchmarks** (`bench/search_bench`) run the whole engine to a fixed
  depth on a fixed position set. This is the only thing that catches a function
  getting faster in isolation while the engine gets slower from cache pressure
  introduced elsewhere.

The macrobenchmark position set is opening (start position), middlegame
(Kiwipete), endgame (K+P), and tactical (WAC.019) — the mix REFERENCE.md §3.11
asks for. Every position a published number refers to is pinned in the
repository; that rule exists because it was once broken, see
[results.md](results.md#a-correction-to-the-tactical-numbers).

## Running them

```bash
scripts/run_benchmarks.sh      # build, run both suites, record a history file, print the deltas
```

One command does the whole loop, so the workflow is: make a change, run the
script, read the numbers, commit the result file with the change. Its real
output for the commit that introduced the transposition table:

```
### 2026-07-31 `acf4aac`  →  2026-08-03 `5dab191`

| Benchmark              | Nodes   | Δ      | Time      | Δ      |      |
|------------------------|---------|--------|-----------|--------|------|
| `BM_Search_Opening`    |  34,195 | -11.2% | 2.631 ms  |  +0.5% | ✅   |
| `BM_Search_Middlegame` | 145,195 | -71.3% | 15.26 ms  | -74.8% | ✅   |
| `BM_Search_Endgame`    |     820 | -16.7% | 0.3449 ms | +167.9%| ✅ ⚠️ |
```

The endgame row is why both columns are shown: fewer nodes, more wall time. To
see the whole arc rather than the last step:

```bash
scripts/compare_bench_results.py --history
scripts/compare_bench_results.py --history --benchmark Middlegame
```

## A history of runs, not a rolling baseline

Every run is recorded as a schema'd JSON file under
[`bench/results/history/`](../bench/results/history), named
`<date>-<short-hash>.json`, and committed alongside the change it measures. The
point is an unedited, plottable record of the engine's whole lifetime, not a
single current number.

Each file records the CPU, compiler version, build type, exact flags, and
repetition count, because a toolchain or flag change is one of the few things
that can move every number at once without a line of Dahlia changing.
`compare_bench_results.py` warns loudly when a baseline and a current run
disagree on any of those — times are then not like-for-like, while node counts
remain valid.

Each benchmark runs multiple repetitions and the **median** is what gets
recorded. Deltas under ±5% are logged but not flagged.

## Timing deliberately isn't in CI

Wall-clock numbers are only comparable within one machine, and a GitHub runner
label pins an image, not a CPU — so automating them would buy an unreliable
signal that trains you to ignore it. Instead the suite runs on one machine and
records its environment in every file.

What *is* automated is the half that can be.

### Node counts are a test, not a benchmark

A fixed-depth search with no time limit visits an *identical* number of nodes on
any machine, compiler, or optimization level — verified by reproducing
34,195 / 145,195 / 820 across Debug and release builds and separate runs days
apart. That makes node count an assertion rather than a measurement, so it lives
in the test suite
([`test_search_nodes.cpp`](../tests/unit/test_search_nodes.cpp)) with the
expected values in a checked-in table, running on every push with no tolerance
band and no statistics.

An intentional search improvement *fails* that test, and the fix is to update
the table in the same PR — so the diff on those values becomes the regression
report, showing exactly what the change did to the tree, reviewable in place.
It is the same pattern as the perft reference values, for the same reason
([ADR 0004](adr/0004-node-counts-in-ci-timing-local.md)).

The table pins the score and best move alongside the node count, in a *separate*
test case, so that a search which got cheaper and a search which changed its
mind fail apart from each other. Since Milestone 6 the engine contains one
technique allowed to do the second — late move reductions — so that half of the
table is no longer expected to hold across every search change. It still makes a
changed conclusion impossible to land by accident, which is what it was for.

#### How the pinned table has moved

The depth-5 counts in `test_search_nodes.cpp` are the same four positions the
macrobenchmark uses, so a count seen there and one in the benchmark history
refer to the same search. Every value that has ever been in that table, and why
it moved:

| Landed | Opening | Middlegame | Endgame | Tactical |
|---|---:|---:|---:|---:|
| Milestone 4 (post-quiescence) | 34,195 | 145,195 | 820 | 458,327¹ |
| \+ tapered PSTs | 23,023 | 161,277 | 1,411 | 151,544 |
| \+ PVS | 26,959 | 193,194 | 1,427 | 159,796 |
| \+ null-move pruning | 26,183 | 152,499 | 1,330 | 156,240 |
| \+ LMR (current) | 4,260 | 25,959 | 742 | 7,510 |

¹ Milestone 3 baseline; the tactical position was pinned at Milestone 5.

The counts include quiescence nodes, which is why two of them went *up* at
Milestone 4 even though ordering cut the main tree sharply — depth 5 now means
five plies plus whatever captures are pending at each leaf.

Only the last row is a change of *conclusion* as well as of cost: the endgame's
score moved by two centipawns (110 → 108) and the opening changed its mind
between two moves it scores identically (g1f3 → d2d4, both 33). LMR is allowed
to do that; everything above it in the table is not, which is why the
conclusions live in their own test case.

Aspiration windows, landing alongside LMR, cost nothing here — at depth 5 they
are below `kAspirationMinDepth` and never engage.

Those counts are specified at a fixed 16 MB hash: node count legitimately falls
as the transposition table grows (164,722 nodes at 1 MB down to 161,277 at
16 MB, flat from there to 256 MB as replacement stops thrashing), so the hash
size is part of the specification. What the test asserts across hash sizes
instead is the **score and best move** — which is also the first thing that would
break if the TT ever returned an entry belonging to a different position.

## Metric of record

**Nodes to reach depth N**, not wall time. Node count is what isolates a
search-quality improvement from the speed of whatever machine happened to run
it. Nodes/sec is a supporting metric and a per-node *cost* metric — a change
that deliberately stops visiting nodes can lower it while being a large win, as
[LMR did](results.md#late-move-reductions-milestone-6).

Strength is a separate question again, and node counts cannot answer it. A
change can raise nodes/sec and lower strength, or search 40% fewer nodes and be
worth nothing; that is what SPRT is for, and the project has now measured a case
of each.

## Known measurement issue

**Nothing is actually built at `-O3`, including the shipped binary.** The
`release` preset and `docker/Dockerfile.release` both pass `-O3` via
`CMAKE_CXX_FLAGS`, which CMake places *before* `CMAKE_CXX_FLAGS_RELWITHDEBINFO`
(`-O2 -g -DNDEBUG`) on the command line — and the last `-O` flag wins, so every
build is effectively `-O2`. Verified by compiling one TU both ways and
byte-comparing the objects.

The intent is unrealized project-wide rather than inconsistent between builds;
benchmarks and the release binary do at least agree with each other, so no
*comparison* is invalidated. Fixing it means setting
`CMAKE_CXX_FLAGS_RELWITHDEBINFO` in `CMakePresets.json`,
`docker/Dockerfile.release` and `scripts/run_benchmarks.sh` together — and it
will shift every recorded time at once, so it lands at a tag boundary and gets
noted in the history rather than slipped in as a cleanup.
