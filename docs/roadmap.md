# Roadmap

The milestone plan, what each one actually delivered, and what is deliberately
out of scope. Every milestone must leave `master` in a buildable, UCI-playable
state.

The full definition of each milestone — goals, deliverables, benchmarks, tests,
success criteria — is
[REFERENCE.md Part V](REFERENCE.md#part-v--staged-roadmap), which also carries
the dated status notes written as each one closed. This page is the summary.

---

## Status

| # | Milestone | Status |
|---|---|---|
| 0 | **Project scaffolding** — CMake + presets, CI skeleton, Catch2, reference doc | ✅ Complete |
| 1 | **Core types, `Move`, move generation** — attack tables, ray-walk sliding attacks, perft-verified | ✅ Complete |
| 2 | **`Position`, make/unmake, Zobrist, minimal UCI** — legal game playable in a real GUI | ✅ Complete |
| 3 | **Material eval + alpha-beta** — negamax, iterative deepening, time management | ✅ Complete |
| 4 | **Move ordering, quiescence, TT** | ✅ Complete — [numbers](results.md#move-ordering-and-quiescence-milestone-4) |
| 5 | **PVS, piece-square tables / tapered eval, null-move pruning** | ✅ Complete — [summary](results.md#milestone-5-end-to-end) |
| 6 | **Aspiration windows, late move reductions** | ✅ Complete — [summary](results.md#milestone-6-end-to-end) |
| 7 | **Polish & portfolio packaging** — architecture docs, full option set, strength estimate | 🚧 In progress |
| 8+ | **Lazy-SMP search**, opening book, evaluation tuning harness | ⬜ Committed stretch goals |

## Milestone 7 tracking

| Deliverable | Status |
|---|---|
| `docs/architecture.md` with the module graph and data flow | ✅ [architecture.md](architecture.md) |
| README with strength estimate and build instructions | ✅ |
| Complete UCI option set (`Hash`, `Move Overhead`) | ✅ [uci.md](uci.md) |
| ADR log reviewed for completeness | ✅ [`adr/`](adr) |
| Live deployment and release pipeline | ✅ [silasteague.com/chess](https://silasteague.com/chess), [@DahliaBot](https://lichess.org/@/DahliaBot) |
| Historical benchmark chart generated from `bench/results/` | ⬜ `compare_bench_results.py --history` renders the series as text; no chart yet |
| Demo GIF | ⬜ |

## Deviations from the original plan

Two things shipped that the roadmap did not anticipate, both driven by real need
rather than by the plan:

- **Asynchronous search and `stop`** ([ADR 0003](adr/0003-async-search-stop.md)),
  which pulled the ThreadSanitizer CI leg forward from the SMP milestone to
  Milestone 4's timeframe.
- **Static Linux release binaries and live deployment**, which is what makes the
  playable web demo possible.

And one criterion was rewritten rather than quietly dropped. Milestones 4 and 5
both required an "SPRT-confirmed Elo gain", and neither met it — the SPRT rig,
opening book and rating ladder deliberately live in a separate repository, so no
milestone in *this* repository can gate on them. Repeating the criterion a third
time would have been an oversight; Milestone 6 replaced it with a measurement on
both instruments, an SPRT self-play result *and* a rating on the Lichess ladder.
Both have since been recorded ([results.md](results.md#strength)).

## Beyond Milestone 7

Committed stretch goals, explicitly scoped for after a portfolio-ready engine
exists:

- **Lazy-SMP search** — multiple threads sharing one TT with minimal
  synchronization. Real complexity in thread-safety (TT access, node counting,
  `stop` propagation), so it gets its own milestone with its own benchmarks
  (NPS scaling vs. thread count, not just single-thread NPS).
- **Opening book support and an evaluation tuning harness** (`tools/tuner`,
  Texel-style). Mostly integration and data-pipeline work. The tuner is what
  eventually replaces the borrowed PeSTO piece-square values with Dahlia's own,
  measured against them.
- **Magic bitboards** for sliding pieces, replacing the loop/bit-shift ray
  walker. Deferred since Milestone 1 on purpose, not forgotten — the
  microbenchmark that will justify them has been recording a baseline the whole
  time ([results.md](results.md#movegen-baseline)).
- **Incremental evaluation**, updating a running score inside
  `make_move`/`unmake_move` instead of recomputing at every node. Gated on "once
  profiling shows eval cost matters"; it now does, and `BM_Evaluate_*` exists to
  prove the change in or out.

## Explicitly out of scope

Not planned; revisit only if priorities change.

- **NNUE-style learned evaluation** — a large scope jump (data generation,
  training infrastructure). A classical, well-documented, well-tested evaluation
  is itself sufficient for the goal.
- **Syzygy endgame tablebases** — revisit if the engine reaches a strength level
  where it matters.
- **PEXT-based attack lookup** as a BMI2 alternative to magics — cheap enough to
  reconsider later, but not committed.
