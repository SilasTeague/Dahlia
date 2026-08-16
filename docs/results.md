# Results

Every performance claim Dahlia makes, with the numbers behind it and the
commits they were measured on.

The depth-5 figures come from committed JSON under
[`bench/results/history/`](../bench/results/history). The milestone comparisons
at depth 7 or a fixed five seconds are fixed-depth and fixed-time searches of
the same four pinned positions, run against the named commits. Nothing here is
quoted from a position or a build the repository does not contain — a rule this
project broke once and now [documents](#a-correction-to-the-tactical-numbers).

How the measurements are produced, and why timing is not in CI, is in
[benchmarking.md](benchmarking.md).

---

## Contents

- [Strength](#strength)
- [Milestone 4 — the transposition table](#the-transposition-table-milestone-4)
- [Milestone 4 — move ordering and quiescence](#move-ordering-and-quiescence-milestone-4)
- [Milestone 5 — piece-square tables and tapered eval](#piece-square-tables-and-tapered-eval-milestone-5)
- [Milestone 5 — principal variation search](#principal-variation-search-milestone-5)
- [Milestone 5 — null-move pruning](#null-move-pruning-milestone-5)
- [Milestone 5 end to end](#milestone-5-end-to-end)
- [Milestone 6 — aspiration windows](#aspiration-windows-milestone-6)
- [Milestone 6 — late move reductions](#late-move-reductions-milestone-6)
- [Milestone 6 end to end](#milestone-6-end-to-end)
- [Movegen baseline](#movegen-baseline)
- [A note on the numbers before 2026-08-14](#a-note-on-the-numbers-before-2026-08-14)

---

## Strength

### Absolute — the Lichess ladder

Dahlia plays rated blitz on Lichess as a BOT account,
[**@DahliaBot**](https://lichess.org/@/DahliaBot).

| Date | Rating | Games | Record | Build |
|---|---:|---:|---|---|
| 2026-08-15 | 1977 | 87 | 47 W / 7 D / 33 L | `v2.1` (Milestone 5) |
| 2026-08-16 | **2108** | 147 | 86 W / 16 D / 45 L | `v2.2` (Milestone 6) deployed mid-sample |

This is a rating still in motion over a small sample rather than a converged
one, which is why it is always quoted with its date and game count. The 147
games also span the Milestone 6 deployment, so the ladder figure is not a clean
A/B between the two builds — the controlled comparison is the SPRT match
[below](#milestone-6-end-to-end).

### Relative — SPRT

Milestone 6 vs. Milestone 5, both built from their committed commits with
identical flags, 10+0.1, same book, same seed, same machine:

```
M6 (0d518a8) vs M5 (6cee021),  10+0.1,  163 games
  +88 =52 -23   [0.699]   Elo +147 +/- 46
  SPRT (elo0=0, elo1=10, alpha=beta=0.05): llr 2.95  ->  H1 accepted
```

Self-play inflates the magnitude roughly 1.5–2× — both sides share every blind
spot — so the honest reading is **on the order of +70 to +100 real Elo**, not
+147. The direction and the significance are what transfer.

This is the only SPRT match the project has run against a milestone boundary;
Milestones 4 and 5 are still unmeasured individually. The match rig lives in a
separate repository.

---

## The transposition table (Milestone 4)

Fixed depth 5, same machine (Apple Silicon), same build (`RelWithDebInfo`,
`-O2 -g`), before
([`acf4aac`](../bench/results/history/2026-07-31-acf4aac.json)) and after
([`5dab191`](../bench/results/history/2026-08-03-5dab191.json)):

| Position | Nodes before | Nodes after | Δ nodes | Time before | Time after |
|---|---:|---:|---:|---:|---:|
| Opening (start position) | 38,489 | 34,195 | **−11.2%** | 2.62 ms | 2.63 ms |
| Middlegame (Kiwipete) | 505,339 | 145,195 | **−71.3%** | 60.54 ms | 15.26 ms |
| Endgame (K+P) | 984 | 820 | **−16.7%** | 0.13 ms | 0.34 ms |

The middlegame result is the one that matters: Kiwipete is dense with
transpositions, and cutting the tree by 71% cut wall time by 75%. The opening
position transposes far less at depth 5, so it gains much less — exactly the
shape you would expect, which is itself a sanity check on the result.

The endgame position got *slower* in wall time while searching fewer nodes. That
was never a regression in search — it was the benchmark measuring the wrong
thing: an 820-node search was dominated by allocating and zeroing the 16 MB
table, which the harness then did once per iteration inside the timed region.
That has since been [fixed](#a-note-on-the-numbers-before-2026-08-14), which
drops the endgame's measured time from 0.345 ms to 0.103 ms. The anomaly is left
in the table rather than quietly dropped, because a benchmark suite you only
cite when it agrees with you is not a benchmark suite.

## Move ordering and quiescence (Milestone 4)

Alpha-beta's node count is decided almost entirely by how early the best move
gets searched, so the four Milestone 4 changes were built and measured **one at
a time**. Same machine, same build, **nodes to reach depth 7** — cumulative
across the iterative-deepening iterations, which is the search's real cost:

| After adding | Opening | Middlegame (Kiwipete) | Endgame (K+P) |
|---|---:|---:|---:|
| *(Milestone 3 baseline)* | 828,549 | 4,866,267 | 3,739 |
| MVV-LVA capture ordering | 502,967 | 1,394,445 | 3,754 |
| \+ killer moves | 371,750 | 1,392,651 | 4,028 |
| \+ history heuristic | 345,941 | 1,384,999 | 4,046 |
| \+ quiescence search | 370,254 | 2,431,525 | 6,349 |
| **Net change** | **−55.3%** | **−50.0%** | **+69.8%** |

Reading this honestly matters more than the headline number:

- **MVV-LVA did most of the work.** Ordering captures by what they win, before
  what they risk, cut Kiwipete by 71% on its own. Nothing else in the milestone
  comes close.
- **Killers and history helped where MVV-LVA couldn't.** Both only rank *quiet*
  moves, so their gains land in the opening position (down another 31%), which
  has few captures to order, and are nearly invisible on Kiwipete, where MVV-LVA
  had already found the cutoffs.
- **The K+P endgame got worse at every step, and that's expected.** It has no
  captures to order and almost no quiets worth remembering, so ordering adds
  bookkeeping and returns nothing. A position with nothing to order is where
  ordering costs you.
- **Quiescence *raises* node counts and is still the most valuable change
  here.** It adds a capture-resolving search at every leaf, so "nodes to depth
  7" now buys strictly more than it used to. Judging it by node count alone
  would be measuring the wrong thing.

The metric that shows what quiescence bought is **depth reached in a fixed 5
seconds**:

| Position | Milestone 3 | Milestone 4 | Nodes at M3's depth |
|---|---:|---:|---|
| Opening | 9 | **10** | 15,347,198 → 5,957,927 |
| Middlegame (Kiwipete) | 7 | **8** | 4,866,267 → 2,431,525 |
| Endgame (K+P) | 25 | **27** | — |
| Tactical (WAC.019) | 7 | **8** | 34,941,856 → 3,523,284 |

### A correction to the tactical numbers

Made at Milestone 5. The two tables above originally carried a fourth position
labelled "Tactical" whose FEN was never committed — not to `bench_search.cpp`,
not to the tests, not anywhere in the history. That made it the one performance
claim in the project that could not be reproduced, which is exactly the failure
mode the benchmark history exists to prevent.

The fix was to pin a tactical position for good
([`BM_Search_Tactical`](../bench/search_bench/bench_search.cpp) — WAC.019, the
same position the tactics suite already uses, filling the tactical slot
[REFERENCE.md §3.11](REFERENCE.md#311-benchmark--regression-framework-bench) asks
the position set to cover) and re-measure it against the two commits that
bracket Milestone 4:
[`098d366`](https://github.com/SilasTeague/Dahlia/commit/098d366) and
[`030d518`](https://github.com/SilasTeague/Dahlia/commit/030d518).

| Tactical (WAC.019) | Milestone 3 `098d366` | Milestone 4 `030d518` | Δ |
|---|---:|---:|---:|
| Nodes to depth 7 | 34,941,856 | 3,523,284 | **−89.9%** |
| Nodes to depth 5 | 458,327 | 151,544 | **−66.9%** |
| Score at depth 7 | +370 | +270 | — |

The per-step breakdown cannot be recovered for this position: the four ordering
changes share a single commit, so the intermediate states were measured but
never committed, and only the endpoints can be re-run. The score column is worth
its own line, though — the drop from +370 to +270 is not a regression, it is
quiescence declining a capture sequence the pre-quiescence search scored as
winning material at the horizon.

### The behaviour quiescence fixes, observed

On `4k3/8/2p1p3/3p4/8/8/8/3QK3 w - -`, where the d5 pawn is defended twice, a
depth-1 search before Milestone 4 played **Qxd5 and scored it +700** — it
watched the pawn come off and stopped looking one ply before `cxd5`. The same
search today declines the capture and scores the position +600, its true
material value. That position is checked in as a regression test
([`test_quiescence.cpp`](../tests/unit/test_quiescence.cpp)).

On the [Win At Chess](../tests/unit/test_tactics.cpp) subset checked into the
test suite, the engine went from **13/18 to 14/18** solved at depth 7. The modest
jump is the honest result: the four it still misses are not search failures.
They need an evaluation that understands pawn structure and king safety, and
they fail identically at one second per move as at depth 7 — which made them
Milestone 5's problem. They are checked in as a documented known-failure list
rather than omitted.

One implementation note worth recording, because it cost a 2× slowdown before it
was caught: quiescence generates **pseudo-legal** moves and proves legality using
the `make_move` it already performs for the recursion. Calling
`generate_legal_moves` instead meant paying full legality checks on ~35 quiet
moves in order to search 4 captures, and it dropped middlegame throughput from
5.4M to 2.5M nodes/sec
([ADR 0005](adr/0005-quiescence-pseudo-legal-movegen.md)).

## Piece-square tables and tapered eval (Milestone 5)

Until this change the engine counted material and nothing else, and the tactics
suite had recorded four positions it could not solve for exactly that reason.
The evaluation now scores *where* each piece stands as well as what it is worth,
twice — once for a full board and once for an endgame — and blends the two by how
much material is left. See [evaluation.md](evaluation.md).

The headline result is the one the known-failure list was checked in to make
falsifiable:

| | Milestone 4 | Milestone 5 |
|---|---:|---:|
| [Win At Chess](../tests/unit/test_tactics.cpp) subset solved at depth 7 | 14/18 | **15/18** |

WAC.022 moved from the known-failure list to the solved list. It is a pawn
endgame where `...Nxg4+` wins a pawn and centralizes the king — a material-only
evaluation scored the result level, because everything the move actually gains
was invisible to it. The other three still fail, and the comment above the list
says why: they turn on king safety and the bishop pair, which piece-square tables
do not reach. A table scores where a piece stands, not what it is doing.

**It cost speed, and the numbers are worth stating plainly rather than burying:**

| | Opening | Middlegame | Endgame | Tactical |
|---|---:|---:|---:|---:|
| Nodes to depth 5 | +17.1% | +19.8% | +1.1% | +5.4% |
| Nodes/sec | −32.5% | −25.6% | −11.1% | — |
| Depth reached in 5 s | 10 → **9** | 8 → 8 | 27 → **25** | 8 → 8 |

Two separate effects, and it is worth not conflating them:

- **Node counts went up** because a positional evaluation returns far fewer
  *equal* scores than a material-only one did. Ties are cheap for alpha-beta —
  sibling moves that score identically cut off immediately — and a table that
  distinguishes a knight on d4 from one on a1 stops handing them out.
- **Nodes/sec went down** because `evaluate()` stopped being ten popcounts and
  became a walk over every piece on the board with two table lookups each —
  **34.2 ns** on a full board against **9.1 ns** on a five-piece endgame, now
  tracked as [`BM_Evaluate_*`](../bench/microbench/bench_eval.cpp). At roughly
  110 ns per node, that is about a third of the engine's time spent in
  evaluation, and the endgame's smaller loss lines up exactly with its smaller
  board.

Together those cost a ply of depth in the opening and two in the K+P endgame.
That is a real regression in search depth, accepted deliberately and not
permanently: the fix is incremental evaluation, which
[REFERENCE.md §3.6](REFERENCE.md#36-evaluation-eval) has listed as a future
extension all along, gated on "once profiling shows eval cost matters". It now
does, and the microbenchmark that will justify it exists as of this change.

## Principal Variation Search (Milestone 5)

PVS is a bet on move ordering. So the bet was checked before it was placed —
instrumenting the search to count *where* beta cutoffs happen:

| Position | Cutoffs on the first move tried |
|---|---:|
| Opening | 84.3% |
| Middlegame (Kiwipete) | 95.5% |
| Endgame (K+P) | 92.7% |
| Tactical (WAC.019) | 92.7% |

That is a search whose first guess is right five times in six at worst —
Milestone 4's ordering work is what paid for it. The result, nodes to reach a
fixed depth:

| Position | Depth 5 | Δ | Depth 7 | Δ |
|---|---:|---:|---:|---:|
| Opening | 26,959 → 26,183 | −2.9% | 533,650 → 488,704 | −8.4% |
| Middlegame (Kiwipete) | 193,194 → 152,499 | **−21.1%** | 2,764,317 → 2,393,395 | −13.4% |
| Endgame (K+P) | 1,427 → 1,330 | −6.8% | 7,043 → 6,437 | −8.6% |
| Tactical (WAC.019) | 159,796 → 156,240 | −2.2% | 4,097,425 → 3,821,576 | −6.7% |

**Every score and best move is unchanged**, which is the entire claim PVS makes
— a smaller tree, the same answer — and it is pinned in
[`test_search_nodes.cpp`](../tests/unit/test_search_nodes.cpp) as a table of
scores and moves alongside the node counts, so a future change that quietly
starts searching *differently* rather than *less* fails a test instead of
passing unnoticed.

Verifying that took an experiment, because scores in the K+P endgame did shift
at deep fixed depths (+268 vs +980 at depth 18). Rebuilding both versions with
the transposition table's score cutoffs disabled — leaving it as a move-ordering
hint only — made the scores identical at every depth and position tested. So PVS
is exact, and what moves the numbers is the table: a null-window search stores a
*bound* where a full-window search stored an exact score, and a later probe that
gets a bound where it used to get an exact value has to search rather than
return.

**Which is also why the K+P endgame got worse, and it is the one position that
did:**

| Endgame (K+P), nodes to depth | 15 | 18 | 20 | 22 |
|---|---:|---:|---:|---:|
| Before PVS | 219,339 | 620,528 | 1,178,279 | 2,531,519 |
| After PVS | 278,001 | 655,497 | 1,214,204 | 5,166,104 |

Depth reached in a fixed 5 seconds fell from 25 to 23 there (reproducibly —
three runs, identical node counts). It is not re-search overhead: the
instrumented build puts the re-search rate at **0.01–0.46%** of scouts across
every position and depth measured. It is the TT effect above, landing hardest on
the position that leans on the table most — five pieces, and nearly every line
transposing into every other.

Two fixes were tried and both rejected on the numbers:

- **Skipping the scout near the leaves** (full window at depth ≤ 1, 2, or 3).
  Changed the three normal positions by under 0.5%, and in the endgame swung
  *both* ways with the threshold (depth ≤ 2 halved nodes at depth 22 but cost 7%
  at depth 20; depth ≤ 3 was worse at both). That is one position's noise, not a
  signal, and tuning a constant on it would be overfitting.
- **Preferring exact entries over bounds when replacing a TT slot at equal
  depth.** Zero change, to the node, on all six measurements — the case is rarer
  than the theory suggests.

So textbook PVS ships, with the regression documented rather than tuned away.
The honest summary is that PVS trades a large middlegame gain for a small
endgame loss, which is the same shape as Milestone 4's ordering result and the
same reason: a position with few moves and no captures is where every
ordering-dependent optimization has the least to work with.

## Null-move pruning (Milestone 5)

| Position | Nodes to depth 5 | Δ | Nodes to depth 7 | Δ | Depth in 5 s |
|---|---:|---:|---:|---:|---:|
| Opening | 26,183 → 21,337 | −18.5% | 488,704 → 287,123 | −41.2% | 9 → **11** |
| Middlegame (Kiwipete) | 152,499 → 136,030 | −10.8% | 2,393,395 → 1,129,624 | −52.8% | 8 → **10** |
| Endgame (K+P) | 1,330 → 1,330 | **0.0%** | 6,437 → 6,437 | **0.0%** | 23 → **24** |
| Tactical (WAC.019) | 156,240 → 25,809 | **−83.5%** | 3,821,576 → 300,109 | **−92.1%** | 8 → **11** |

Scores and best moves are unchanged on all four. Two of these rows are worth
reading closely:

**The tactical position lost 92% of its tree** because it is winning by roughly
three pawns, and that is exactly the shape null-move pruning is built for: in a
position this far above beta, most subtrees can be dismissed without being
searched.

**The endgame moved by exactly zero nodes, and that is the feature working**,
not a bug. It is a king and a pawn per side, so the zugzwang guard switches the
heuristic off completely. Rebuilding the engine with the guard removed and
nothing else changed, that same K+P position at depth 17 scores **+219 instead
of +962** and never finds the promotion. That is a 700-centipawn misjudgement of
a won game, pinned as a regression test
([`test_nullmove.cpp`](../tests/unit/test_nullmove.cpp)) with the failing number
written into the comment.

The endgame also gained a ply in fixed time (23 → 24) despite the guard, which
looks contradictory until you notice where: once a pawn promotes, the side to
move *has* a queen, the guard stops applying, and the pruning switches itself
back on for the rest of the line.

## Milestone 5 end to end

Three changes — tapered PSTs, PVS, and null-move pruning — measured against the
Milestone 4 tag:

| | Opening | Middlegame | Endgame (K+P) | Tactical |
|---|---:|---:|---:|---:|
| Nodes to depth 7, M4 | 370,254 | 2,431,525 | 6,349 | 3,523,284 |
| Nodes to depth 7, M5 | 287,123 | 1,129,624 | 6,437 | 300,109 |
| Δ | −22.5% | **−53.5%** | +1.4% | **−91.5%** |
| Depth in 5 s, M4 → M5 | 10 → **11** | 8 → **10** | 27 → **24** | 8 → **11** |

Plus one more Win At Chess position solved (14/18 → 15/18). The endgame column
is the honest cost: it lost three plies of depth in fixed time across the
milestone — two to the evaluation getting more expensive, one to PVS — and
null-move pruning could only give one back, because the guard correctly refuses
to help there. Every other position gained one to three plies.

## Aspiration windows (Milestone 6)

Measured alone, at depth 10 and before LMR landed, aspiration windows were **a
wash** — ±5% on every position, sometimes negative. That is not a surprise in
hindsight: PVS already searches every root move but the first with a null
window, so the only subtree an aspiration window narrows is the first move's,
and the re-searches ate the difference. It only starts paying once the engine is
searching deep enough for the saving to compound, which is exactly what the
other half of this milestone provided.

Re-measured at depth 14 with LMR on:

| δ (half-width) | Opening | Middlegame | Endgame | Tactical |
|---|---:|---:|---:|---:|
| 15 | +32.5% | +4.6% | −3.8% | −21.7% |
| **25** (chosen) | **−10.2%** | **+1.0%** | **+1.3%** | **−9.4%** |
| 50 | −4.7% | +1.5% | +5.9% | +42.2% |
| 100 | ~0% | ~0% | 0% | +0.9% |

Too narrow and every iteration fails and re-searches; too wide and the window
stops narrowing anything, converging on the full-window baseline as it must.

### The interesting failure

The K+P endgame's zugzwang regression test broke when this landed, and the
obvious suspect was LMR — the inexact technique, in the position the project
already knows is fragile. It was not LMR.

The aspiration window moved that win from depth 17 to depth 18 *without being
inexact*: rebuilt with the transposition table's score cutoffs disabled, an
aspirating build and a full-window build return the identical score (+262) at
depth 17. What a narrow window changes is what lands in the *table* — bounds
where a wide window stored exact scores — and a pawn endgame is dense with
transpositions, so the engine had been reading a real extra ply of effective
depth back out of those exact entries.

That is the same effect [PVS](#principal-variation-search-milestone-5) hit in
this same position one milestone ago, which makes it a general property of every
narrowing technique in this engine rather than a quirk of one. The ply comes
back on the clock, which is the only place it matters: the win is now found in
**72 ms at depth 18**, where it used to take **79 ms at depth 17**.

## Late move reductions (Milestone 6)

The largest single-change node reduction in the project:

| Nodes to depth 7 | Opening | Middlegame | Endgame (K+P) | Tactical |
|---|---:|---:|---:|---:|
| Before | 287,123 | 1,129,624 | 6,437 | 300,109 |
| After | 25,424 | 110,879 | 3,090 | 27,338 |
| Δ | **−91.1%** | **−90.2%** | −52.0% | **−90.9%** |

**Nodes/sec fell 20–51%, and that is fine.** The
[recorded run](../bench/results/history/2026-08-15-6cee021.json) shows wall time
down 30–65% at depth 5 alongside a large drop in nodes/sec, which looks alarming
until you ask what the remaining nodes *are*.

It is not per-node overhead: rebuilding this same code with both new techniques
switched off reproduces Milestone 5's tree exactly, and the Kiwipete benchmark
then runs in the identical time to the node — so the `in_check` query hoisted to
the top of every node, the obvious suspect, costs nothing measurable. What
changed is the node *mix*. LMR deletes whole subtrees, and subtrees bottom out
in quiescence leaves, which are the cheapest nodes in the engine — one
evaluation and a capture scan. Removing them leaves a tree proportionally richer
in interior nodes, which pay for move generation, move scoring, and a TT probe
each. The average node costs more because the cheap ones are gone.

Nodes/sec is a per-node cost metric, and this is a change that deliberately
stops visiting nodes — which is why
[REFERENCE.md's metrics catalog](REFERENCE.md#part-iv--metrics-catalog--benchmarking)
makes nodes-to-depth-N the primary search metric and treats nodes/sec as a
supporting one.

**What it costs, stated plainly.** At a fixed depth 7 the Win At Chess suite fell
to 13/18, from 15/18. That is a measurement artifact, not a strength regression —
under a *time* limit, which is how the engine is actually used, the same suite
scores 15/18 at any movetime from 200 ms up, unchanged from Milestone 5. A
nominal ply simply means less tree than it used to, so the suite's depth was
raised to 10 (where it again solves everything it did before, in under a second)
and its solve count is no longer compared across milestones. Two of the four
pinned depth-5 conclusions also moved: the endgame's score by two centipawns, and
the opening's best move between two moves it scores identically.

**Four conventional extra guards were implemented, measured, and rejected** — a
zugzwang guard mirroring null-move's, exempting killer moves, reducing less at PV
nodes, and searching four moves at full depth instead of three. Only the last was
rejected as harmful (2.5× the nodes on the opening). The other three were
rejected as *unresolvable*: exempting killers, for instance, is −29% nodes on the
opening and +14% on Kiwipete with no change to the tactics suite, and node counts
cannot rank an inexact change that helps one position and hurts another.

Two of them were then played out, and **neither survived contact with a real
match.** The aggressive divisor (`1.75`), the standout of the whole sweep at
34–43% fewer nodes for an identical tactics score, scored 49.9% over 480 games.
Exempting killers ran the full 2,000-game cap at **+9 ± 11 Elo** — a confidence
interval straddling zero, and a point estimate landing almost exactly on the +10
threshold the test was asking about, which is precisely why it never resolved.
Both stay rejected, now on evidence rather than for want of it.
([ADR 0006](adr/0006-aspiration-lmr-constants.md) has the full sweeps and the
match records.)

## Milestone 6 end to end

Both changes, measured against the Milestone 5 tag:

| | Opening | Middlegame | Endgame (K+P) | Tactical |
|---|---:|---:|---:|---:|
| Nodes to depth 7, M5 | 287,123 | 1,129,624 | 6,437 | 300,109 |
| Nodes to depth 7, M6 | 25,424 | 110,879 | 3,090 | 27,338 |
| Δ | **−91.1%** | **−90.2%** | **−52.0%** | **−90.9%** |
| Depth in 5 s, M5 → M6 | 11 → **14** | 10 → **14** | 24 → **27** | 11 → **16** |

Three to five plies on every position — the largest single-milestone movement in
the project, and the first one where the endgame column is not the apology. Both
metrics are quoted because LMR is specifically the kind of change that can trade
one for the other; here it won on both.

**And, for the first time in the project, that was checked by playing games
rather than inferred from node counts** — the SPRT result in
[Strength](#relative--sprt) above.

That check mattered more than it looks. The same rig, pointed at an LMR constant
that searched 34–43% *fewer* nodes at fixed depth with an identical tactics
score, measured it at 49.9% over 480 games — worth nothing. Fewer nodes is not
the same claim as better play, and this milestone is the first place in the
project where the two have been told apart by evidence instead of argument.

## Movegen baseline

The loop/bit-shift ray walker, unchanged since Milestone 1 — this is the number a
future magic-bitboard implementation gets measured against:

| Benchmark | 2026-07-26 | 2026-08-03 |
|---|---:|---:|
| `BM_RookAttacksLookup` | 8.67 ns | 8.30 ns |
| `BM_BishopAttacksLookup` | 5.42 ns | 4.93 ns |
| `BM_QueenAttacksLookup` | 14.52 ns | 13.57 ns |
| `BM_GeneratePseudoLegalMoves_StartPosition` | 81.20 ns | 76.28 ns |

No movegen optimization has been attempted yet, so these deltas are run-to-run
drift on identical code — mostly inside the ±5% band the framework treats as
noise. They are listed to establish the baseline, not to claim a win.

## A note on the numbers before 2026-08-14

The search benchmark had two measurement bugs, fixed on 2026-08-14. They
affected the reported figures, never the engine:

- **`nodes_per_second` was wrong by the iteration count.** The Google Benchmark
  rate counter was fed a single search's node count but divides by the *whole*
  benchmark's elapsed time, so it reported "one search's nodes ÷ ~0.7 s".
  Because the iteration count varies with how slow the position is (269 for the
  opening, 2,080 for the endgame), three runs of one engine appeared 200× apart.
  Fixed by switching to `kIsIterationInvariantRate`, which multiplies by the
  iteration count before dividing by time.
- **Timing included setup.** FEN parsing and allocating + zeroing a 16 MB
  transposition table sat inside the timed loop. For an 820-node endgame search
  that setup dominated the measurement. The table is now allocated once outside
  the loop and cleared under `PauseTiming`.

Node counts were never affected, and `scripts/compare_bench_results.py` derives
nps from nodes and time rather than reading the recorded field — so the whole
history reads correctly without any file being edited. The pre-fix records are
kept as-is; a benchmark history you retouch when it embarrasses you is not a
benchmark history.
