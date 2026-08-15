# ADR 0006: Aspiration window and late-move-reduction constants

- **Status:** Accepted
- **Date:** 2026-08-15
- **Milestone:** 6
- **Supersedes / superseded by:** none

## Context

Milestone 6 adds two techniques that are governed entirely by numbers chosen by
the implementer:

- **Aspiration windows** search each iterative-deepening iteration with a narrow
  window centred on the previous iteration's score, widening and re-searching
  when the score escapes. The numbers are how wide the first window is, how it
  grows, and the depth below which it is not used at all.
- **Late move reductions (LMR)** search moves that the ordering ranked late to a
  shallower depth, re-searching at full depth if the shallow search says the
  move beats alpha. The numbers are how many moves are exempt, how deep a node
  must be, and how much depth a reduced move gives up.

The milestone requires these constants to be documented with their rationale,
which is what this record is for. It matters more here than elsewhere in the
project because these are the first constants Dahlia has that cannot be derived
from anything: piece values come from material, the null-move R=2 is a
convention with a mechanical justification, but "how wide should the first
window be" has no answer except measurement.

Two facts shape every decision below:

1. **Dahlia cannot run SPRT.** The match rig lives in a separate repository, so
   no constant here can be justified by an Elo delta. What is available is node
   count at fixed depth, depth reached in fixed time, and an 18-position tactics
   suite — the first two deterministic, the third small.
2. **Node count at fixed depth answers a different question for each of the two
   techniques.** Aspiration windows are exact, so fewer nodes at the same depth
   is strictly better and the metric decides the matter. LMR is not exact, so
   fewer nodes at the same depth may only mean a worse search that finishes
   sooner. The metric ranks aspiration constants; for LMR it can only rule
   candidates out.

All measurements below are on the four pinned benchmark positions (opening,
Kiwipete, K+P endgame, WAC.019), `-O2`, 16 MB hash, single-threaded.

## Decision

### Aspiration windows

| Constant | Value | Meaning |
|---|---|---|
| `kAspirationInitialDelta` | 25 | Half-width of the first window, in centipawns |
| `kAspirationMinDepth` | 4 | Below this depth, search the full window |
| widening | `delta += delta / 2` | Geometric growth after each failed window |

**Initial delta = 25.** Measured at depth 14 (nodes, LMR on, against a
full-window baseline of 6,243,772 / 12,731,787 / 72,278 / 4,397,877):

| δ | opening | Kiwipete | endgame | tactical |
|---|---|---|---|---|
| 15 | +32.5% | +4.6% | −3.8% | −21.7% |
| **25** | **−10.2%** | **+1.0%** | **+1.3%** | **−9.4%** |
| 50 | −4.7% | +1.5% | +5.9% | +42.2% |
| 100 | ~0% | ~0% | 0% | +0.9% |

25 is the only value that wins on more than one position without an outlier
against it. The shape is the expected one: too narrow and every iteration fails
and re-searches (δ=15 on the opening), too wide and the window stops narrowing
anything (δ=100 converges on the full-window baseline, as it must).

**Minimum depth = 4.** Below it the iteration costs microseconds and the
previous score is still swinging by hundreds of centipawns per ply, so there is
nothing to save and no estimate worth trusting. `kLmrFullDepthMoves`-style
sensitivity was not swept here; the constant is doing little work either way,
because the first three iterations are a rounding error in every measurement
above.

**Widening geometrically, and one-sided.** A failed window moves only the bound
that failed — a fail-low says nothing new about beta — and the new bound is set
from the returned score rather than from the old bound, which is what fail-soft
buys: the search reports how far past the bound it got, so the next window is
placed around the real answer. Growth by half bounds the number of re-searches
at a handful even when the first guess is badly wrong, where fixed steps would
chase a genuinely moved score outward one re-search at a time.

### Late move reductions

| Constant | Value | Meaning |
|---|---|---|
| reduction formula | `0.75 + ln(depth) · ln(move_index) / 2.25` | Plies given up, truncated to an integer |
| `kLmrMinDepth` | 3 | Below this depth, no reductions |
| `kLmrFullDepthMoves` | 3 | Moves per node searched at full depth first |
| exclusions | captures, promotions, checks, in-check nodes | Never reduced |

**The formula.** Both logarithms matter and multiplying them is what makes the
shape right: deep searches can afford to give up more plies because they have
more to give, late moves deserve to lose more because the ordering is more
confident about them, and a move that is both gets reduced hardest. Measured at
depth 14, varying only the divisor:

| divisor | opening | Kiwipete | endgame | tactical | tactics suite |
|---|---|---|---|---|---|
| 1.75 (aggressive) | −43% | −34% | −4% | −41% | 13/18 |
| **2.25** | **baseline** | **baseline** | **baseline** | **baseline** | **13/18** |
| 3.00 (timid) | +161% | +83% | +15% | +125% | 13/18 |

