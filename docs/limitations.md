# Current limitations

Stated plainly, because a portfolio project that only lists strengths isn't an
engineering document. Each of these is known, measured where measurement is
possible, and either scheduled or deliberately accepted.

---

## Evaluation

- **It is material and piece-square tables, nothing else.** No mobility, no pawn
  structure, no king safety beyond "a castled king scores better than a central
  one". The three Win At Chess positions still recorded as failures all need one
  of those terms, and searching them deeper does not help — evaluation, not
  search, remains the binding constraint on strength.
- **It is recomputed from scratch at every node.** `evaluate()` walks every piece
  on the board rather than maintaining a running score through
  `make_move`/`unmake_move`, which costs about a third of the engine's time
  (34.2 ns per call on a full board, against roughly 110 ns per node). The
  incremental version is a known, measurable win with the benchmark for it
  already in place ([`BM_Evaluate_*`](../bench/microbench/bench_eval.cpp)) — it
  has not been done yet.
- **The piece-square values are borrowed, not tuned here.** They are PeSTO's
  published tables. A tuner of Dahlia's own is a post-Milestone-7 stretch goal;
  until it exists, using a tuned set is the honest option and inventing numbers
  would be the dishonest one. See [evaluation.md](evaluation.md#the-tables).

## Search

- **Null-move pruning mishandles stalemate.** It runs before the move list is
  generated, so a stalemated side is pruned as though it could pass — which is
  exactly what it would want to do. The zugzwang guard covers the common case (a
  stalemated side is usually down to a bare king), and catching the rest would
  mean generating the move list, which is the cost the heuristic exists to
  avoid.
- **Quiescence doesn't handle checks.** A node that is *in check* still stands
  pat as though the side to move could decline, and a mate appearing exactly at
  a quiescence leaf is scored as material. Every mate at depth 1 or deeper is
  still found by the main search; only the horizon-leaf case is missed.
- **Move ordering has no SEE.** MVV-LVA ranks captures by what they take, with no
  notion of whether the victim is defended, so QxP-into-a-recapture is searched
  at the same priority as a genuinely winning QxP. That costs nodes, never
  correctness — quiescence still scores the exchange correctly once it searches
  it.
- **LMR can lose a move the engine would otherwise have found.** It is the one
  technique here allowed to be wrong: a reduced move that fails low is believed
  without verification. The re-search guarantees it cannot *promote* a bad move,
  and the tactics suite at a fixed time shows no measured cost, but "no measured
  cost on 18 positions" is a weaker claim than soundness and is not dressed up as
  one.
- **The fifty-move rule and insufficient material are not scored.** Repetition
  is; the other two draws are not.

## Movegen

- **Sliding attacks are a loop/bit-shift ray walk**, not magic bitboards. That is
  deliberate staging rather than an oversight — see
  [roadmap.md](roadmap.md#beyond-milestone-7) — but it is real speed left on the
  table today.

## Measurement

- **Only one SPRT match has been run.** Milestone 6 vs. Milestone 5 is measured;
  nothing before it is. The absolute Lichess rating says what Dahlia is worth,
  but not whether any individual change helped, because every other variable
  moves with it. That is what SPRT is for, and every milestone up to and
  including 5 is still unmeasured individually. The match tooling lives outside
  this repository.
- **The rating is a first measurement, not a converged one.** 2108 blitz over 147
  games, and the sample spans a build change. The confidence interval on a sample
  that size is wide and Lichess is still moving the number quickly. Quote it with
  its date and game count or not at all.
- **Timing regressions depend on remembering to run the script.** Node-count
  regressions are caught automatically on every push, but wall-clock ones surface
  only when `scripts/run_benchmarks.sh` is run. Accepted deliberately: an
  unreliable automated timing signal is worse than a reliable manual one
  ([ADR 0004](adr/0004-node-counts-in-ci-timing-local.md)).
- **Nothing is actually built at `-O3`, including the shipped binary.** The
  `-O3` in the `release` preset is overridden by CMake's per-config flags, so
  every build is effectively `-O2`. The intent is unrealized project-wide rather
  than inconsistent between builds. Full detail and the fix in
  [benchmarking.md](benchmarking.md#known-measurement-issue).

## Protocol

- **`info` lines omit `seldepth`, `pv`, and `hashfull`**, all of which a GUI will
  display if given and silently skip if not.
- **No `go nodes`, `searchmoves`, `ponder`, or `Threads`.** `Threads` waits on
  Lazy-SMP; the rest are unimplemented rather than blocked.
