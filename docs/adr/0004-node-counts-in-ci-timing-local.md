# 0004 — Node counts are a CI test; wall-clock benchmarking is local and manual

## Context

REFERENCE.md 3.11 specified a `benchmark.yml` workflow that would run the full
benchmark suite in CI, compare against the last committed result, flag deltas
outside a ±5% tolerance band, and commit a new history file. It was committed as
an empty placeholder and never implemented.

Implementing it surfaced the reason it was hard. The design treated "benchmark
results" as one kind of thing, when the suite actually produces two kinds with
opposite properties:

- **Node counts** are deterministic. A fixed-depth search with no time limit
  visits an identical number of nodes on any machine, compiler, or optimization
  level — verified by reproducing 34,195 / 145,195 / 820 across a Debug build, a
  RelWithDebInfo build, and separate runs days apart.
- **Wall-clock times** are not comparable across machines at all, and on
  GitHub-hosted runners are not reliably comparable *between runs*: the
  `ubuntu-24.04` label pins an image, not a CPU model.

A single mechanism covering both forced the timing half's weaknesses onto the
node half. Tolerance bands, medians over repetitions, pinned runners, CPU/
compiler/flag fingerprinting, and cross-machine warnings all existed to manage
timing noise — and none of it is needed for an integer that never varies.

## Decision

Split the two by what the number actually is.

**Node counts are a golden-file test** (`tests/unit/test_search_nodes.cpp`),
running in `ci.yml` on every push like any other test. Expected values live in a
checked-in table. There is no tolerance band, no repetition, no timing, and no
workflow: any change is a real change to the search tree.

An intentional search improvement fails this test, and the fix is to update the
table in the same PR — so **the diff on that table is the regression report**,
showing exactly what the change did to the tree, reviewable in place. This is
the same pattern as the perft reference values in `tests/perft/`, which have
worked this way since Milestone 1.

**Wall-clock benchmarking is local and manual.** `scripts/run_benchmarks.sh`
builds, runs both suites, writes a history file under `bench/results/history/`,
and prints the delta against the previous run;
`scripts/compare_bench_results.py --history` shows the whole series. The
developer runs it on one machine and commits the result file alongside the
change it measures.

`benchmark.yml` is deleted rather than reduced. With node counts covered by the
test suite and timing deliberately off CI, it had nothing left to do.

## Consequences

- **Performance regressions in node count are caught earlier than the original
  design would have caught them** — on every push, rather than on whatever
  subset of pushes the trigger matched. The mechanism is also cheaper: roughly
  20 ms added to a test suite that already runs.
- **The timing history stays a single comparable series**, because one machine
  produces all of it. This is the property that makes "nodes/sec over the
  project's lifetime" (1.7, Milestone 7) a real chart rather than a mix of
  laptop and runner numbers with a discontinuity wherever the CI fleet changed.
- **Timing regressions depend on the developer running the script.** This is a
  real gap and is accepted deliberately: an unreliable automated timing signal
  is worse than a reliable manual one, because a check that cries wolf gets
  ignored and then catches nothing. 1.7's before/after discipline already
  requires a PR claiming a performance win to cite numbers.
- **Node counts are specified at a fixed hash size.** Node count legitimately
  falls as the transposition table grows (measured: 146,584 at 1 MB down to
  145,076 at 256 MB, flattening as replacement stops thrashing), so 16 MB is
  part of the specification of the recorded values, not an incidental choice.
  The accompanying test asserts what *is* invariant across hash sizes — the
  score and best move — which is also the property that would break first under
  the key-collision failure mode 3.7 warns about.
- **CI no longer writes to the repository.** The original design had the
  workflow committing history files to `master`, which required `contents:
  write`, a concurrency group, `[skip ci]`, and a carve-out from the
  "every change goes through a PR" rule in 1.5. All of that is gone.
- The `-O3`-that-never-applied bug (see 3.11) is decoupled from this: since no
  automated process compares times across it, fixing it no longer needs to be
  choreographed around a tag boundary.