3.00 is ruled out outright — it costs 1.8–2.6× the nodes for no measured
accuracy gain. Between 2.25 and 1.75 the metric genuinely cannot decide: 1.75 is
40% cheaper per ply and scores identically on the tactics suite, but that suite
is 18 positions and a cheaper ply is not automatically a better one. **2.25 is
chosen as the conventional published value**, on the principle that where
Dahlia's own measurements are silent, the value with the most external evidence
behind it wins — the same reasoning that made Milestone 5 adopt PeSTO's
piece-square tables rather than hand-picked ones. 1.75 is recorded here as the
open question, to be settled by games once a rating signal exists.

**`kLmrFullDepthMoves` = 3.** Raising it to 4 cost 2.5× the nodes on the opening
(14,131,159 vs 5,609,497 at depth 14) for no change to the tactics suite, so 3
is not merely a default: the fourth move is where the ordering's confidence
genuinely starts, and paying full depth for it is expensive.

**`kLmrMinDepth` = 3**, for the mechanical reason that at depth 2 a reduced
search runs at depth 0, which is a bare quiescence — a different question from
the one it stands in for. This is the same argument that sets
`kNullMoveMinDepth`, and the reduction is additionally clamped so a reduced
search never drops below depth 1.

**Exclusions.** Captures and promotions are ordered by what they win rather than
by how good they proved to be — and with no SEE in the ordering, a late capture
does not even mean a losing one. Moves that give check force the reply, so their
subtree is narrow and cheap already. Nodes already in check consist entirely of
forced evasions, so the "late means unpromising" claim the whole heuristic rests
on does not apply to them.

## Alternatives considered and rejected

Four additional LMR guards were implemented and measured. **None is adopted.**

- **Zugzwang guard** (`has_non_pawn_material`, mirroring null-move pruning) —
  rejected. It was tried on the hypothesis that LMR was what broke the K+P
  endgame regression test; it wasn't (see below), and switching reductions off
  in pawn endgames cost a ply of depth there (27 → 26 in five seconds) to fix
  nothing.
- **Not reducing killer moves** — rejected as unresolvable rather than as
  harmful. It is a large effect in both directions: −29% nodes on the opening at
  depth 14, +14% on Kiwipete, no change to the tactics suite. Node counts cannot
  rank an inexact change that helps one position and hurts another; games can.
- **Reducing one ply less at PV nodes** (`beta > alpha + 1`) — rejected. No
  measured effect on the tactics suite and no meaningful node change.
- **`kLmrFullDepthMoves` = 4** — rejected, as measured above.

The first three are the conventional refinements, and all three remain
reasonable things to revisit. They are rejected *for now* on the specific ground
that this milestone has no instrument fine enough to tell an improvement from a
lateral move, and adding conditions to a hot loop on that basis is how a search
accumulates folklore.

## Consequences

**What this buys.** Against the Milestone 5 tag: nodes to depth 7 fell 91.1%
(opening), 90.2% (Kiwipete), 52.0% (K+P) and 90.9% (tactical), and depth reached
in a fixed five seconds rose from 11 to 14, 10 to 14, 24 to 27, and 11 to 16 —
three to five plies on every position, the largest single-milestone movement in
the project.

**What it costs, stated plainly.** LMR is the first heuristic in this engine
allowed to reach a different answer than a plain search, and it does:

- The tactics suite scores 13/18 at a fixed depth 7, down from 15/18. It is
  **not** a strength regression — under a time limit, which is how the engine is
  used, the same suite scores 15/18 at any movetime from 200 ms up, unchanged
  from Milestone 5. The suite's depth was raised to 10 to keep it measuring
  evaluation rather than reduction depth, and its node-count-indexed comparison
  across milestones is retired.
- The pinned depth-5 conclusions moved on two of four positions: the K+P
  endgame's score by two centipawns (110 → 108) and the opening's best move
  between two moves it scores identically (g1f3 → d2d4, both 33).

**A finding that changed the milestone's account of itself.** The K+P zugzwang
regression test broke, and the obvious suspect was LMR. It was not: **aspiration
windows** moved the win from depth 17 to depth 18, and they did so *without
being inexact*. Rebuilt with the transposition table's score cutoffs disabled, an
aspiration build and a full-window build return the identical score (+262) at
depth 17. What a narrow window changes is what lands in the table — bounds where
a wide window stored exact scores — and this position is dense with
transpositions, so the engine had been reading a real extra ply of effective
depth out of those exact entries. This is the same effect Milestone 5 traced in
this same position when PVS landed. The ply comes back on the clock, which is
where it matters: the win is now found in 72 ms at depth 18 where it used to take
79 ms at depth 17.

That diagnosis is why `SearchTuning` exists and why it covers null-move and
delta pruning as well as the two new techniques. Both of those read the current
window before deciding what to skip, so both change their answers when a window
narrows — which means "does the aspiration window change the result?" cannot be
asked at all without switching them off. `SearchTuning::exact()` is that
configuration, and it turns a verification Milestone 5 performed by rebuilding
the engine twice by hand into `tests/unit/test_research.cpp`, which runs in CI.

**Open, and named as such:** the LMR divisor (2.25 vs 1.75) and the killer-move
exemption are both live questions that node counts cannot settle. Both should be
re-measured as games once a rating signal exists — which, as of this milestone,
it does: Dahlia plays rated games on Lichess.
