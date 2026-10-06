# Dahlia documentation

The engine's front door is the [top-level README](../README.md). Everything
below it lives here.

| Document | What's in it |
|---|---|
| [architecture.md](architecture.md) | module graph, data flow, structural properties, repository layout |
| [movegen.md](movegen.md) | leaper tables, magic bitboards, how the magic numbers are found and checked |
| [search.md](search.md) | ordering, quiescence, TT, PVS, null-move, aspiration windows, LMR, time management |
| [evaluation.md](evaluation.md) | tapered piece-square tables, game phase, orientation, what's missing |
| [uci.md](uci.md) | supported commands and options, time management, concurrency behaviour |
| [testing.md](testing.md) | perft oracle, unit and regression suites, CI matrix |
| [benchmarking.md](benchmarking.md) | the two benchmark layers, the committed history, why timing isn't in CI |
| [results.md](results.md) | every measured number, milestone by milestone, with the commits behind it |
| [releases.md](releases.md) | the tag pipeline, static linking, live deployments |
| [roadmap.md](roadmap.md) | milestone status, Milestone 7 tracking, stretch goals, what's out of scope |
| [limitations.md](limitations.md) | what Dahlia doesn't do, and whether that's scheduled or accepted |
| [REFERENCE.md](REFERENCE.md) | the specification: conventions, subsystem designs, metrics catalog, staged roadmap |
| [`adr/`](adr) | architecture decision records — one file per contested decision |

## Design decisions

Contested decisions are recorded rather than re-litigated. Full ADRs live in
[`adr/`](adr); the complete index, including decisions settled inline, is
[REFERENCE.md Appendix C](REFERENCE.md#appendix-c--architecture-decision-log).

| Decision | Rationale |
|---|---|
| [Pseudo-legal movegen + legality filter](REFERENCE.md#33-move-generation-movegen) | Keeps the hot generation loop simple and benchmarkable in isolation; cost is slightly more complex make/unmake |
| [Magic bitboards deferred past Milestone 1](REFERENCE.md#33-move-generation-movegen) | Correctness → Measurement → Optimization: the loop-based baseline is what makes the later swap a citable win instead of a vibes-based one |
| [Magic numbers searched offline and checked in; the ray walker kept as the oracle](adr/0007-magic-bitboards.md) | A startup search hides 128 correctness-critical values from every diff; the implementation being replaced is the only test oracle that wasn't written by the person who'd have written the bug |
| [CMake as the build system of record](adr/0002-cmake-migration.md) | Library/executable/test/bench target separation, sanitizers, `FetchContent`; the Makefile survives only as a thin wrapper with no logic of its own |
| [Async search; concurrent `go` rejected, not queued](adr/0003-async-search-stop.md) | Queueing means answering "what does a *third* `go` do", for a case no compliant GUI produces |
| [Node counts in CI, timing local and manual](adr/0004-node-counts-in-ci-timing-local.md) | Deterministic and noisy metrics have opposite needs; one mechanism for both forced timing's weaknesses onto node counts, which need no tolerance band at all |
| [Quiescence generates pseudo-legal moves](adr/0005-quiescence-pseudo-legal-movegen.md) | Filtering a *legal* move list pays a make/unmake for ~35 quiet moves to search ~4 captures; halved engine throughput with every test still green |
| [`Move` stays a plain struct](REFERENCE.md#32-move-representation-coremoveh) | Bit-packing is deferred until a benchmark shows move-list/TT cache pressure actually matters |
| [`Position` is a struct; operations are free functions](REFERENCE.md#34-position--state-management-position) | Matches the idiom `core`/`movegen` already established rather than introducing a second one mid-codebase |
| [Repetition history lives outside `Position`](REFERENCE.md#34-position--state-management-position) | `make_move` also runs inside the legality filter and perft, neither of which can repeat anything; the bookkeeping would tax the hottest path for nothing |
| [PeSTO tables, used as published](REFERENCE.md#36-evaluation-eval) | Piece-square values are either fitted or guessed; borrowing a tuned set beats dressing eyeballed numbers up as tuned ones |
| [Aspiration/LMR constants, and four LMR guards rejected](adr/0006-aspiration-lmr-constants.md) | The first constants in the project that nothing derives — three of the four rejected guards were dropped as *unresolvable by node counts*, because an inexact heuristic that helps one position and hurts another needs games to rank |
| [Every inexact technique gets a runtime switch](adr/0006-aspiration-lmr-constants.md) | "Is this technique exact?" can only be asked with the *other* inexact ones held still; `SearchTuning::exact()` turns a hand-rebuilt experiment into a CI test |
| [Feature branch + PR for every change](REFERENCE.md#15-git-conventions) | The PR description is where design rationale and before/after numbers live |
